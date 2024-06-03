#include "volk.h"
#include "GLFW/glfw3.h" //for surface

#include "vulkan_handle.h"
#include "vulkan_handle_make_shared.h"
#include "vulkan_data_getters.h"
#include "debug.h"
#include "read_file.h"

#include <map>
#include <algorithm>


typedef unsigned int uint; //MSVC can't handle the power of pure uint

//this module handles all communication with vulkan


            /***********************************helper interface***********************************/ 




vk_handle::instance* VULKAN;

std::vector<VkPhysicalDevice> PHYSICAL_DEVICES;

struct destruction_queue
{
    void reserve_extra(size_t functions)
    {
        queue.reserve(queue.size() + functions);
    }
    void push(std::function<void()> statement)
    {
        queue.push_back(statement);
    }
    void flush()
    {
        //call destruction functions in LIFO order 
        for(auto itr = queue.rbegin(); itr != queue.rend(); itr++)
            (*itr)();
    }
private:
    std::vector<std::function<void()>> queue;
};
    
            /***************************************GLOBALS***************************************/

destruction_queue TERMINATION_QUEUE;

static bool INIT = false;

namespace vk   = vk_handle;
namespace get  = vk_handle::data_getters;
namespace data = vk_handle::description;

        /***************************************PROCEDURES***************************************/

//TODO listen bud, you can do this. You're gonna run down every function in this file, and I mean EVERY FUNCTION, and 
//factor out whatever you can. 'k, bud?
//Also, definitely use Vulkan.hpp next time...

//Note :  on Unified devices (integrated GPU) you can and should simply access GPU memory directly, since all device-local
//memory is host-visible. Thus, staging is not necessary.

//Aesthetic Interactive Computing Engine
//愛子ーアイコ
namespace AiCo
{
    //client code
    class Instance
    {
        //public info
    private:
        struct instance_impl;
        instance_impl* impl;
    };

    //server side 
    struct Instance::instance_impl
    {
    };
};



/*
    This class initializes 3rd party dependencies as well as the vulkan instance,
    finds all physical devices in the system, and enables the vulkan validation layers in debug mode.

    Since all engine objects rely on these resources, they must be destroyed last.
    It is important that no engine objects are created before calling start(), and none after this class is destroyed.

    It is safe to copy this class; Only the last copy will actually destroy the context.
    It is also safe to call start() multiple times.
    There is, however, only one global context at any time.
*/
class vulkan_context
{
    //terminates 3rd party libraries and all Vulkan objects
    static void terminate()
{
    if(!INIT)
        INFORM_ERR("WARNING : calling terminate() without successful init()");
    INFORM("Terminating context...");
    TERMINATION_QUEUE.flush();
    INIT = false;
}

    //Initializes all third party dependencies, as well as the Vulkan instanc and debugger, and determines physical devices.
    //In case of failure, returns false, and, if throws is set, throws runtime error.
    static bool init(bool throws = true)
    {
        if(INIT)
            return true;

        //this is vulkan baby
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        EXIT_IF(glfwInit() == GLFW_FALSE, "GLFW INIT FAILED", terminate);

        TERMINATION_QUEUE.push(DO(glfwTerminate();));
        
        EXIT_IF(volkInitialize(), "VOLK INIT FAILED", terminate);

        TERMINATION_QUEUE.push(DO(volkFinalize();));

        data::instance_desc description{};
        try
        {
            uint debug_flag = 0;
            if(DEBUG_MODE)
                debug_flag = get::instance::DEBUG;
            description = get::instance::get_instance_description(get::instance::GLFW | debug_flag, debug_flag);
        }
        catch(const std::exception& e)
        {
            EXIT_IF(true, e.what(), terminate);
        }
        VULKAN  = new vk::instance(description);
        //EXIT_IF(VULKAN.init(description), "VULKAN INSTANTIATION FAILED", terminate);
        
        TERMINATION_QUEUE.push(DO(delete VULKAN;));

        volkLoadInstance(*VULKAN);

        if(DEBUG_MODE)
        {
            vk::debug_messenger* lifetime_extension = new vk::debug_messenger(data::debug_messenger_desc{*VULKAN, get_debug_create_info()});
            TERMINATION_QUEUE.push(DO(delete lifetime_extension;));
        }

        //check for swapchain support by default
        auto candidates = get::physical_device::find_physical_devices(*VULKAN);
        for(const auto& candidate : candidates)
            if(get::physical_device::supports_extensions(candidate, get::physical_device::get_required_extension_names(get::physical_device::SWAPCHAIN)))
                PHYSICAL_DEVICES.push_back(candidate);

        EXIT_IF(PHYSICAL_DEVICES.size() == 0, "FAILED TO FIND PHYSICAL DEVICE", terminate);

        INIT = true;

    std::vector<std::string> greetings;
    greetings.push_back("Vulkan speaking, yes?");
    greetings.push_back("This is vulkan, baby!");
    uint num = rand()%greetings.size();
    INFORM(greetings[num]);

        return true;
    }

