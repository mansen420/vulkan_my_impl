#include "volk.h"
#include "GLFW/glfw3.h" //for surface

#include "vulkan_handle_util.h"
#include "vulkan_handle_make_shared.h"
#include "debug.h"
#include "read_file.h"

#include <map>
#include <algorithm>


//this module handles all communication with vulkan


            /***********************************helper interface***********************************/ 


#define EXIT_IF(COND, MESSAGE, CLEANUP)     \
        if(COND)                            \
        {                                   \
            CLEANUP();                      \
            if(throws){                     \
                DEBUG_LOG("CRITICAL ERROR");\
                THROW(MESSAGE);}            \
            return false;                   \
        }                                   \

void DO_NOTHING(){}
#define DO(STATEMENTS) [=]()mutable{STATEMENTS}


struct image_t
{
    VkExtent2D extent;
    VkSurfaceFormatKHR format;
};


struct instance_t
{
    std::shared_ptr<vk_handle::instance>handle{make_shared<vk_handle::instance>()};

    enum extension_enable_flag_bits
    {
        GLFW  = 0b0001,
        DEBUG = 0b0010
    };
    static std::vector<std::string> get_required_extension_names(uint flags)
    {
        std::vector<std::string> names;
        if(flags & GLFW)
        {
            uint32_t count;
            auto ptr = glfwGetRequiredInstanceExtensions(&count);
            names.reserve(names.size() + count);
            for(uint32_t i = 0; i < count; ++i)
                names.push_back(std::string(ptr[i]));
        }
        if(flags & DEBUG)
            names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        return names;
    }
    static std::vector<std::string> get_required_layer_names(uint flags)
    {
        std::vector<std::string> names;
        if(flags & DEBUG)
            names.push_back("VK_LAYER_KHRONOS_validation");
        return names;
    }
    static std::vector<std::string> get_available_instance_extension_names()
    {
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        VkExtensionProperties* ptr = new VkExtensionProperties[count]; //never use a pointer uninitialized!! THE COMPILER WANRED ME ABOUT THIS
        vkEnumerateInstanceExtensionProperties(nullptr, &count, ptr);
        std::vector<std::string> names;
        names.reserve(count);
        for(size_t i = 0; i < count; ++i)
            names.push_back(std::string(ptr[i].extensionName));
        delete[] ptr;
        return names;
    }
    static std::vector<std::string> get_available_instance_layers_names()
    {
        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        VkLayerProperties* ptr = new VkLayerProperties[count];
        vkEnumerateInstanceLayerProperties(&count, ptr);
        std::vector<std::string> names(count);
        for(size_t i = 0; i < count; ++i)
            names[i] = std::string(ptr[i].layerName);
        delete[] ptr;
        return names;
    }
    static vk_handle::description::instance_desc get_instance_description(uint ext_flags, uint layer_flags)
    {
        vk_handle::description::instance_desc description{};

        std::vector<std::string> req_ext;
        req_ext = get_required_extension_names(ext_flags);
        
        std::vector<std::string> req_layers;
        req_layers = get_required_layer_names(layer_flags);

        auto names = get_available_instance_extension_names();
        if(!check_support(names, req_ext))
            INFORM_ERR("requested instance extension not supported!");
        names = get_available_instance_layers_names();
        if(!check_support(names, req_layers))
            INFORM_ERR("requested instance layer not supported!");

        description.ext_info.extensions = req_ext;
        description.ext_info.layers     = req_layers;
        description.app_info            = get_app_info();

        if((layer_flags & DEBUG) xor (ext_flags & DEBUG))
            INFORM("WARNING : conflicting flags in extension and layers!");
        if((layer_flags & DEBUG )|| (ext_flags & DEBUG))
            description.debug_messenger_ext = get_debug_create_info();
        
        return description;
    }
private:
};

struct physical_device_t
{

