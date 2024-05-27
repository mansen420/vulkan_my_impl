#include"volk.h"
#include "GLFW/glfw3.h"
#include "read_file.h"
#include "glm/glm.hpp"

#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <optional>
#include <type_traits>
#include <set>
#include <algorithm>
#include <array>
#include <memory>

#include "vulkan_handle.h"
#include "vulkan_handle_util.h"
#include "ignore.h"
#include "eng_log.h"
/************************************************GLOBALS************************************************/


#define APP_NAME "Vulkan Prototype"

#ifdef NDEBUG   //make sure to use the correct CMAKE_BUILD_TYPE!
    const bool DEBUG_MODE = false;
#else
    const bool DEBUG_MODE = true;
    #include<typeinfo>
#endif

constexpr uint32_t VK_UINT32_MAX = 0xFFFFFFFF;



/************************************************PUBLIC STRUCTS************************************************/


struct window_info
{
    window_info(size_t width, size_t height, const char* title) 
    {
        this->width  =  width;
        this->height = height;
        this->title  =  title;
    }
    window_info()
    {
        width = 800, height = 600;
        title = "UNTITLED";
    }
    size_t width, height;
    const char* title;
};
struct vulkan_communication_instance_init_info
{
    window_info window_parameters;
    const char* app_name;
};

//only call GLFW functions here
class GLFW_window_interface
{
public:
    void init(window_info window_parameters = window_info())
    {
        glfwInit();
        //we assume this is a vulkan application
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        WINDOW_PTR = glfwCreateWindow(window_parameters.width, window_parameters.height, window_parameters.title, nullptr, nullptr);
    }
    
    void terminate()
    {
        glfwDestroyWindow(WINDOW_PTR);
        glfwTerminate();
    }
    static std::vector<const char*> get_glfw_required_extensions()
    {
        uint32_t count;
        const char** data = glfwGetRequiredInstanceExtensions(&count);
        //Apparently pointers can function like iterators: vector(c_array, c_array + size). nice!
        std::vector<const char*> extensions(data, data + count);
        return extensions;
    }
    VkResult init_vk_surface(VkInstance instance, VkSurfaceKHR& surface)
    {
        return glfwCreateWindowSurface(instance, WINDOW_PTR, nullptr, &surface);
    }
    VkExtent2D get_window_extent()
    {
        int width, height;
        glfwGetFramebufferSize(WINDOW_PTR, &width, &height);

        VkExtent2D window_extent;
        window_extent.height = static_cast<uint32_t>(height);
        window_extent.width = static_cast<uint32_t>(width);

        return window_extent;
    }
    bool should_close()
    {
        return glfwWindowShouldClose(WINDOW_PTR);
    }
    void poll_events()
    {
        glfwPollEvents();
    }
    //public now. This class will removed soon
    GLFWwindow* WINDOW_PTR;
};

//queues are a different kind of object from vk_hndl, since they describe themselves to you instead of the other way around
struct queue_desc
{
    VkDevice parent;

    uint32_t family_index;
    uint32_t index_in_family;
};

struct vertex
{
    glm::vec2   pos;
    glm::vec3 color;
};
const std::vector<vertex> TRIANGLE_VERTICES
{
    {{ 0.0f, -0.5f}, {1.f, 0.f, 0.f}},
    {{ 0.5f,  0.5f}, {0.f, 1.f, 0.f}},
    {{-0.5f,  0.5f}, {0.f, 0.f, 1.f}}
};

template <typename vertex_t> VkVertexInputBindingDescription get_per_vertex_binding_description(uint32_t binding)
{
    VkVertexInputBindingDescription description{};

    description.binding = binding;
    description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    description.stride = sizeof(vertex_t);

    return description;
}
template <typename vertex_t> std::vector<VkVertexInputAttributeDescription> get_attrib_description(uint32_t binding);
template<> 
std::vector<VkVertexInputAttributeDescription> get_attrib_description<vertex> (uint32_t binding)
{
    std::vector<VkVertexInputAttributeDescription> descriptions(2);
    descriptions[0].binding  = binding, descriptions[1].binding  = binding;
    descriptions[0].location = 0      , descriptions[1].location = 1;
    descriptions[0].format   = VK_FORMAT_R32G32_SFLOAT      , descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    descriptions[0].offset   = offsetof(vertex, vertex::pos), descriptions[1].offset = offsetof(vertex, vertex::color);
    return descriptions;
}


class window_interface
{
public:
    window_interface() = delete;
    bool should_close()
    {
        return glfw->should_close();
    }
private:
    window_interface(GLFW_window_interface* glfw_ptr){glfw = glfw_ptr;}
    GLFW_window_interface* glfw;
    friend class vulkan_glfw_interface;
};