    static unsigned int copies;
public:
    vulkan_context() {vulkan_context::copies++;}
    bool start(bool throws = true){return init(throws);}
    ~vulkan_context()
    {
        if(copies == 1)
            terminate();
        vulkan_context::copies--;
    }
};
uint vulkan_context::copies = 0;

struct render_data_t
{
    vk::shader_module              fragment_shader;
    vk::shader_module                vertex_shader;
    vk::pipeline_layout            pipeline_layout;
    vk::graphics_pipeline          graphics_pipeline;

    vk::cmd_pool                   command_pool;
    vk::cmd_buffers                command_buffers;

    VkQueue                        submit_queue;

    render_data_t(const vk::device& device, VkRenderPass renderpass, uint concurrent_cmd_buffers) : 
    fragment_shader(get_shader_desc("triangle_no_input_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, device)),
    vertex_shader(get_shader_desc("triangle_no_input_vert.spv", VK_SHADER_STAGE_VERTEX_BIT, device)),
    pipeline_layout(data::pipeline_layout_desc{device}),
    graphics_pipeline({get_pipeline_desc(renderpass, pipeline_layout, {vertex_shader, fragment_shader}, device)}),
    command_pool(data::cmd_pool_desc{device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, device.description.graphics_queue.fam_idx}),
    command_buffers(data::cmd_buffers_desc{device, command_pool, concurrent_cmd_buffers, VK_COMMAND_BUFFER_LEVEL_PRIMARY})
    {
        submit_queue = get::device::queue_handle(device, device.description.graphics_queue);
    }
    
    private:
    static data::shader_module_desc get_shader_desc(const char* filename, VkShaderStageFlagBits stage_bits, const vk::device& device)
    {
        //these need to stay alive until the graphics pipeline is created!
        data::shader_module_desc description{};
        std::vector<char> byte_code;
        //"triangle_no_input_frag.spv"
        read_binary_file({"shaders/"}, filename, byte_code);
        description.byte_code = byte_code;
        description.entry_point_name = "main";
        description.parent = device;
        description.stage = stage_bits;
        return description;
    }
    
    static data::graphics_pipeline_desc get_pipeline_desc(VkRenderPass renderpass, VkPipelineLayout layout, 
    const std::vector<std::reference_wrapper<vk::shader_module>> shaders, VkDevice device)
    {
        data::graphics_pipeline_desc triangle_pipeline_d{};
        {
            triangle_pipeline_d.color_blend_info.logic_op_enabled = VK_FALSE;
            VkPipelineColorBlendAttachmentState color_attachment{};
            color_attachment.blendEnable = VK_FALSE;
            color_attachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT  | VK_COLOR_COMPONENT_B_BIT;
            triangle_pipeline_d.color_blend_info.attachment_states.push_back(color_attachment);

            triangle_pipeline_d.dynamic_state_info.dynamic_state_list = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            
            triangle_pipeline_d.input_assembly_info.primitive_restart_enabled = VK_FALSE;
            triangle_pipeline_d.input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            
            triangle_pipeline_d.multisample_info.rasterization_samples = VK_SAMPLE_COUNT_1_BIT;
            triangle_pipeline_d.multisample_info.sample_shading_enable = VK_FALSE;

            triangle_pipeline_d.renderpass = renderpass;
            triangle_pipeline_d.subpass_index = 0;
            triangle_pipeline_d.pipeline_cache = VK_NULL_HANDLE;
            triangle_pipeline_d.rasterization_info.polygon_mode = VK_POLYGON_MODE_FILL;
            triangle_pipeline_d.rasterization_info.rasterization_discard = VK_FALSE;
            triangle_pipeline_d.rasterization_info.depth_bias_enable = VK_FALSE;
            triangle_pipeline_d.rasterization_info.depth_clamp_enable = VK_FALSE;
            triangle_pipeline_d.rasterization_info.front_face = VK_FRONT_FACE_CLOCKWISE;
            triangle_pipeline_d.rasterization_info.cull_mode = VK_CULL_MODE_NONE;


            triangle_pipeline_d.shader_stages_info.resize(shaders.size());
            for(size_t i = 0; i < shaders.size(); ++i)
            {
                triangle_pipeline_d.shader_stages_info[i].entry_point = shaders[i].get().description.entry_point_name;
                triangle_pipeline_d.shader_stages_info[i].module = shaders[i].get();
                triangle_pipeline_d.shader_stages_info[i].stage = shaders[i].get().description.stage;
            }

            triangle_pipeline_d.viewport_state_info.scissors.resize(1);
            triangle_pipeline_d.viewport_state_info.viewports.resize(1);

            triangle_pipeline_d.vertex_input_info.attrib_descriptions.resize(0);
            triangle_pipeline_d.vertex_input_info.binding_descriptions.resize(0);

            triangle_pipeline_d.parent = device;
            triangle_pipeline_d.pipeline_layout = layout;
        }
        return triangle_pipeline_d;
    }

};
typedef std::function<bool (VkSemaphore, VkFence, const VkSemaphore, const VkRenderPassBeginInfo, uint , const render_data_t&, const bool)> 
frame_render_callback_fnc;

bool render_triangles(VkSemaphore signal_semaphore, VkFence signal_fence, const VkSemaphore image_available, 
const VkRenderPassBeginInfo renderpass_binfo, uint frame_index, const render_data_t& render_data, const bool throws = true)
{
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    const auto& cmd_buffer = render_data.command_buffers.handle[frame_index];

    EXIT_IF(vkBeginCommandBuffer(cmd_buffer, &begin_info), "FAILED TO BEGIN CMD BUFFER", DO_NOTHING);

    vkCmdBeginRenderPass(cmd_buffer, &renderpass_binfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_data.graphics_pipeline.handle[0]);
    
    VkViewport viewport{(float)renderpass_binfo.renderArea.offset.x, (float)renderpass_binfo.renderArea.offset.y,
    (float)renderpass_binfo.renderArea.extent.width, (float)renderpass_binfo.renderArea.extent.height, 0.0, 1.0};
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor{renderpass_binfo.renderArea};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buffer);

    EXIT_IF(vkEndCommandBuffer(cmd_buffer), "FAILED TO END CMD BUFFER", DO_NOTHING);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1, submit_info.pCommandBuffers = &cmd_buffer;
    
    VkSemaphore wait_s = image_available;
    submit_info.waitSemaphoreCount = 1, submit_info.pWaitSemaphores = &wait_s;
    VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &wait_stage_mask;

    VkSemaphore submit_s = signal_semaphore;
    submit_info.signalSemaphoreCount = 1, submit_info.pSignalSemaphores = &submit_s;

    EXIT_IF(vkQueueSubmit(render_data.submit_queue, 1, &submit_info, signal_fence), "FAILED TO SUBMIT CMD BUFFER", DO_NOTHING);
    
    return true;
}

class frame
{
private:
    class window_t
    {
    public:
        vk::shared_device        owner;