    static std::vector<physical_device_t> find_physical_devices(VkInstance instance)
    {
        std::vector<physical_device_t> physical_devices;
        auto handles = get_physical_device_handles(instance);
        physical_devices.reserve(handles.size());
        for(const auto& handle : handles)
        {
            physical_device_t device;
            device.handle = handle;
            physical_devices.push_back(device);
            auto props = device.get_properties();
            INFORM("Physical device determined : " << props.deviceName);
        }
        return physical_devices;
    }
    static bool supports_extensions(physical_device_t device, std::vector<std::string> extensions)
    {
        auto props = device.get_properties();
        INFORM(props.deviceName << " : ");
        auto avlbl_ext = device.get_available_extensions();
        return check_support(avlbl_ext, extensions);
    }
    static physical_device_t pick_best_physical_device(std::vector<physical_device_t> devices)
    {
        std::map<VkDeviceSize, physical_device_t> device_memory_size; //sorted ascending
        for(size_t i = 0; i < devices.size(); ++i)
        {
            device_memory_size.insert({get_local_memory_size(devices[i]), devices[i]});
        }
        
        auto picked_device = (*device_memory_size.rbegin()).second;
        auto device_mem_size = (*device_memory_size.rbegin()).first;
        for(auto itr = device_memory_size.rbegin(); itr != device_memory_size.rend(); ++itr)
        {
            auto props = (*itr).second.get_properties();
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            || props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                picked_device = (*itr).second;
                device_mem_size = (*itr).first;
            }
        }
        auto props = picked_device.get_properties();
        INFORM("Picked " << props.deviceName << "\nWith " << device_mem_size << " Bytes of local memory.");

        return picked_device;
    }
    static VkDeviceSize get_local_memory_size(physical_device_t physical_device)
    {
        VkDeviceSize device_memory_size{};
        auto mem_props = physical_device.get_memory_properties();
        for(uint32_t j = 0; j < mem_props.memoryHeapCount; ++j)
            if(mem_props.memoryHeaps[j].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                device_memory_size += mem_props.memoryHeaps[j].size;
        return device_memory_size;
    }
    
    VkPhysicalDevice handle;

    enum extension_enable_flag_bits
    {
        SWAPCHAIN = 0b01
    };
    static std::vector<std::string> get_required_extension_names(uint flags)
    {
        std::vector<std::string> names;
        if(flags & SWAPCHAIN)
            names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        return names;
    }

    std::vector<VkQueueFamilyProperties> get_queue_fams()
    {
        std::vector<VkQueueFamilyProperties> queue_fams;
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, nullptr);
        queue_fams.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, queue_fams.data());
        return queue_fams;
    }
    std::vector<std::string> get_available_extensions()
    {
        std::vector<std::string> available_extensions;
        uint32_t count;
        vkEnumerateDeviceExtensionProperties(handle, nullptr, &count, nullptr);
        VkExtensionProperties* ptr = new VkExtensionProperties[count];
        vkEnumerateDeviceExtensionProperties(handle, nullptr, &count, ptr);

        available_extensions.resize(count);
        for(size_t i = 0; i < count; ++i)
            available_extensions[i] = std::string(ptr[i].extensionName);
        delete[] ptr;
        return available_extensions;
    }
    VkPhysicalDeviceMemoryProperties get_memory_properties()
    {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(handle, &memory_properties);
        return memory_properties;
    }
    VkPhysicalDeviceProperties get_properties()
    {
        VkPhysicalDeviceProperties f;
        vkGetPhysicalDeviceProperties(handle, &f);
        return f;
    }
    VkPhysicalDeviceFeatures get_features()
    {
        VkPhysicalDeviceFeatures f;
        vkGetPhysicalDeviceFeatures(handle, &f);
        return f;
    }
};

struct device_t
{
    //assigning to this will combine flags!
    struct family_index
    {
        int index      = -1;
        uint32_t flags =  0;
        operator uint32_t() const {return index;}
        family_index() : index(-1), flags(0) {}
        family_index(uint32_t index, uint32_t flags) : index(index), flags(flags) {}
        family_index(const family_index& other)
        {
            *this = other;
        }
        family_index& operator =(const family_index& rhs)
        {
            if(this == &rhs)
                return *this;
            this->index = rhs.index;
            this->flags |= rhs.flags;
            return *this;
        }
        bool operator < (const family_index& rhs)
        {
            return this->index < rhs.index;
        }
    };
    struct family_indices_t
    {
        std::optional<family_index> graphics, compute, present, transfer;
    };
    struct queue_t
    {
        VkQueue           handle;
        uint32_t index_in_family;
        family_index     fam_idx;
    };
    physical_device_t phys_device{};
    std::shared_ptr<vk_handle::device> handle{make_shared<vk_handle::device>()};
    device_t() : handle(make_shared<vk_handle::device>()){}
    queue_t graphics_queue{};
    queue_t transfer_queue{};
    queue_t  compute_queue{};
    queue_t  present_queue{};