//only call the vulkan API here
class vulkan_glfw_interface
{
    bool framebuffer_resized = false;
    static void framebuffer_resize_callback(GLFWwindow* window ,int width, int height)
    {
        ignore(width);
        ignore(height);
        auto ptr = reinterpret_cast<vulkan_glfw_interface*>(glfwGetWindowUserPointer(window));
        ptr->framebuffer_resized = true; //we dont need the new width and height since we query them from GLFW anyway
    }
    
    struct command_buffer_data
    {
        VkFramebuffer     framebuffer;
        VkRenderPass       renderpass;
        VkExtent2D        draw_extent;
        VkOffset2D        draw_offset;
        VkClearValue      clear_value;
        VkViewport   dynamic_viewport;
        VkRect2D      dynamic_scissor;
        VkPipeline  graphics_pipeline;
        VkBuffer        vertex_buffer;
    };

public:
    //run this once at the start
    void init(vulkan_communication_instance_init_info init_info)
    {
        if(volkInitialize())
            throw std::runtime_error("Failed to initialize vulkan");

        GLFW_INTERFACE.init(init_info.window_parameters);
        glfwSetWindowUserPointer(GLFW_INTERFACE.WINDOW_PTR, this);
        glfwSetFramebufferSizeCallback(GLFW_INTERFACE.WINDOW_PTR, framebuffer_resize_callback);

        vk_handle::instance instance;
        if(instance.init(get_instance_description(init_info.app_name)))
            throw std::runtime_error("Failed to create instance");

        DESTROY_QUEUE.push_back([=]()mutable{instance.destroy();});

        volkLoadInstance(instance);

        if(DEBUG_MODE)
        {
            vk_handle::debug_messenger debug_messenger;
            if(debug_messenger.init({instance, get_debug_create_info()}))
                throw std::runtime_error("Failed to create debug messenger");
            DESTROY_QUEUE.push_back([=]()mutable{debug_messenger.destroy();});
        }

        vk_handle::surface surface;
        if(surface.init({instance, GLFW_INTERFACE.WINDOW_PTR}))
            throw std::runtime_error("Failed to create surface");

        DESTROY_QUEUE.push_back([=]()mutable{surface.destroy();});

        VkPhysicalDevice phys_device = pick_physical_device(instance.handle, surface.handle);

        vk_handle::device device;
        {
            vk_handle::description::device_desc description;
            description.device_queues      = get_device_queues(find_queue_family(phys_device, surface.handle));
            description.phys_device        = phys_device;
            description.enabled_extensions = ::get_physical_device_required_extension_names();
            if(device.init(description))
                throw std::runtime_error("Failed to create device");
        }

        DESTROY_QUEUE.push_back([=]()mutable{device.destroy();});

        vk_handle::swapchain swapchain;
        {
            vk_handle::description::swapchain_support swp_support = get_swapchain_support(phys_device, surface);
            vk_handle::description::swapchain_features features   = get_swapchain_features(swp_support, GLFW_INTERFACE);
            vk_handle::description::swapchain_desc description;
            description.device_queues = device.get_description().device_queues;
            description.features      = features;
            description.surface       = surface;
            description.parent        = device;
            if(swapchain.init(description))
                throw std::runtime_error("Failed to create swapchain");
        }
        
        auto swapchain_images = get_swapchain_images(swapchain, device);
        std::vector<vk_handle::image_view> swapchain_image_views;
        swapchain_image_views.resize(swapchain_images.size());
        for(size_t i = 0; i < swapchain_images.size(); i++)
        {
            auto& image = swapchain_images[i];

            vk_handle::image_view image_view;

            vk_handle::description::image_view_desc description;

            description.format = swapchain.get_description().features.surface_format.format;
            description.image  = image;
            description.parent = device;

            if(image_view.init(description))
                throw std::runtime_error("Failed to create image view");

            swapchain_image_views[i] = image_view;
        }


        vk_handle::renderpass renderpass;
        if(renderpass.init(get_simple_renderpass_description(swapchain.get_description().features, device)))
            throw std::runtime_error("Failed to create render pass");

        DESTROY_QUEUE.push_back([=]()mutable{renderpass.destroy();});

        std::vector<char> fragment_code, vertex_code;
        read_binary_file({"shaders/"}, "triangle_frag.spv", fragment_code);
        read_binary_file({"shaders/"}, "triangle_vert.spv", vertex_code);

        vk_handle::shader_module fragment_module, vertex_module;
        {
            vk_handle::description::shader_module_desc v_description, f_description;
            f_description.byte_code = fragment_code;
            v_description.byte_code   =   vertex_code;
            v_description.stage = VK_SHADER_STAGE_VERTEX_BIT, f_description.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            v_description.parent = f_description.parent = device;
            if(fragment_module.init(f_description) || vertex_module.init(v_description))
                throw std::runtime_error("Failed to create shader modules");
            DESTROY_QUEUE.push_back([=]()mutable{fragment_module.destroy(); vertex_module.destroy();});
        }

        vk_handle::graphics_pipeline graphics_pipeline;
        {
            vk_handle::description::graphics_pipeline_desc description{};
            description.shader_stages_info = get_shader_stages({vertex_module.get_description(), fragment_module.get_description()},
            {vertex_module, fragment_module});
            
            auto color_blend_attachment = get_color_no_blend_attachment(VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT);
            description.color_blend_info = get_color_no_blend_state_descr({color_blend_attachment});

            description.dynamic_state_info = vk_handle::description::dynamic_state_desc({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
            description.vertex_input_info.binding_descriptions = {get_per_vertex_binding_description<vertex>(0)};
            description.vertex_input_info.attrib_descriptions  = get_attrib_description<vertex>(0);
            description.input_assembly_info = get_input_assemly_state(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);
            description.multisample_info = get_disabled_multisample_info();
            description.rasterization_info = get_simple_rasterization_info(VK_POLYGON_MODE_FILL, 1.f);
            description.viewport_state_info = vk_handle::description::viewport_state_desc({{0.f, 0.f,
            (float)swapchain.get_description().features.extent.width, (float)swapchain.get_description().features.extent.height, 0.f, 1.f}}, 
            {{0, 0, swapchain.get_description().features.extent}});
            description.renderpass = renderpass;
            description.subpass_index = 0;
            description.parent = device;

            vk_handle::pipeline_layout pipeline_layout;
            if(pipeline_layout.init(vk_handle::description::pipeline_layout_desc{device}))
                throw std::runtime_error("Failed to init pipeline layout");

            description.pipeline_layout = pipeline_layout;

            if(graphics_pipeline.init(description))
                throw std::runtime_error("Failed to init graphics pipeline");

            DESTROY_QUEUE.push_back([=]()mutable{graphics_pipeline.destroy(); pipeline_layout.destroy();});
        }

        std::vector<vk_handle::framebuffer> framebuffers;
        framebuffers.reserve(swapchain_image_views.size());
        for(size_t i = 0; i < swapchain_image_views.size(); i++)
        {
            vk_handle::framebuffer framebuffer;

            const auto& image_view = swapchain_image_views[i];

            vk_handle::description::framebuffer_desc description{};

            description.attachments = {image_view};
            description.height = swapchain.get_description().features.extent.height;
            description.width  =  swapchain.get_description().features.extent.width;
            description.renderpass  = renderpass;
            description.parent = device;

            if(framebuffer.init(description))
                throw std::runtime_error("Failed to create framebuffer");

            framebuffers.push_back(framebuffer);
        }

        vk_handle::cmd_pool cmd_pool;
        {
            vk_handle::description::cmd_pool_desc description;
            description.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            description.queue_fam_index = find_queue_family(phys_device, surface).graphics_fam.value().index;
            description.parent = device;
            if (cmd_pool.init(description))
                throw std::runtime_error("Failed to create command pool");    
            DESTROY_QUEUE.push_back([=]()mutable{cmd_pool.destroy();});
        }


        MAX_FRAMES_IN_FLIGHT = 2;

        vk_handle::cmd_buffers cmd_buffers;
        {
            vk_handle::description::cmd_buffers_desc description{};
            description.buffer_count = MAX_FRAMES_IN_FLIGHT;
            description.cmd_pool = cmd_pool;
            description.parent = device;
            if(cmd_buffers.init(description))
                throw std::runtime_error("Failed to allocate command buffers");
            DESTROY_QUEUE.push_back([=]()mutable{cmd_buffers.destroy();});
        }

        std::vector<vk_handle::fence> inflight{};
        std::vector<vk_handle::semaphore> rendering_finished{}, swapchain_image_available{};
        inflight.reserve(MAX_FRAMES_IN_FLIGHT), rendering_finished.reserve(MAX_FRAMES_IN_FLIGHT), swapchain_image_available.reserve(MAX_FRAMES_IN_FLIGHT);
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk_handle::fence fnc;
            fnc.init(vk_handle::description::fence_desc{device});
            inflight.push_back(fnc);

            vk_handle::semaphore s1, s2;
            s1.init(vk_handle::description::semaphore_desc{device}), s2.init(vk_handle::description::semaphore_desc{device});

            DESTROY_QUEUE.push_back([=]()mutable{s1.destroy(); s2.destroy(); fnc.destroy();});

            rendering_finished.push_back(s1);
            swapchain_image_available.push_back(s2);
        }

        vk_handle::buffer vertex_bffr, staging_bffr;
        {
            vk_handle::description::buffer_desc description{};
            description.parent = device;
            description.queue_fam_indices = {find_queue_family(phys_device, surface).graphics_fam.value().index};
            description.size = sizeof(vertex) * TRIANGLE_VERTICES.size();
            description.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            if(vertex_bffr.init(description))
                throw std::runtime_error("Failed to init vertex buffer, kid");

            description.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if(staging_bffr.init(description))
                throw std::runtime_error("Failed to init staging buffer, kid");

            VkMemoryAllocateInfo vertex_bffr_alloc_info = get_mem_alloc_info(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertex_bffr, phys_device);
            
            VkDeviceMemory vertex_buffer_memory;
            if(vkAllocateMemory(device, &vertex_bffr_alloc_info, nullptr, &vertex_buffer_memory))
                throw std::runtime_error("Failed to allocate buffer memory");
            vkBindBufferMemory(device, vertex_bffr, vertex_buffer_memory, 0);
            /*Since this memory is allocated specifically for this the vertex buffer,
            the offset is simply 0. If the offset is non-zero, then it is required to be divisible by mem_reqs.alignment.*/

            VkMemoryAllocateInfo staging_alloc_inco = get_mem_alloc_info(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_bffr, phys_device);
            VkDeviceMemory staging_buffer_memory;
            if(vkAllocateMemory(device, &staging_alloc_inco, nullptr, &staging_buffer_memory))
                throw std::runtime_error("Failed to allocate buffer memory");
            vkBindBufferMemory(device, staging_bffr, staging_buffer_memory, 0);


            DESTROY_QUEUE.push_back([=]()mutable
            {
                vertex_bffr.destroy();
                staging_bffr.destroy();
                vkFreeMemory(device, vertex_buffer_memory, nullptr);
                vkFreeMemory(device, staging_buffer_memory, nullptr);
            });

            void* mem_ptr;
            vkMapMemory(device, staging_buffer_memory, 0, vertex_bffr.get_description().size, 0, &mem_ptr);
            memcpy(mem_ptr, TRIANGLE_VERTICES.data(), vertex_bffr.get_description().size);
            vkUnmapMemory(device, staging_buffer_memory);
        }


        GLOBALS.instance          = instance;
        GLOBALS.device            = device;
        GLOBALS.fam_indices       = find_queue_family(phys_device, surface);
        GLOBALS.cmd_buffers       = cmd_buffers;
        GLOBALS.graphics_pipeline = graphics_pipeline;
        GLOBALS.image_available   = swapchain_image_available;
        GLOBALS.inflight_fences   = inflight;
        GLOBALS.rendering_end     = rendering_finished;
        GLOBALS.renderpass        = renderpass;
        GLOBALS.swapchain         = swapchain;
        GLOBALS.swp_framebuffers  = framebuffers;
        GLOBALS.swp_view          = swapchain_image_views;
        GLOBALS.surface           = surface;
        GLOBALS.vertex_buffer     = vertex_bffr;
        GLOBALS.staging_buffer    = staging_bffr;
        for(const auto& queue : device.get_description().device_queues)
        {
            if(queue.flags & vk_handle::description::PRESENT_BIT)
                vkGetDeviceQueue(device, queue.family_index, 0, &GLOBALS.present_queue);
            if(queue.flags & vk_handle::description::GRAPHICS_BIT)
                vkGetDeviceQueue(device, queue.family_index, 0, &GLOBALS.graphics_queue);
        }
        for(const auto& queue : device.get_description().device_queues)
        {
            if(queue.flags & vk_handle::description::DEDICATED_TRANSFER_BIT)
            {
                vkGetDeviceQueue(device, queue.family_index, 0, &GLOBALS.transfer_queue);
                break;
            }
            if(queue.flags & vk_handle::description::GRAPHICS_BIT)
                vkGetDeviceQueue(device, queue.family_index, queue.count - 1, &GLOBALS.transfer_queue);
        }
        copy_buffer();
    }

    void copy_buffer()
    {
        /*
            why im not using the transfer queue is because we need a whole abstraction for device queues.
            the abstraction would give us 3 queues for present, graphics, and transfer, which may not be independent queues
            under the hood. These queues should also know their family index and own index in that family.
            Generally our abstractions for the lower level data has been okay so far but queues seem to be breaking this rule.
            So, for simplicity I'm using the graphics queue with the same cmd_pool object here. 
        */
        const auto& G = GLOBALS;

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(G.cmd_buffers.handle[0], &begin_info);

        VkBufferCopy copy_region{};
        copy_region.size = G.vertex_buffer.get_description().size;
        copy_region.dstOffset = copy_region.srcOffset = 0;
        vkCmdCopyBuffer(G.cmd_buffers.handle[0], G.staging_buffer, G.vertex_buffer, 1, &copy_region);
        if(vkEndCommandBuffer(G.cmd_buffers.handle[0]))
            throw std::runtime_error("Failed to copy stagin buffer");
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pCommandBuffers = &G.cmd_buffers.handle[0];
        submit_info.commandBufferCount = 1;
        if(vkQueueSubmit(G.graphics_queue, 1, &submit_info, VK_NULL_HANDLE))
            throw std::runtime_error("Failed to submit");

        vkQueueWaitIdle(G.graphics_queue);
    }
    //run this inside the render loop
    void invoke()
    {
        GLFW_INTERFACE.poll_events();
        draw();
    }
    void recreate_swapchain()
    {
        int width, height;
        glfwGetFramebufferSize(GLFW_INTERFACE.WINDOW_PTR, &width, &height);
        while(width == 0 || height == 0)
        {
            glfwGetFramebufferSize(GLFW_INTERFACE.WINDOW_PTR, &width, &height);
        }

        auto& G = GLOBALS;

        vkDeviceWaitIdle(G.device);

        vk_handle::swapchain new_swp;
        vk_handle::description::swapchain_desc desc{};

        vk_handle::description::swapchain_support support = get_swapchain_support(G.device.get_description().phys_device, G.surface);
        vk_handle::description::swapchain_features features = get_swapchain_features(support, GLFW_INTERFACE);

        desc.features = features;
        desc.device_queues = G.device.get_description().device_queues;
        desc.parent = G.device;
        desc.surface = G.surface;
        desc.old_swapchain = G.swapchain;
        auto res = new_swp.init(desc);
        {
            if(res == VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
            {
                return;
            }
            else if(res)
                throw std::runtime_error("failed to renew swapchain, kid");
        }
        auto images = get_swapchain_images(new_swp, G.device);
        std::vector<vk_handle::image_view> img_vs;
        img_vs.reserve(images.size());
        std::vector<vk_handle::framebuffer> framebuffers;
        framebuffers.reserve(images.size());
        for(const auto& image : images)
        {
            vk_handle::description::image_view_desc view_desc{};
            view_desc.format = new_swp.get_description().features.surface_format.format;
            view_desc.image = image;
            view_desc.parent = G.device;

            vk_handle::image_view img_view;
            if(img_view.init(view_desc))
                throw std::runtime_error("Failed to make new image views, kid");

            img_vs.push_back(img_view);

            vk_handle::framebuffer frm_bfr;
            
            vk_handle::description::framebuffer_desc frm_desc{};
            frm_desc.attachments = {img_view};
            frm_desc.height = new_swp.get_description().features.extent.height;
            frm_desc.width = new_swp.get_description().features.extent.width;
            frm_desc.renderpass = G.renderpass;
            frm_desc.parent = G.device;

            if(frm_bfr.init(frm_desc))
                throw std::runtime_error("Failed to make new frambuffer, kid");
            
            framebuffers.push_back(frm_bfr);
        }
        if(G.swapchain)
            G.swapchain.destroy();
        for(auto& x : G.swp_framebuffers)
            x.destroy();
        for(auto& x : G.swp_view)
            x.destroy();
        G.swapchain = new_swp;
        G.swp_framebuffers = framebuffers;
        G.swp_view = img_vs;
    }
    
    //make it have class scope instead of global scope
    uint32_t frame_index = 0;
    void draw()
    {
        auto G = get_frame_data(GLOBALS);   //TODO cache this
        frame_index = (frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
        auto IDX = G.indexed_data[frame_index];

        vkWaitForFences(G.device, 1, &IDX.inflight_fence, VK_TRUE, UINT64_MAX);

//ENG_ERR_LOG << frame_index << '\r';

        uint32_t image_index;
        auto res = vkAcquireNextImageKHR(G.device, G.swapchain, UINT64_MAX, IDX.swapchain_image_available, VK_NULL_HANDLE,
        &image_index);

        if(res != VK_SUCCESS)
        {
            if(res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreate_swapchain();
                G = get_frame_data(GLOBALS);//update frame data
                IDX = G.indexed_data[frame_index];
                framebuffer_resized = false;    //since this would be set, unset it 
            }
            else if(res != VK_SUBOPTIMAL_KHR)
                throw std::runtime_error("Failed to fetch frame");
        }
        if(framebuffer_resized)
        {
            recreate_swapchain();
            framebuffer_resized = false;
            G = get_frame_data(GLOBALS);//update frame data
            IDX = G.indexed_data[frame_index];
            return;                     //reset state
        }

        vkResetFences(G.device, 1, &IDX.inflight_fence);

        vkResetCommandBuffer(IDX.cmdbuffer, 0);

        command_buffer_data data;
        data.clear_value = {{0.f, 0.f, 0.f, 1.f}};
        data.draw_extent = G.swapchain_extent;  //XXX old value 
        data.draw_offset = {0, 0};
        data.dynamic_scissor  = VkRect2D{{0, 0}, G.swapchain_extent};
        data.dynamic_viewport = VkViewport{0.f, 0.f, (float)G.swapchain_extent.width, (float)G.swapchain_extent.height, 0.f, 1.f};
        data.framebuffer = G.framebuffers[image_index];
        data.renderpass  = G.renderpass;
        data.graphics_pipeline = G.graphics_pipeline;
        data.vertex_buffer     = G.vertex_buffer;

        if(record_command_buffer(IDX.cmdbuffer, data))
            throw std::runtime_error("Failed to record");

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1, submit_info.pCommandBuffers = &IDX.cmdbuffer;
        submit_info.waitSemaphoreCount = 1, submit_info.pWaitSemaphores = &IDX.swapchain_image_available;
        auto wait_stage = VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.pWaitDstStageMask = &wait_stage; //this is the stage that needs to wait. Other stages can execute
        submit_info.pSignalSemaphores = &IDX.rendering_finished, submit_info.signalSemaphoreCount = 1;

        if(vkQueueSubmit(G.graphics_queue, 1, &submit_info, IDX.inflight_fence))
            throw std::runtime_error("Failed to submit");


        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1, present_info.pWaitSemaphores = &IDX.rendering_finished;
        present_info.swapchainCount = 1, present_info.pSwapchains = &G.swapchain, present_info.pImageIndices = &image_index;
        
        vkQueuePresentKHR(G.present_queue, &present_info);
    }
    VkResult record_command_buffer(VkCommandBuffer cmd_buffer, command_buffer_data data)
    {
        VkCommandBufferBeginInfo cmdbuffer_info{};
        cmdbuffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        auto err = vkBeginCommandBuffer(cmd_buffer, &cmdbuffer_info);
        if(err)
            return err;
        VkRenderPassBeginInfo renderpass_info{};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.clearValueCount = 1, renderpass_info.pClearValues = &data.clear_value;
        renderpass_info.framebuffer = data.framebuffer;
        renderpass_info.renderArea = VkRect2D{data.draw_offset, data.draw_extent};
        renderpass_info.renderPass = data.renderpass;
        
        vkCmdBeginRenderPass(cmd_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.graphics_pipeline);
        const VkDeviceSize buffer_offset = 0;
        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &data.vertex_buffer, &buffer_offset);
        vkCmdSetViewport(cmd_buffer, 0, 1, &data.dynamic_viewport);
        vkCmdSetScissor(cmd_buffer, 0, 1, &data.dynamic_scissor);
        vkCmdDraw(cmd_buffer, static_cast<uint32_t>(TRIANGLE_VERTICES.size()), 1, 0, 0);
        vkCmdEndRenderPass(cmd_buffer);

        err = vkEndCommandBuffer(cmd_buffer);

        return err;
    }
    
    window_interface get_window_interface()
    {
        window_interface interface(&GLFW_INTERFACE);
        return interface;
    }
    //run this once at the end
    //note that a vulkan_communication_layer object can not be restarted after termination
    void terminate()
    {
        vkDeviceWaitIdle(GLOBALS.device.handle);

        //destroy dynamically generated data
        DESTROY_QUEUE.push_back([=]()
        {
            for(auto& x : GLOBALS.swp_framebuffers)
                x.destroy();
            for(auto& x : GLOBALS.swp_view)
                x.destroy();
            GLOBALS.swapchain.destroy();
        });

        for(auto itr = DESTROY_QUEUE.rbegin(); itr != DESTROY_QUEUE.rend(); itr++)
            (*itr)();

        GLFW_INTERFACE.terminate(); //So terminating GLFW without terminating all vulkan objects will cause the swapchain to segfault! all this headache for this?
    }
private:
    VkMemoryAllocateInfo get_mem_alloc_info(uint32_t memory_type_bitmask, const vk_handle::buffer buffer, VkPhysicalDevice phys_dev)
    {
        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(buffer.get_description().parent, buffer, &mem_reqs);
        info.memoryTypeIndex = get_memory_type_index(memory_type_bitmask, mem_reqs, phys_dev);
        info.allocationSize  = mem_reqs.size;
        info.pNext = nullptr;
        return info;
    }
    //determines index in physical device memory properties for this buffer's requirements with a user-defined bitmask
    uint32_t get_memory_type_index(uint32_t memory_type_bitmask, VkMemoryRequirements mem_reqs, VkPhysicalDevice phys_dev)
    {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_properties);

        //from the spec
        /*memoryTypeBits is a bitmask and contains one bit set for every supported memory type for the resource.
        Bit i is set if and only if the memory type i in the VkPhysicalDeviceMemoryProperties structure for the physical
        device is supported for the resource.*/

        for(size_t i = 0; i < mem_properties.memoryTypeCount; ++i)
        {
            if((mem_reqs.memoryTypeBits & (0b1 << i)) && ((mem_properties.memoryTypes[i].propertyFlags & memory_type_bitmask) 
            == memory_type_bitmask))
                return i;
        }
        throw std::runtime_error("Failed to find memory index for buffer");
    }
    static VkExtent2D get_extent(VkSurfaceCapabilitiesKHR surface_capabilities, GLFW_window_interface glfw_interface)
    {
        if(surface_capabilities.currentExtent.width != VK_UINT32_MAX) 
            return surface_capabilities.currentExtent;
        VkExtent2D window_extent = glfw_interface.get_window_extent();
        window_extent.height = std::clamp(window_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        window_extent.width  = std::clamp(window_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);

        return window_extent;
    }    
    static vk_handle::description::swapchain_features get_swapchain_features(vk_handle::description::swapchain_support swp_support, GLFW_window_interface glfw_interface)
    {
        VkSurfaceFormatKHR surface_format = swp_support.surface_formats[0];
        for(const auto& surface_format_candidate : swp_support.surface_formats)
            if(surface_format_candidate.format == VK_FORMAT_B8G8R8_SRGB && surface_format_candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                surface_format = surface_format_candidate;
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(const auto& present_mode_candidate : swp_support.surface_present_modes)
            if(present_mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR)
                present_mode = present_mode_candidate;
        
        VkExtent2D extent = get_extent(swp_support.surface_capabilities, glfw_interface);

        return {surface_format, present_mode, extent, swp_support.surface_capabilities};
    }
    static vk_handle::description::extension_info get_instance_required_extension_names()
    {
        const bool& ENABLE_VALIDATION_LAYERS = DEBUG_MODE;

        std::vector<const char*> required_extension_names;
        std::vector<const char*>     required_layer_names;

        required_extension_names = GLFW_window_interface::get_glfw_required_extensions();
//HACK hardcoding 
        if(ENABLE_VALIDATION_LAYERS)
        {
            required_layer_names.push_back("VK_LAYER_KHRONOS_validation");
            required_extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        vk_handle::description::extension_info info{};
        info.extensions = required_extension_names;
        info.layers     =     required_layer_names;

        return info;
    }
    static vk_handle::description::instance_desc get_instance_description(const char* app_name)
    {
        vk_handle::description::instance_desc description{};
        if(DEBUG_MODE)
            description.debug_messenger_ext = get_debug_create_info();
        
        //check extension support
        const auto ext_info = get_instance_required_extension_names();
        //first, check for extension and layer support
        uint32_t instance_property_count;
        vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
        nullptr);
        std::vector<VkExtensionProperties> instance_properties(instance_property_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
        instance_properties.data());

        std::vector<const char*> instance_extension_names(instance_property_count);
        for(size_t i = 0; i < instance_property_count; i++)
            instance_extension_names[i] = instance_properties[i].extensionName;

        if(!check_support((size_t) instance_property_count, instance_extension_names.data(),
        ext_info.extensions.data(), ext_info.extensions.size()))
            throw std::runtime_error("Failed to find required instance extensions");
        
        uint32_t instance_layer_count;
        vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
        std::vector<VkLayerProperties> instance_layer_properties(instance_layer_count);
        vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties.data());

        std::vector<const char*> instance_layer_names(instance_layer_count);
        for(size_t i = 0; i < instance_layer_count; i++)
            instance_layer_names[i] = instance_layer_properties[i].layerName;

        if(!check_support((size_t) instance_layer_count, instance_layer_names.data(),
        ext_info.layers.data(), ext_info.layers.size()))
            throw std::runtime_error("Failed to find required instance layers");

        description.ext_info = ext_info;
        description.app_info = get_app_info(app_name);
        
        return description;
    }
    static VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface)
    {
        const auto phys_devices = get_physical_devices(instance);

        std::vector<VkPhysicalDevice> candidates;

        size_t i = 1;
        for(const auto& device : phys_devices)
        {
            ENG_LOG << "Physical device "<< i++ <<" check : \n";
            if(is_adequate(device, surface))
                candidates.push_back(device);
        }
        if(candidates.empty())
            throw std::runtime_error("Failed to find adequate physical device.");
//TODO pick the best-performing adequate physical device
        auto phys_device = candidates[0];

        return phys_device;
    }
    
    GLFW_window_interface GLFW_INTERFACE;

    uint32_t MAX_FRAMES_IN_FLIGHT = 1;


    struct indexed_frame_data
    {
        VkFence                  inflight_fence;
        VkSemaphore   swapchain_image_available;
        VkSemaphore          rendering_finished;
        VkCommandBuffer               cmdbuffer;
    };
    struct frame_data
    {
        VkBuffer                       vertex_buffer;
        VkDevice                              device;
        VkSwapchainKHR                     swapchain;
        VkQueue                       graphics_queue;
        VkQueue                        present_queue;
        VkExtent2D                  swapchain_extent;
        VkRenderPass                      renderpass;
        VkPipeline                 graphics_pipeline;
        std::vector<indexed_frame_data> indexed_data;
        std::vector<VkFramebuffer>      framebuffers;
    };

    struct globals
    {
        VkQueue                       graphics_queue;
        VkQueue                       transfer_queue;
        VkQueue                        present_queue;
        vk_handle::buffer                      vertex_buffer;
        vk_handle::buffer                     staging_buffer;
        vk_handle::surface                           surface;
        vk_handle::instance                         instance;
        vk_handle::device                             device;
        vk_handle::swapchain                       swapchain;
        std::vector<vk_handle::framebuffer> swp_framebuffers;
        std::vector<vk_handle::image_view>          swp_view;
        vk_handle::renderpass                     renderpass;
        vk_handle::graphics_pipeline       graphics_pipeline;
        vk_handle::description::queue_families                   fam_indices;
        vk_handle::cmd_buffers                   cmd_buffers;
        std::vector<vk_handle::semaphore>    image_available;
        std::vector<vk_handle::semaphore>      rendering_end;
        std::vector<vk_handle::fence>        inflight_fences;
    };
    globals GLOBALS;

    frame_data get_frame_data(globals& G)
    {
        frame_data data{};
        data.device = G.device;
        data.swapchain = G.swapchain;
        //TODO move this logic to devie_queue 
        for(const auto& queue : G.device.get_description().device_queues)
        {
            if(queue.flags & vk_handle::description::GRAPHICS_BIT)
                vkGetDeviceQueue(G.device, queue.family_index, 0, &data.graphics_queue);
            if(queue.flags & vk_handle::description::PRESENT_BIT)
                vkGetDeviceQueue(G.device, queue.family_index, 0, &data.present_queue);
        }
        data.swapchain_extent = G.swapchain.get_description().features.extent;
        data.renderpass = G.renderpass;
        data.graphics_pipeline = G.graphics_pipeline.handle[0];
        data.framebuffers.resize(G.swp_framebuffers.size());
        for(size_t i = 0; i < G.swp_framebuffers.size(); i++)
            data.framebuffers[i] = G.swp_framebuffers[i];
        data.indexed_data.resize(MAX_FRAMES_IN_FLIGHT);
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            data.indexed_data[i].cmdbuffer = G.cmd_buffers.handle[i];
            data.indexed_data[i].inflight_fence = G.inflight_fences[i];
            data.indexed_data[i].rendering_finished = G.rendering_end[i];
            data.indexed_data[i].swapchain_image_available = G.image_available[i];
        }
        data.vertex_buffer = G.vertex_buffer;
        return data;
    }

    std::vector<std::function<void()>> DESTROY_QUEUE;
};

/************************************************APPLICATION************************************************/


int main()
{
    vulkan_glfw_interface instance;
    vulkan_communication_instance_init_info init_info{{800, 600, "Vulkan Prototype"}, "Vulkan Prototype"};

    try
    {
        instance.init(init_info);
    } 
    catch(const std::exception& e)
    {
        ENG_ERR_LOG << e.what() << std::endl;
        return -1;
    }
    
    window_interface window = instance.get_window_interface();
    while(!window.should_close())
    {
        instance.invoke();       
    }

    instance.terminate();

    return 0;    
}