        GLFWwindow* window_ptr;

        window_t& operator =(const window_t&) = delete;
        window_t(const window_t&) = delete;

        struct description
        {
            int width, height;
            const char* title;
        };
        
        window_t(description description, VkInstance instance, vk::shared_device owner) : 
        window_ptr(glfwCreateWindow(description.width, description.height, description.title, nullptr, nullptr)),
        surface(data::surface_desc{instance, window_ptr}), 
        swapchain(new vk::swapchain(get::swapchain::description(surface, *owner)))
        {
            this->owner = owner;
            
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            update_swapchain_imageviews();
        }

        vk_handle::description::surface_features get_features() const {return swapchain->description.features;}

        VkExtent2D get_framebuffer_size()
        {
            int height, width;
            glfwGetFramebufferSize(window_ptr, &width, &height);
            return VkExtent2D{static_cast<uint>(width), static_cast<uint>(height)};
        }
        static std::vector<vk::shared_image_view> get_window_image_views(window_t& window)
        {
            std::vector<vk::shared_image_view> views;
            views.reserve(window.swapchain_image_views.size());
            for(auto& i : window.swapchain_image_views)
                views.push_back(i);
            return views;
        }
        
        //framebuffers must be updated after calling this!
        bool update_swapchain()
        {
            //please don't touch this function again. please. (i did touch it again. many times ;> )

            vk_handle::description::swapchain_desc desc{};

            auto description = get::swapchain::description(this->surface, *this->owner, *this->swapchain);

            auto temp = swapchain;  //old swapchain saved here

            swapchain = std::make_shared<vk::swapchain>(description);

            temp.reset();    //destroys old swapchain

            update_swapchain_imageviews();

            return true;
        }  
        ~window_t()
        {
            glfwDestroyWindow(window_ptr);
        }
        description description;
        