    static void report_device_queues( device_t device)
    {
        auto list_queue_props = [](const queue_t& q)
        {
            INFORM("Family index : " << q.fam_idx.index << " Index within family : " << q.index_in_family);
        };
        INFORM("Device queues\nGraphics Queue");
        list_queue_props(device.graphics_queue);
        INFORM("Compute Queue");
        list_queue_props(device.compute_queue);
        INFORM("Transfer Queue");
        list_queue_props(device.transfer_queue);
        INFORM("Present Queue");
        list_queue_props(device.present_queue);
    }
    static bool find_queue_indices(VkInstance instance, device_t device, family_indices_t& indices, bool throws = true)
    {
        auto queue_fams = device.phys_device.get_queue_fams();

        vk_handle::surface dummy_surface; //just to check support

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        auto glfw_window = glfwCreateWindow(1, 1, "dummy", nullptr, nullptr);
        EXIT_IF(dummy_surface.init({instance, glfw_window}), "INIT SURFACE FAILED", DO(glfwDestroyWindow(glfw_window);));
        //find family indices
        for(uint32_t i = 0; i < queue_fams.size(); ++i)
        {   
            using namespace vk_handle::description;
            if(queue_fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.graphics = family_index{i, GRAPHICS_BIT};
            if(queue_fams[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
                if((queue_fams[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0)
                    indices.transfer = family_index{i, TRANSFER_BIT};
            if(queue_fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                if((queue_fams[i].queueCount & VK_QUEUE_GRAPHICS_BIT) == 0)
                    indices.compute  = family_index{i, COMPUTE_BIT};

            VkBool32 supports_present;
            vkGetPhysicalDeviceSurfaceSupportKHR(device.phys_device.handle, i, dummy_surface, &supports_present);
            if(supports_present)
                indices.present = family_index{i, PRESENT_BIT};
        }
        //cleanup
        glfwDestroyWindow(glfw_window);
        dummy_surface.destroy();
        //find fallbacks
        if(!indices.transfer.has_value())   //use a graphics queue
        {
            indices.transfer = indices.graphics;
            indices.transfer.value().flags = vk_handle::description::TRANSFER_BIT;
        }
        if(!indices.compute.has_value())    //find ANY compute queue
            for(uint32_t i = 0; i < queue_fams.size(); ++i)
                if(queue_fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    using namespace vk_handle::description;
                    indices.compute = family_index{i, COMPUTE_BIT};
                }
        bool found_all_families = indices.compute.has_value() && indices.graphics.has_value() && indices.present.has_value() && indices.transfer.has_value();
        EXIT_IF(!found_all_families, "FAILED TO FIND QUEUE FAMILIES", DO_NOTHING);
        return true;
    }
    //sets the queue members of device and the value you should pass to the handle's description
    static bool determine_queues(VkInstance instance, device_t& device, std::vector<vk_handle::description::device_queue>& device_queues,
    bool throws = true)
    {
        family_indices_t indices;
        find_queue_indices(instance, device, indices, throws);
        //determine device queues 
        /*
        the spec states that each device queue should refer to a unique family index.
         Since the family indices above are not necessarily unique, we must check for that
        */
        auto queue_fams = device.phys_device.get_queue_fams();

        //combine non-unique indices
        std::vector<family_index> unique_indices({indices.graphics.value(), indices.compute.value(), indices.transfer.value(), indices.present.value()});
        for(auto itri = unique_indices.begin(); itri != unique_indices.end(); ++itri)
        {
            auto& index = (*itri);
            for(auto itrj = unique_indices.begin(); itrj != unique_indices.end(); ++itrj)
            {
                if(itrj == itri)
                    continue;
                auto& candidate = itrj;
                if(index.index == (*candidate).index)
                {
                    index = *candidate; //combines the flags 
                    (*candidate).flags = 0; //"erase" candidate
                }
            }
        }
        for(const auto& index : unique_indices)
        {
            if(index.flags == 0)
                continue;
            using namespace vk_handle::description;
            uint32_t index_queue_count = 0;
            if(index.flags & GRAPHICS_BIT)
            {
                index_queue_count++;
                device.graphics_queue.fam_idx = index;
                device.graphics_queue.index_in_family = index_queue_count - 1;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
            }
            if(index.flags & COMPUTE_BIT)
            {
                index_queue_count++;
                device.compute_queue.fam_idx = index;
                device.compute_queue.index_in_family = index_queue_count - 1;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
            }
            if(index.flags & PRESENT_BIT)
            {
                index_queue_count++;
                device.present_queue.fam_idx = index;
                device.present_queue.index_in_family = index_queue_count - 1;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
            }
            if(index.flags & TRANSFER_BIT)
            {
                index_queue_count++;
                device.transfer_queue.fam_idx = index;
                device.transfer_queue.index_in_family = index_queue_count - 1;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
            }
            vk_handle::description::device_queue device_queue{};
            device_queue.count = index_queue_count;
            if(index.index < 0)
                INFORM_ERR("WARNING : using negative family index");
            device_queue.family_index = index.index;
            device_queue.flags = index.flags;
            device_queue.queue_family_flags = queue_fams[index].queueFlags; //eh why not
            device_queues.push_back(device_queue);
        }
        //determine priority
        for(auto& queue : device_queues)
        {
            float priority = 0.0f;
            using namespace vk_handle::description;
            if(queue.flags & GRAPHICS_BIT)
                priority += 1.0f;
            if(queue.flags & TRANSFER_BIT)
                priority += 0.5f;
            if(queue.flags & COMPUTE_BIT)
                priority += 0.5f;
            if(queue.flags & PRESENT_BIT)
                priority += 0.25f;
            priority = std::clamp(priority, 0.0f, 1.0f);
            queue.priority = priority;
        }
        return true;
    }

};

struct window_t
{
    std::shared_ptr<vk_handle::surface>     surface{};
    std::shared_ptr<vk_handle::swapchain> swapchain{};

    device_t owner;

    window_t& operator =(const window_t& rhs)
    {
        if(this == &rhs)
            return *this;
        if(!rhs)
            INFORM_ERR("WARNING : copying an empty window! Their resources aren't shared!");
        this->owner = rhs.owner;
        this->surface = rhs.surface;
        this->swapchain = rhs.swapchain;
        this->window_ptr = rhs.window_ptr;

        return *this;
    }
    window_t(const window_t& other)
    {
        *this = other;
    }
    window_t(){}
    //GFLW will refrain from dereferencing NULL 
    std::shared_ptr<GLFWwindow> window_ptr{NULL, [](GLFWwindow* ptr)mutable{glfwDestroyWindow(ptr);}};
    operator bool() const {return window_ptr.get() != NULL;}

    vk_handle::description::surface_features get_features() const {return swapchain->get_description().features;}
    static std::vector<std::shared_ptr<vk_handle::image_view>> get_window_attachments(const window_t& window, bool throws = true)
    {
        auto images = window.get_swapchain_images();
        std::vector<std::shared_ptr<vk_handle::image_view>> attachments;
        attachments.reserve(images.size());
        for(auto& image : images)
        {
            vk_handle::description::image_view_desc desc{};
            desc.format = window.swapchain->get_description().features.surface_format.format;
            desc.image  = image;
            desc.parent = *window.owner.handle;
            auto img_view = make_shared<vk_handle::image_view>();
            if(img_view->init(desc) && throws)
                THROW("FAILED TO INIT SWAPCHAIN IMAGE VIEW");
            attachments.push_back(img_view);
        }
        return attachments;
    }
    std::vector<VkImage> get_swapchain_images() const
    {
        //retrieve image handles. Remember : image count specified in swapchain creation is only a minimum!
        uint32_t swapchain_image_count;
        vkGetSwapchainImagesKHR(*owner.handle, *swapchain, &swapchain_image_count,
        nullptr);
        std::vector<VkImage> swapchain_images(swapchain_image_count);
        vkGetSwapchainImagesKHR(*owner.handle, *swapchain, &swapchain_image_count,
        swapchain_images.data());

        return swapchain_images;
    }
};

struct renderpass_t
{
    std::shared_ptr<vk_handle::renderpass> handle = make_shared<vk_handle::renderpass>();
    std::vector<VkAttachmentDescription> attachments{};
    std::vector<vk_handle::description::subpass_description> subpass_descriptions{};
    std::vector<VkSubpassDependency> subpass_dependencies{};

    device_t owner;
};

struct framebuffer_t
{
    std::shared_ptr<vk_handle::framebuffer> handle{make_shared<vk_handle::framebuffer>()};
    framebuffer_t() : handle(make_shared<vk_handle::framebuffer>()) {}

    std::vector<std::shared_ptr<vk_handle::image_view>> attachments;

    uint32_t width, height;
    std::optional<uint32_t> layers;
    std::optional<VkFramebufferCreateFlags> flags;

    renderpass_t owner;
    static std::vector<framebuffer_t> get_window_framebuffers(const window_t& window)
    {
        auto attachments = window_t::get_window_attachments(window);
        std::vector<framebuffer_t> frmbffrs;
        frmbffrs.reserve(attachments.size());
        for(auto att : attachments)
        {
            framebuffer_t frm;
            frm.height = window.get_features().extent.height, frm.width = window.get_features().extent.width;
            frm.attachments.push_back(att);
            frmbffrs.push_back(frm);
        }
        return frmbffrs;
    }

};

struct pipeline_layout_t
{
    std::shared_ptr<vk_handle::pipeline_layout> handle{make_shared<vk_handle::pipeline_layout>()};
    vk_handle::description::pipeline_layout_desc description;

    device_t owner;
};

struct graphics_pipeline_t
{
    std::shared_ptr<vk_handle::graphics_pipeline> handle{make_shared<vk_handle::graphics_pipeline>()};

    renderpass_t     owner;
    uint32_t subpass_index;

    pipeline_layout_t layout;

    vk_handle::description::viewport_state_desc viewport_state;
    vk_handle::description::vertex_input_desc vertex_input_state;
    vk_handle::description::dynamic_state_desc dynamic_state;
    vk_handle::description::color_blend_desc color_blend_state;
    vk_handle::description::multisample_desc ms_state;
    vk_handle::description::rasterization_desc rasterization_state;
    vk_handle::description::input_assembly_desc input_assembly_state;
    std::vector<vk_handle::description::shader_stage_desc> shader_modules;
    vk_handle::description::depth_stencil_desc depth_stencil_state;
    VkPipelineCache pipeline_cache;
};

struct shader_module_t
{
    std::shared_ptr<vk_handle::shader_module> handle{make_shared<vk_handle::shader_module>()};

    device_t owner;

    VkShaderStageFlagBits stage;

    std::vector<char> byte_code;
};

struct command_pool
{
    std::shared_ptr<vk_handle::cmd_pool> handle{make_shared<vk_handle::cmd_pool>()};
};

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

vk_handle::instance VULKAN;
std::vector<physical_device_t> PHYSICAL_DEVICES;

static bool INIT = false;


        /***************************************PROCEDURES***************************************/

//TODO listen bud, you can do this. You're gonna run down every function in this file, and I mean EVERY FUNCTION, and 
//factor out whatever you can. 'k, bud?
//Also, definitely use Vulkan.hpp next time...

//TODO implement RAII classes that wrap these bad boys up?
//I think it would be good to ensure no double init or such shenanigans happen

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
        vk_handle::instance            vulkan_instance;
        std::vector<physical_device_t> physical_devices;
        vk_handle::debug_messenger     debug_messenger;
    };
};

//terminates 3rd party libraries and all Vulkan objects
void terminate()
{
    if(!INIT)
        INFORM_ERR("WARNING : calling terminate() without successful init()");
    INFORM("Terminating context...");
    TERMINATION_QUEUE.flush();
    INIT = false;
}


//Initializes all third party dependencies, as well as the Vulkan instanc and debugger, and determines physical devices.
//In case of failure, returns false, and, if throws is set, throws runtime error.
bool init(bool throws = true)
{
    if(INIT)
        return true;

using namespace vk_handle;
using namespace vk_handle::description;

    //this is vulkan baby
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    EXIT_IF(glfwInit() == GLFW_FALSE, "GLFW INIT FAILED", terminate);

    TERMINATION_QUEUE.push(DO(glfwTerminate();));
    
    EXIT_IF(volkInitialize(), "VOLK INIT FAILED", terminate);

    TERMINATION_QUEUE.push(DO(volkFinalize();));

    instance_desc description{};
    try
    {
        uint debug_flag = 0;
        if(DEBUG_MODE)
            debug_flag = instance_t::DEBUG;
        description = instance_t::get_instance_description(instance_t::GLFW | debug_flag, debug_flag);
    }
    catch(const std::exception& e)
    {
        EXIT_IF(true, e.what(), terminate);
    }
    EXIT_IF(VULKAN.init(description), "VULKAN INSTANTIATION FAILED", terminate);
    
    TERMINATION_QUEUE.push(DO(VULKAN.destroy();));

    volkLoadInstance(VULKAN);

    if(DEBUG_MODE)
    {
        debug_messenger db;
        EXIT_IF(db.init({VULKAN, get_debug_create_info()}), "DEBUGGER INIT FAILED", terminate);

        TERMINATION_QUEUE.push(DO(db.destroy();));
    }


    //check for swapchain support by default
    auto candidates = physical_device_t::find_physical_devices(VULKAN);
    for(const auto& candidate : candidates)
        if(physical_device_t::supports_extensions(candidate, physical_device_t::get_required_extension_names(physical_device_t::SWAPCHAIN)))
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

//a device is just something that takes rendering commands (window agnostic)
bool create_device(device_t& device, bool throws = true)
{
    device.phys_device = physical_device_t::pick_best_physical_device(PHYSICAL_DEVICES);
    vk_handle::description::device_desc description{};
    description.enabled_features   = device.phys_device.get_features();
    description.phys_device        = device.phys_device.handle;
    description.enabled_extensions = physical_device_t::get_required_extension_names(physical_device_t::SWAPCHAIN);
    device_t::determine_queues(VULKAN, device, description.device_queues);
    EXIT_IF(device.handle->init(description), "FAILED TO INIT DEVICE", DO_NOTHING);
    
    return true;
}

bool create_window(const device_t& device, int width, int height, const char* title, window_t& window, bool throws = true)
{
    EXIT_IF(window, "INITIALIZING NON-EMPTY WINDOW", DO_NOTHING);

    using namespace vk_handle;
    using namespace vk_handle::description;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window.window_ptr = std::shared_ptr<GLFWwindow>{glfwCreateWindow(width, height, title, nullptr, nullptr),
    [](GLFWwindow* ptr)mutable{glfwDestroyWindow(ptr);}};

    //the basic assumption is that srf has a valid pointer inside it, and that the underlying vulkan handle is not initialized
    //this assumption holds because this function rejects double initialization of windows

    auto& srf = window.surface;
    srf = make_shared<vk_handle::surface>();
    EXIT_IF(srf->init({VULKAN, window.window_ptr.get()}), "FAILED TO CREATE WINDOW SURFACE", DO_NOTHING)


    swapchain_desc desc{};
    desc.surface = *srf;
    desc.features = get_swapchain_features(get_swapchain_support(device.phys_device.handle, *srf), window.window_ptr.get());
    desc.device_queues = device.handle->get_description().device_queues;
    desc.parent = *device.handle;
    window.swapchain = make_shared<vk_handle::swapchain>();
    EXIT_IF(window.swapchain->init(desc), "FAILED TO CREATE SWAPCHAIN", DO_NOTHING)

    window.owner = device;

    return true;
}

bool create_renderpass(const device_t& device, renderpass_t& renderpass, bool throws = true)
{
    vk_handle::description::renderpass_desc desc{};
    desc.attachments = renderpass.attachments;
    desc.parent = *device.handle;
    desc.subpass_dependencies = renderpass.subpass_dependencies;
    desc.subpass_descriptions = renderpass.subpass_descriptions;
    EXIT_IF(renderpass.handle->init(desc), "FAILED TO INIT RENDERPASS", DO_NOTHING);
    renderpass.owner = device;
    return true;
}

bool create_framebuffer(const renderpass_t& renderpass, framebuffer_t& framebuffer, bool throws = true)
{
    vk_handle::description::framebuffer_desc desc{};

    EXIT_IF(framebuffer.attachments.empty(), "EMPTY FRAMEBUFFE ATTACHMENT VECTOR", DO_NOTHING)

    desc.attachments.reserve(framebuffer.attachments.size());
    for(auto att : framebuffer.attachments)
        desc.attachments.push_back(*att);
    desc.flags  = framebuffer.flags;
    desc.height = framebuffer.height, desc.width = framebuffer.width;
    desc.layers = framebuffer.layers;

    framebuffer.owner = renderpass;
    desc.renderpass = *framebuffer.owner.handle;
    desc.parent     = *framebuffer.owner.owner.handle;

    EXIT_IF(framebuffer.handle->init(desc), "FAILED TO INIT FRAMEBUFFER", DO_NOTHING);

    return true;
}

bool create_pipeline_layout(pipeline_layout_t& pipeline_layout, bool throws = true)
{
    pipeline_layout.description.parent = *pipeline_layout.owner.handle;
    EXIT_IF(pipeline_layout.handle->init(pipeline_layout.description), "FAILED TO INIT PIPELINE LAYOUT", DO_NOTHING);
    return true;
}

bool create_graphics_pipeline(graphics_pipeline_t& pipeline, bool throws = true)
{
    vk_handle::description::graphics_pipeline_desc description{};

    description.color_blend_info = pipeline.color_blend_state;
    description.depth_stencil_info = pipeline.depth_stencil_state;
    description.dynamic_state_info = pipeline.dynamic_state;
    description.input_assembly_info = pipeline.input_assembly_state;
    description.multisample_info = pipeline.ms_state;
    description.parent = *pipeline.owner.owner.handle;
    description.pipeline_cache = pipeline.pipeline_cache;
    description.pipeline_layout = *pipeline.layout.handle;
    description.rasterization_info = pipeline.rasterization_state;
    description.renderpass = *pipeline.owner.handle;
    description.shader_stages_info = pipeline.shader_modules;
    description.subpass_index = pipeline.subpass_index;
    description.vertex_input_info = pipeline.vertex_input_state;
    description.viewport_state_info = pipeline.viewport_state;

    EXIT_IF(pipeline.handle->init({description}), "FAILED TO INIT GRAPHICS PIPELINE", DO_NOTHING);
    return true;
}

bool create_shader_module(shader_module_t& module, bool throws = true)
{
    vk_handle::description::shader_module_desc description{};
    description.byte_code = module.byte_code;
    description.parent    = *module.owner.handle;
    description.stage     = module.stage;

    EXIT_IF(module.handle->init(description), "FAILED TO INIT SHADER MODULE", DO_NOTHING);

    return true;
}
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
unsigned int vulkan_context::copies = 0;

void draw_frame()
{

}
void invoke_renderpass()
{
    glfwPollEvents();
    draw_frame();
}

//remember, this is a thin vulkan abstraction :>
int main()
{
    vulkan_context context;
    context.start();    //this will enforce correct destruction order
    device_t mydev;
    create_device(mydev);
    window_t wind;
    create_window(mydev, 100, 100, "title", wind);

    renderpass_t renderpass;
    {
        renderpass.attachments.resize(1);
        renderpass.attachments[0].format = wind.get_features().surface_format.format;
        renderpass.attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        renderpass.attachments[0].finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        renderpass.attachments[0].flags         = 0;
        renderpass.attachments[0].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;  //beginning of the subpass
        renderpass.attachments[0].storeOp       = VK_ATTACHMENT_STORE_OP_STORE; //end of the subpass
        renderpass.attachments[0].samples       = VK_SAMPLE_COUNT_1_BIT;
        renderpass.attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        renderpass.attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

        renderpass.subpass_descriptions.resize(1);
        renderpass.subpass_descriptions[0].bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        renderpass.subpass_descriptions[0].color_attachment_refs.resize(1);
        renderpass.subpass_descriptions[0].color_attachment_refs[0].attachment = 0; //index
        renderpass.subpass_descriptions[0].color_attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //layout during subpass

        renderpass.subpass_dependencies.resize(1);
        renderpass.subpass_dependencies[0].dependencyFlags = 0;   
        renderpass.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        renderpass.subpass_dependencies[0].dstSubpass = 0;
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
        renderpass.subpass_dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;            //wait for nothing
        renderpass.subpass_dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//this is where the color attachment load op and store op happens
        renderpass.subpass_dependencies[0].srcAccessMask = 0; //no access flags
        renderpass.subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    create_renderpass(mydev, renderpass);      

    auto s = window_t::get_window_attachments(wind);
    framebuffer_t f;
    auto window_frmbffrs = framebuffer_t::get_window_framebuffers(wind);
    for (auto& f : window_frmbffrs)
        create_framebuffer(renderpass, f);
    graphics_pipeline_t triangle_pipeline;
    {
        triangle_pipeline.color_blend_state.logic_op_enabled = VK_FALSE;
        VkPipelineColorBlendAttachmentState color_attachment{};
        color_attachment.blendEnable = VK_FALSE;
        color_attachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT  | VK_COLOR_COMPONENT_B_BIT;
        triangle_pipeline.color_blend_state.attachment_states.push_back(color_attachment);

        triangle_pipeline.dynamic_state.dynamic_state_list = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        
        triangle_pipeline.input_assembly_state.primitive_restart_enabled = VK_FALSE;
        triangle_pipeline.input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        
        pipeline_layout_t pplt;
        pplt.owner = mydev;
        create_pipeline_layout(pplt);
        triangle_pipeline.layout = pplt;

        triangle_pipeline.ms_state.rasterization_samples = VK_SAMPLE_COUNT_1_BIT;
        triangle_pipeline.ms_state.sample_shading_enable = VK_FALSE;

        triangle_pipeline.owner = renderpass;
        triangle_pipeline.subpass_index = 0;
        triangle_pipeline.pipeline_cache = VK_NULL_HANDLE;
        triangle_pipeline.rasterization_state.polygon_mode = VK_POLYGON_MODE_FILL;
        triangle_pipeline.rasterization_state.rasterization_discard = VK_FALSE;
        triangle_pipeline.rasterization_state.depth_bias_enable = VK_FALSE;
        triangle_pipeline.rasterization_state.depth_clamp_enable = VK_FALSE;
        triangle_pipeline.rasterization_state.cull_mode = VK_CULL_MODE_NONE;

        std::vector<char> fragment_code, vertex_code;
        read_binary_file({"shaders/"}, "triangle_frag.spv", fragment_code);
        read_binary_file({"shaders/"}, "triangle_vert.spv", vertex_code);
        shader_module_t frag, vert;
        frag.byte_code = fragment_code, vert.byte_code = vertex_code;
        frag.owner = vert.owner = mydev;
        frag.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        vert.stage = VK_SHADER_STAGE_VERTEX_BIT;

        create_shader_module(vert);
        create_shader_module(frag);

        triangle_pipeline.shader_modules.resize(2);
        triangle_pipeline.shader_modules[0].module = *vert.handle;
        triangle_pipeline.shader_modules[0].stage  = vert.stage;
        triangle_pipeline.shader_modules[1].module = *frag.handle;
        triangle_pipeline.shader_modules[1].stage  = frag.stage;
        triangle_pipeline.shader_modules[0].entry_point = "main";
        triangle_pipeline.shader_modules[1].entry_point = "main";

        triangle_pipeline.viewport_state.scissors.resize(1);
        triangle_pipeline.viewport_state.viewports.resize(1);

        triangle_pipeline.vertex_input_state.attrib_descriptions.resize(0);
        triangle_pipeline.vertex_input_state.binding_descriptions.resize(0);
        
        create_graphics_pipeline(triangle_pipeline);
    }
}