        private:
        friend class frame;
        vk::surface            surface;
        vk::shared_swapchain swapchain;

        std::vector<vk::shared_image_view> swapchain_image_views;

        std::vector<VkImage> get_swapchain_images() const
        {
            //retrieve image handles. Remember : image count specified in swapchain creation is only a minimum!
            uint32_t swapchain_image_count;
            vkGetSwapchainImagesKHR(*owner, *swapchain, &swapchain_image_count,
            nullptr);
            std::vector<VkImage> swapchain_images(swapchain_image_count);
            vkGetSwapchainImagesKHR(*owner, *swapchain, &swapchain_image_count,
            swapchain_images.data());

            return swapchain_images;
        }
        
        void update_swapchain_imageviews()
        {
            auto images = get_swapchain_images();
            swapchain_image_views.clear(); //clear any old values. this kills the image views
            swapchain_image_views.reserve(images.size());
            for(auto& image : images)
            {
                vk_handle::description::image_view_desc desc{};
                desc.format = swapchain->description.features.surface_format.format;
                desc.image  = image;
                desc.parent = *owner;
                swapchain_image_views.emplace_back(std::make_shared<vk::image_view>(desc));
            }
        }
    };
    struct frame_data_t
    {
        struct indexed_data
        {
            vk::fence       f_rendering_finished;
            vk::semaphore   swapchain_img_acquired;
            vk::semaphore   s_rendering_finished;
            indexed_data(const vk::device& device) :
            f_rendering_finished(data::fence_desc{device, VK_FENCE_CREATE_SIGNALED_BIT}),
            swapchain_img_acquired(data::semaphore_desc{device}),
            s_rendering_finished(data::semaphore_desc{device})
            {
            }
        };
        std::vector<indexed_data> idx_data;
        std::vector<vk::framebuffer> swapchain_framebuffers;
        vk::renderpass framebuffer_renderpass;
        VkQueue present_queue;

        frame_data_t(const vk::device& device, uint frames_in_flight, std::vector<vk::shared_image_view> image_views, VkExtent2D framebuffer_size) : 
        framebuffer_renderpass(get_frame_renderpass_desc(device))
        {
            idx_data.reserve(frames_in_flight);
            for(size_t i = 0; i < frames_in_flight; ++i)
            {
                idx_data.emplace_back(device);
            }
            update_framebuffers(image_views, framebuffer_size, device);
            present_queue = get::device::queue_handle(device, device.description.present_queue);
        }
        void update_framebuffers(std::vector<vk::shared_image_view> image_views, VkExtent2D framebuffer_size, VkDevice device)
        {
            std::vector<data::framebuffer_desc> descriptions{};
            for(auto view : image_views)
            {
                data::framebuffer_desc desc{};
                desc.attachments.push_back(*view);
                desc.height = framebuffer_size.height;
                desc.width  = framebuffer_size.width;
                desc.renderpass = framebuffer_renderpass;
                desc.parent     = device;
                descriptions.push_back(desc);
            }
            swapchain_framebuffers.clear();
            swapchain_framebuffers.reserve(descriptions.size());
            for(auto& d : descriptions)
                swapchain_framebuffers.emplace_back(vk::framebuffer(d));
        }
        
        private:
        VkRenderPassBeginInfo get_begin_info(uint image_index)
        {
            VkRenderPassBeginInfo info{};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.clearValueCount = 1;
            info.pClearValues = &clr;
            info.pNext = nullptr;
            info.renderArea.extent = VkExtent2D{swapchain_framebuffers[0].description.width, swapchain_framebuffers[0].description.height};
            info.renderArea.offset = {0, 0};
            info.renderPass = framebuffer_renderpass;
            info.framebuffer = swapchain_framebuffers[image_index];   //image index!! I was putting frame idnex
            return info;
            //TODO make sure teh swapchain -> framebuffer pipeline is properly syncchronized!
            //I am lucky I was able to catch this error here
        }
        VkClearValue clr{};//kept alive for renderpass begin ingo
        friend class frame;
        uint frame_idx = 0;
        static data::renderpass_desc get_frame_renderpass_desc(const vk::device& device)
        {
            data::renderpass_desc desc{};
        
            desc.attachments.resize(1);
            desc.attachments[0].format = get::physical_device::get_surface_features(*VULKAN, device.description.phys_device).surface_format.format;
            desc.attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            desc.attachments[0].finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            desc.attachments[0].flags         = 0;
            desc.attachments[0].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;  //beginning of the subpass
            desc.attachments[0].storeOp       = VK_ATTACHMENT_STORE_OP_STORE; //end of the subpass
            desc.attachments[0].samples       = VK_SAMPLE_COUNT_1_BIT;
            desc.attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            desc.attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.subpass_descriptions.resize(1);
            desc.subpass_descriptions[0].bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
            desc.subpass_descriptions[0].color_attachment_refs.resize(1);
            desc.subpass_descriptions[0].color_attachment_refs[0].attachment = 0; //index
            desc.subpass_descriptions[0].color_attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //layout during subpassdesc
            desc.subpass_dependencies.resize(1);
            desc.subpass_dependencies[0].dependencyFlags = 0;   
            desc.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            desc.subpass_dependencies[0].dstSubpass = 0;
            desc.subpass_dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;            //wait for nothing
            desc.subpass_dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//this is where the color attachment load op and store op happens
            desc.subpass_dependencies[0].srcAccessMask = 0; //no access flags
            desc.subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            /*
            If stage 5 of B depends on stage 3 of A, then we specify such depedence in the src and dst stage masks.
            Stages 1-4 of B will be executed regardless of A, but stage 5 will wait on stage 3 of A.
            from reddit : https://www.reddit.com/r/vulkan/comments/muo5ud/subpasses_dependencies_stage_and_access_masks/
                Access masks relate to memory availability/visibility. Somewhat suprising (at least it was to me initially), is that just because you set up an execution dependency where for example, A (the src) writes to some resource and then B (dst) reads from the resource. Even if B executes after A, that doesn't mean B will "see" the changes A has made, because of caching! It is very possible that even though A has finished, it has made its changes to a memory cache that hasn't been made available/"flushed". So in the dependency you could use

                    srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT
                    dstAccessMask=VK_ACCESS_MEMORY_READ_BIT

                I don't know actually know how gpu cache structures work or are organized but I think the general idea is

                    The src access mask says that the memory A writes to should be made available/"flushed" to like the shared gpu memory

                    The dst access mask says that the memory/cache B reads from should first pull from the shared gpu memory

                This way B is reading from up to date memory, and not stale cache data.
            https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
                srcStageMask of TOP_OF_PIPE is basically saying “wait for nothing”, or to be more precise,
                we’re waiting for the GPU to parse all commands, which is, a complete noop. 
                We had to parse all commands before getting to the pipeline barrier command to begin with.

                As an analog to srcStageMask with TOP_OF_PIPE, for dstStageMask, using BOTTOM_OF_PIPE can be kind of useful.
                This basically translates to “block the last stage of execution in the pipeline”. Basically, we translate 
                this to mean “no work after this barrier is going to wait for us”. 
            BOTTOM OF THE PIPE is the last stage of execution, TOP OF THE PIPE is the first 
                Memory access and TOP_OF_PIPE/BOTTOM_OF_PIPE

                Never use AccessMask != 0 with these stages. These stages do not perform memory accesses,
                so any srcAccessMask and dstAccessMask combination with either stage will be meaningless,
                and spec disallows this. TOP_OF_PIPE and BOTTOM_OF_PIPE are purely there for the sake of execution barriers,
                not memory barriers.
        */
            desc.parent = device;
        
            return desc;
        }
        static data::cmd_pool_desc get_cmdpool_desc(const vk::device& device)
        {
            data::cmd_pool_desc desc{};
            desc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            desc.parent = device;
            desc.queue_fam_index = device.description.graphics_queue.fam_idx;
            return desc;
        }

    };
    window_t window;
    frame_data_t data;
public:
    
    frame(int width, int height, const char* title, uint frames_in_flight, vk::shared_device device) :
    window({width, height, title}, *VULKAN, device), 
    data(*device, frames_in_flight, window_t::get_window_image_views(window), window.get_framebuffer_size())
    {
        glfwSetWindowUserPointer(window.window_ptr, this);
        glfwSetFramebufferSizeCallback(window.window_ptr, window_resize_callback);   
    }
    
    bool draw_frames(frame_render_callback_fnc render_callback, const render_data_t& render_data)
    {
        return internal_draw_frames(data, data.idx_data.size(), render_callback, render_data);
    }

    VkRenderPass get_renderpass()
    {
        return data.framebuffer_renderpass;
    }
    GLFWwindow* get_window_handle()
    {
        return window.window_ptr;
    }
private:
    void update_frame()
    {
        window.update_swapchain();
        data.update_framebuffers(window_t::get_window_image_views(window), window.get_framebuffer_size(), *window.owner);
        frame_resized = false;
    }
    bool frame_resized = false;
    static void window_resize_callback(GLFWwindow* window, [[maybe_unused]] int width, [[maybe_unused]] int height)
    {
        auto ptr = glfwGetWindowUserPointer(window);
        reinterpret_cast<frame*>(ptr)->frame_resized = true;
    }
    
    bool internal_draw_frames(frame_data_t& data, const uint& max_frames_inflight,
    frame_render_callback_fnc render_callback, const render_data_t& render_data, const bool throws = true)
    {
        data.frame_idx = (data.frame_idx + 1)%max_frames_inflight;
        auto& idx_data = data.idx_data[data.frame_idx];


        auto& device_handle = *window.owner;
        VkFence frame_rendered = idx_data.f_rendering_finished;

        vkWaitForFences(device_handle, 1, &frame_rendered, VK_TRUE, UINT64_MAX);


        uint32_t swpch_img_idx;
        auto swpch_res = vkAcquireNextImageKHR(device_handle, *window.swapchain, UINT64_MAX, idx_data.swapchain_img_acquired,
        VK_NULL_HANDLE, &swpch_img_idx);
        EXIT_IF(swpch_res < 0, "FAILED TO ACQUIRE NEXT SWAPCHAIN IMAGE", DO_NOTHING)

        if(frame_resized)
        {
            update_frame();
            return true;
        }

        vkResetFences(device_handle, 1, &frame_rendered);

        
        render_callback(idx_data.s_rendering_finished, idx_data.f_rendering_finished, idx_data.swapchain_img_acquired, 
        data.get_begin_info(swpch_img_idx), data.frame_idx, render_data, throws);

        VkPresentInfoKHR swpch_present_info{};
        swpch_present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        VkSemaphore sem = idx_data.s_rendering_finished;
        swpch_present_info.waitSemaphoreCount = 1, swpch_present_info.pWaitSemaphores = &sem;
        swpch_present_info.swapchainCount = 1, swpch_present_info.pSwapchains = &window.swapchain->handle, swpch_present_info.pImageIndices = &swpch_img_idx;

        EXIT_IF(vkQueuePresentKHR(data.present_queue, &swpch_present_info) < 0, "FRAME SUBMIT FAILED", DO_NOTHING);

        return true;
    }
};

//remember, this is a thin vulkan abstraction :>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    vulkan_context context;
    context.start();    //this will enforce correct destruction order
    
    vk::shared_device device(std::make_shared<vk::device>(get::device::description(*VULKAN, 
    get::physical_device::pick_best_physical_device(PHYSICAL_DEVICES))));

    constexpr uint FRAMES_IN_FLIGHT = 1;

    frame my_frame(150, 150, "title", FRAMES_IN_FLIGHT, device);

    render_data_t render_data(*device, my_frame.get_renderpass(), FRAMES_IN_FLIGHT);

    while(!glfwWindowShouldClose(my_frame.get_window_handle()))
    {
        glfwPollEvents();
        my_frame.draw_frames(render_triangles, render_data);
    }
    vkDeviceWaitIdle(*device);
}