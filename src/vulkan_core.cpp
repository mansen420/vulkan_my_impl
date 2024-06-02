#include "volk.h"
#include "GLFW/glfw3.h" //for surface

#include "vulkan_handle_util.h"
#include "vulkan_handle_make_shared.h"
#include "debug.h"
#include "read_file.h"

#include <map>
#include <algorithm>


typedef unsigned int uint; //MSVC can't handle the power of pure uint

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



struct instance_t
{
    enum extension_enable_flag_bits
    {
        GLFW              = 0b0001,
        DEBUG             = 0b0010,
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

vk_handle::instance VULKAN;

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
        SWAPCHAIN         = 0b001
    };
    static std::vector<std::string> get_required_extension_names(uint flags)
    {
        std::vector<std::string> names;
        if(flags & SWAPCHAIN)
            names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        return names;
    }

    std::vector<VkQueueFamilyProperties> get_queue_fams()const
    {
        std::vector<VkQueueFamilyProperties> queue_fams;
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, nullptr);
        queue_fams.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, queue_fams.data());
        return queue_fams;
    }
    std::vector<std::string> get_available_extensions()const
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
    VkPhysicalDeviceMemoryProperties get_memory_properties()const
    {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(handle, &memory_properties);
        return memory_properties;
    }
    VkPhysicalDeviceProperties get_properties()const
    {
        VkPhysicalDeviceProperties f;
        vkGetPhysicalDeviceProperties(handle, &f);
        return f;
    }
    VkPhysicalDeviceFeatures get_features()const
    {
        VkPhysicalDeviceFeatures f;
        vkGetPhysicalDeviceFeatures(handle, &f);
        return f;
    }
    //don't call this frequently
    vk_handle::description::surface_features get_surface_features() const 
    {
        vk_handle::surface srf;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        auto glfw = glfwCreateWindow(1, 1, "", nullptr, nullptr);
        srf.init({VULKAN, glfw});
        auto result = get_swapchain_features(get_swapchain_support(this->handle, srf), glfw);   
        glfwDestroyWindow(glfw);
        srf.destroy();
        return result;
    }
};

std::vector<physical_device_t> PHYSICAL_DEVICES;


class device_t
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
    vk_handle::device handle;
public:
    operator VkDevice() const{return handle;}
    vk_handle::description::device_desc get_description()const{return handle.get_description();}
    device_t() 
    {
        try 
        {
            create_device(*this, VULKAN);
        }
        catch (std::exception& e)
        {
            INFORM_ERR(e.what());
        }
    }
    physical_device_t phys_device{};

    device_t& operator=(const device_t&) = delete;
    device_t(const device_t&) = delete;

    struct queue_t
    {
        VkQueue           handle;
        uint32_t index_in_family;
        family_index     fam_idx;
        operator VkQueue() const
        {
            return handle;
        }
    };
    //these should be const...
    queue_t graphics_queue{};
    queue_t transfer_queue{};
    queue_t  compute_queue{};
    queue_t  present_queue{};
    ~device_t()
    {
        handle.destroy();
    }
private:
    static void report_device_queues(device_t& device)
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
    static bool find_queue_indices(VkInstance instance, device_t& device, family_indices_t& indices, bool throws = true)
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
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
                device.graphics_queue.fam_idx = index;
                device.graphics_queue.index_in_family = index_queue_count - 1;

            }
            if(index.flags & COMPUTE_BIT)
            {
                index_queue_count++;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
                device.compute_queue.fam_idx = index;
                device.compute_queue.index_in_family = index_queue_count - 1;
            }
            if(index.flags & PRESENT_BIT)
            {
                index_queue_count++;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
                device.present_queue.fam_idx = index;
                device.present_queue.index_in_family = index_queue_count - 1;
            }
            if(index.flags & TRANSFER_BIT)
            {
                index_queue_count++;
                if(queue_fams[index].queueCount < index_queue_count)
                    index_queue_count--;
                device.transfer_queue.fam_idx = index;
                device.transfer_queue.index_in_family = index_queue_count - 1;
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
    //only call this AFTER determine_queues()
    static void init_queue_handles(device_t& device)
    {
        if(!device)
            INFORM_ERR("WARNING : trying to init queue handles of a device with NULL HANDLE");
        vkGetDeviceQueue(device.handle, device.graphics_queue.fam_idx, device.graphics_queue.index_in_family, &device.graphics_queue.handle);
        vkGetDeviceQueue(device.handle, device.compute_queue.fam_idx, device.compute_queue.index_in_family, &device.compute_queue.handle);
        vkGetDeviceQueue(device.handle, device.transfer_queue.fam_idx, device.transfer_queue.index_in_family, &device.transfer_queue.handle);
        vkGetDeviceQueue(device.handle, device.present_queue.fam_idx, device.present_queue.index_in_family, &device.present_queue.handle);
    }

    static bool create_device(device_t& device, vk_handle::instance instance, bool throws = true)
    {
        device.phys_device = physical_device_t::pick_best_physical_device(PHYSICAL_DEVICES);
        vk_handle::description::device_desc description{};
        description.enabled_features   = device.phys_device.get_features();
        description.phys_device        = device.phys_device.handle;
        //XXX watch out for lack of support here 
        description.enabled_extensions = physical_device_t::get_required_extension_names(
        physical_device_t::SWAPCHAIN);
        device_t::determine_queues(instance, device, description.device_queues);
        EXIT_IF(device.handle.init(description), "FAILED TO INIT DEVICE", DO_NOTHING);
        device_t::init_queue_handles(device);

        return true;
    }
};

class renderpass_t
{
    vk_handle::renderpass handle;
public:

    operator VkRenderPass() const {return handle;}

    struct description
    {
        std::vector<VkAttachmentDescription> attachments{};
        std::vector<vk_handle::description::subpass_description> subpass_descriptions{};
        std::vector<VkSubpassDependency> subpass_dependencies{};
    };
    vk_handle::description::renderpass_desc get_desription(){return handle.get_description();}
    renderpass_t(description description, std::shared_ptr<const device_t> owner)
    {
        create_renderpass(owner, *this, description);
    }
    std::shared_ptr<const device_t> owner;
    ~renderpass_t()
    {
        handle.destroy();
    }
private:
    bool create_renderpass(std::shared_ptr<const device_t> device, renderpass_t& renderpass, description description, bool throws = true)
    {
        vk_handle::description::renderpass_desc desc{};
        desc.attachments = description.attachments;
        desc.parent = *device;
        desc.subpass_dependencies = description.subpass_dependencies;
        desc.subpass_descriptions = description.subpass_descriptions;
        EXIT_IF(renderpass.handle.init(desc), "FAILED TO INIT RENDERPASS", DO_NOTHING);
        renderpass.owner = device;
        return true;
    }
};

class framebuffer_t
{
    vk_handle::framebuffer handle;
    std::vector<vk_handle::image_view> attachments;
public:
    operator VkFramebuffer() const {return handle;}
    vk_handle::description::framebuffer_desc get_description(){return handle.get_description();}

    framebuffer_t& operator =(const framebuffer_t&) = delete;
    framebuffer_t(const framebuffer_t&)             = delete;

    struct description
    {
        std::vector<vk_handle::image_view> attachments;
        uint32_t width, height;
        std::optional<uint32_t> layers;
        std::optional<VkFramebufferCreateFlags> flags;
    };
    uint32_t width, height;
    
    framebuffer_t(description description, std::shared_ptr<const renderpass_t> renderpass)
    {
        create_framebuffer(renderpass, description, *this);
        this->width  = description.width;
        this->height = description.height;
        this->attachments = description.attachments;
    }

    std::shared_ptr<const renderpass_t> owner;
    ~framebuffer_t()
    {
        for(auto& x : attachments)
            x.destroy();
        handle.destroy();
    }
private:
    static bool create_framebuffer(std::shared_ptr<const renderpass_t> renderpass, description description,
    framebuffer_t& framebuffer, bool throws = true)
    {
        vk_handle::description::framebuffer_desc desc{};

        EXIT_IF(description.attachments.empty(), "EMPTY FRAMEBUFFE ATTACHMENT VECTOR", DO_NOTHING)

        desc.attachments.reserve(description.attachments.size());
        for(auto att : description.attachments)
            desc.attachments.push_back(att);
        desc.flags  = description.flags;
        desc.height = description.height, desc.width = description.width;
        desc.layers = description.layers;

        framebuffer.owner = renderpass;
        desc.renderpass = *framebuffer.owner;
        desc.parent     = *framebuffer.owner->owner;

        EXIT_IF(framebuffer.handle.init(desc), "FAILED TO INIT FRAMEBUFFER", DO_NOTHING);

        return true;
    }

};

class graphics_pipeline_t
{
    vk_handle::graphics_pipeline handle;
    vk_handle::pipeline_layout layout;
    VkPipelineCache pipeline_cache;
public:
    typedef vk_handle::description::graphics_pipeline_desc description;

    const description record;

    operator VkPipeline()const {return handle.handle[0];}

    graphics_pipeline_t& operator=(const graphics_pipeline_t&) = delete;
    graphics_pipeline_t(const graphics_pipeline_t&) = delete;

    graphics_pipeline_t(description description, std::shared_ptr<const renderpass_t> renderpass) : record(description)
    {
        layout.init({*renderpass->owner});
        description.pipeline_layout = layout;
        description.parent = *renderpass->owner;
        create_graphics_pipeline(*this, description);
        layout.destroy();
    }
    std::shared_ptr<const renderpass_t>     owner;

    ~graphics_pipeline_t()
    {
        handle.destroy();
    }
private:

    static bool create_graphics_pipeline(graphics_pipeline_t& pipeline, description desc, bool throws = true)
    {
        EXIT_IF(pipeline.handle.init({desc}), "FAILED TO INIT GRAPHICS PIPELINE", DO_NOTHING);
        return true;
    }

};

class shader_module_t
{
    vk_handle::shader_module handle;
public:
    typedef vk_handle::description::shader_module_desc description;

    operator VkShaderModule()
    {
        return handle;
    }

    shader_module_t& operator=(const shader_module_t&) = delete;
    shader_module_t(const shader_module_t&) = delete;

    description record;

    shader_module_t() = delete;

    shader_module_t(description description, std::shared_ptr<const device_t> device) : record(description), owner(device)
    {
        description.parent = *device;
        create_shader_module(*this, description);
    }

    std::shared_ptr<const device_t> owner;

    ~shader_module_t()
    {
        handle.destroy();
    }
private:

    static bool create_shader_module(shader_module_t& module, description desc, bool throws = true)
    {
        EXIT_IF(module.handle.init(desc), "FAILED TO INIT SHADER MODULE", DO_NOTHING);

        return true;
    }

};

class command_pool_t
{
    vk_handle::cmd_pool handle;
public:
    typedef vk_handle::description::cmd_pool_desc description;

    operator VkCommandPool() const {return handle;}

    command_pool_t& operator=(const command_pool_t&) = delete;
    command_pool_t(const command_pool_t&) = delete;

    description record;

    command_pool_t(description description, std::shared_ptr<const device_t> device, device_t::queue_t queue)
    {
        owner = device;
        record = description;

        description.parent = *device;
        description.queue_fam_index = queue.fam_idx;
        create_command_pool(*this, description);
    }

    std::shared_ptr<const device_t> owner;
    device_t::queue_t owning_queue;
    ~command_pool_t()
    {
        handle.destroy();
    }
private:

    static bool create_command_pool(command_pool_t& cmd_pool, description desc, bool throws = true)
    {
        EXIT_IF(cmd_pool.handle.init(desc), "FAILED TO INIT CMDPOOL", DO_NOTHING);

        return true;
    }

};

struct command_buffer_t
{
    std::shared_ptr<vk_handle::cmd_buffers> handle{make_shared<vk_handle::cmd_buffers>()};
    std::shared_ptr<const command_pool_t> owner;
    // primary or secondary (always use primary)
    VkCommandBufferLevel level; 
    private : 
    uint32_t count = 1;
    friend bool create_command_buffer(command_buffer_t&, bool);
};

class fence_t
{
    vk_handle::fence handle;
public:
    std::shared_ptr<const device_t> owner;

    operator VkFence() const {return handle;}

    fence_t& operator =(const fence_t&) = delete;
    fence_t(const fence_t&) = delete;

    fence_t() = delete;

    typedef vk_handle::description::fence_desc description;
    description record;
    fence_t(description description, std::shared_ptr<const device_t> device) : owner(device), record(description)
    {
        description.parent = *owner;
        create_fence(*this, description);
    }
    ~fence_t()
    {
        handle.destroy();
    }
private:
    static bool create_fence(fence_t& fence, description desc, bool throws = true)
    {
        EXIT_IF(fence.handle.init(desc), "FAILED TO INIT FENCE", DO_NOTHING);

        return true;
    }

};

class semaphore_t
{
    vk_handle::semaphore handle;
public:
    typedef vk_handle::description::semaphore_desc description;

    operator VkSemaphore () const {return handle;}

    semaphore_t& operator =(const semaphore_t&) = delete;
    semaphore_t(const semaphore_t&) = delete;

    semaphore_t() = delete;

    description record;
    semaphore_t(description description, std::shared_ptr<const device_t> device) : record(description)
    {
        description.parent = *device;
        create_semaphore(*this, description);
    }
    std::shared_ptr<const device_t> owner;

    ~semaphore_t()
    {
        handle.destroy();
    }

private:

    static bool create_semaphore(semaphore_t& semaphore, description desc, bool throws = true)
    {
        EXIT_IF(semaphore.handle.init(desc), "FAILED TO INIT SEMAPHORE", DO_NOTHING);

        return true;
    }

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
namespace AiCO
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


bool create_command_buffer(command_buffer_t& cmd_buffer, bool throws = true)
{
    vk_handle::description::cmd_buffers_desc description{};
    description.buffer_count = cmd_buffer.count;
    description.cmd_pool     = *cmd_buffer.owner;
    description.level        = cmd_buffer.level;
    description.parent       = *cmd_buffer.owner->owner;
    
    EXIT_IF(cmd_buffer.handle->init(description), "FAILED TO INIT CMDBUFFERS", DO_NOTHING);

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
uint vulkan_context::copies = 0;

struct render_data_t
{
    std::shared_ptr<const graphics_pipeline_t>  graphics_pipeline;
    VkClearValue              clear_values;
};
struct frame_draw_data_t
{
    std::shared_ptr<const renderpass_t> framebuffer_renderpass;
    std::shared_ptr<const framebuffer_t>     swpch_framebuffer; 
    vk_handle::cmd_buffers                          cmd_buffer;
    vk_handle::semaphore                   swpch_img_available;
    VkRect2D                                         draw_area;
    device_t::queue_t                             submit_queue;
};
typedef std::function<bool (VkSemaphore, VkFence, const frame_draw_data_t&, const render_data_t&, const bool)>  frame_render_callback_fnc;

bool render_triangles(VkSemaphore signal_semaphore, VkFence signal_fence, const frame_draw_data_t& frame_data,
const render_data_t& data, const bool throws = true)
{
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    auto& cmd_buffer = frame_data.cmd_buffer.handle[0];

    EXIT_IF(vkBeginCommandBuffer(cmd_buffer, &begin_info), "FAILED TO BEGIN CMD BUFFER", DO_NOTHING);

    VkRenderPassBeginInfo renderpass_bi{};
    renderpass_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_bi.framebuffer     = *frame_data.swpch_framebuffer;
    renderpass_bi.clearValueCount = 1;
    renderpass_bi.pClearValues    = &data.clear_values;
    renderpass_bi.renderArea      = frame_data.draw_area;
    renderpass_bi.renderPass      = *frame_data.framebuffer_renderpass;

    vkCmdBeginRenderPass(cmd_buffer, &renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *data.graphics_pipeline);
    
    VkViewport viewport{(float)frame_data.draw_area.offset.x, (float)frame_data.draw_area.offset.y, (float)frame_data.draw_area.extent.width,
    (float)frame_data.draw_area.extent.height, 0.0, 1.0};
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);
    VkRect2D scissor{frame_data.draw_area};
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buffer);

    EXIT_IF(vkEndCommandBuffer(cmd_buffer), "FAILED TO END CMD BUFFER", DO_NOTHING);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1, submit_info.pCommandBuffers = &cmd_buffer;
    VkSemaphore wait_s = frame_data.swpch_img_available;
    submit_info.waitSemaphoreCount = 1, submit_info.pWaitSemaphores = &wait_s;
    VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &wait_stage_mask;
    VkSemaphore submit_s = signal_semaphore;
    submit_info.signalSemaphoreCount = 1, submit_info.pSignalSemaphores = &submit_s;

    EXIT_IF(vkQueueSubmit(frame_data.submit_queue, 1, &submit_info, signal_fence), "FAILED TO SUBMIT CMD BUFFER", DO_NOTHING);
    
    return true;
}

class frame
{
    class window_t
    {
    public:
        vk_handle::swapchain swapchain;
        vk_handle::surface surface;
        std::shared_ptr<const device_t> owner;

        window_t& operator =(const window_t&) = delete;
        window_t(const window_t&) = delete;

        struct description
        {
            int width, height;
            const char* title;
        };
        window_t(description description, std::shared_ptr<const device_t> owner) : record(description)
        {
            create_window(owner, record.width, record.height, record.title, *this);
        }
        
        GLFWwindow* window_ptr;

        static std::vector<vk_handle::image_view> get_window_attachments(const window_t& window, bool throws = true)
        {
            auto images = window.get_swapchain_images();
            std::vector<vk_handle::image_view> attachments;
            attachments.reserve(images.size());
            for(auto& image : images)
            {
                vk_handle::description::image_view_desc desc{};
                desc.format = window.swapchain.get_description().features.surface_format.format;
                desc.image  = image;
                desc.parent = *window.owner;
                vk_handle::image_view img_view;
                if(img_view.init(desc) && throws)
                    THROW("FAILED TO INIT SWAPCHAIN IMAGE VIEW");
                attachments.push_back(img_view);
            }
            return attachments;
        }

        vk_handle::description::surface_features get_features() const {return swapchain.get_description().features;}
        std::vector<VkImage> get_swapchain_images() const
        {
            //retrieve image handles. Remember : image count specified in swapchain creation is only a minimum!
            uint32_t swapchain_image_count;
            vkGetSwapchainImagesKHR(*owner, swapchain, &swapchain_image_count,
            nullptr);
            std::vector<VkImage> swapchain_images(swapchain_image_count);
            vkGetSwapchainImagesKHR(*owner, swapchain, &swapchain_image_count,
            swapchain_images.data());

            return swapchain_images;
        }
        bool update_swapchain(bool throws = true)
        {
            //please don't touch this function again. please.

            vk_handle::description::swapchain_desc desc{};

            desc.features = get_swapchain_features(get_swapchain_support(owner->phys_device.handle, this->surface),
            this->window_ptr);
            auto old_swapchain =  this->swapchain;
            desc.device_queues =  old_swapchain.get_description().device_queues;
            desc.old_swapchain =  old_swapchain.handle;
            desc.parent        = *this->owner;
            desc.surface       =  this->surface;

            //note that this line should discourage us from copying window_t objects around due to this sort of state change 
            this->swapchain = vk_handle::swapchain{};
            EXIT_IF(this->swapchain.init(desc), "FAILED TO UPDATE SWAPCHAIN", DO_NOTHING);

            old_swapchain.destroy();

            return true;
        }  

        ~window_t()
        {
            swapchain.destroy();
            surface.destroy();
            glfwDestroyWindow(window_ptr);
        }
    private:
        bool create_window(std::shared_ptr<const device_t> device, int width, int height, const char* title, window_t& window, bool throws = true)
        {
            //EXIT_IF(window, "INITIALIZING NON-EMPTY WINDOW", DO_NOTHING);

            using namespace vk_handle;
            using namespace vk_handle::description;

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            window.window_ptr = glfwCreateWindow(width, height, title, nullptr, nullptr);

            //the basic assumption is that srf has a valid pointer inside it, and that the underlying vulkan handle is not initialized
            //this assumption holds because this function rejects double initialization of windows

            auto& srf = window.surface;
            EXIT_IF(srf.init({VULKAN, window.window_ptr}), "FAILED TO CREATE WINDOW SURFACE", DO_NOTHING)

            swapchain_desc desc{};
            desc.surface = srf;
            desc.features = get_swapchain_features(get_swapchain_support(device->phys_device.handle, srf),
            window.window_ptr);
            desc.device_queues = device->get_description().device_queues;
            desc.parent = *device;
            EXIT_IF(window.swapchain.init(desc), "FAILED TO CREATE SWAPCHAIN", DO_NOTHING)

            window.owner = device;

            return true;
        }
        description record;
    };
    
    static std::vector<framebuffer_t::description> get_window_framebuffers_descriptions(const window_t& window)
    {
        auto attachments = window_t::get_window_attachments(window);
        std::vector<framebuffer_t::description> descriptions;
        descriptions.reserve(attachments.size());
        for(auto att : attachments)
        {
            framebuffer_t::description desc{};
            desc.height = window.get_features().extent.height, desc.width = window.get_features().extent.width;
            desc.attachments.push_back(att);
            descriptions.push_back(desc);
        }
        return descriptions;
    }
public:
    window_t window;
    
    frame(int width, int height, const char* title, uint frames_in_flight, std::shared_ptr<const renderpass_t> renderpass,
    std::shared_ptr<const device_t> device) : window({width, height, title}, device)
    {
        glfwSetWindowUserPointer(window.window_ptr, this);
        glfwSetFramebufferSizeCallback(window.window_ptr, window_resize_callback);

        command_pool_t::description cmdp_desc{};
        cmdp_desc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdp_desc.parent = *device;
        cmdp_desc.queue_fam_index = device->graphics_queue.fam_idx;
        data.cmdpool.init(cmdp_desc);

        data.idx_data.resize(frames_in_flight);
        for(auto& idx_data : data.idx_data)
        {
            fence_t::description fence_d;
            fence_d.flags  = VK_FENCE_CREATE_SIGNALED_BIT;
            fence_d.parent = *device;
            idx_data.f_rendering_finished.init(fence_d);

            idx_data.s_rendering_finished.init({*device});
            idx_data.swapchain_img_acquired.init({*device});

            vk_handle::description::cmd_buffers_desc cmdbfr_d{};
            cmdbfr_d.buffer_count = 1;
            cmdbfr_d.cmd_pool = data.cmdpool;
            cmdbfr_d.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cmdbfr_d.parent = *device;
            idx_data.cmdbuffer.init(cmdbfr_d);
        }
        
        auto descriptions = get_window_framebuffers_descriptions(window);
        data.swapchain_framebuffers.reserve(descriptions.size());
        for(auto& d : descriptions)
            data.swapchain_framebuffers.push_back(std::make_shared<const framebuffer_t>(d, renderpass));
        data.framebuffer_renderpass = renderpass;
    }
    bool draw_frames(frame_render_callback_fnc render_callback, const render_data_t& render_data)
    {
        return internal_draw_frames(data, data.idx_data.size(), render_callback, render_data);
    }
    ~frame()
    {
        //FIXME add clean-up here
    }
private:
    struct frame_data_t
    {
        struct indexed_data
        {
            vk_handle::fence       f_rendering_finished;
            vk_handle::semaphore   swapchain_img_acquired;
            vk_handle::semaphore   s_rendering_finished;
            vk_handle::cmd_buffers cmdbuffer;
        };
        vk_handle::cmd_pool cmdpool;
        std::vector<indexed_data> idx_data;
        std::vector<std::shared_ptr<const framebuffer_t>> swapchain_framebuffers;
        uint frame_idx = 0;
        std::shared_ptr<const renderpass_t> framebuffer_renderpass;
    };
    frame_data_t data;
    void update_frame()
    {
        window.update_swapchain();
        data.swapchain_framebuffers.clear();    //flush old framebuffers
        auto descriptions = get_window_framebuffers_descriptions(window);
        data.swapchain_framebuffers.reserve(descriptions.size());
        for(auto& d : descriptions)
            data.swapchain_framebuffers.push_back(std::make_shared<const framebuffer_t>(d, data.framebuffer_renderpass));

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
        auto swpch_res = vkAcquireNextImageKHR(device_handle, window.swapchain, UINT64_MAX, idx_data.swapchain_img_acquired,
        VK_NULL_HANDLE, &swpch_img_idx);
        EXIT_IF(swpch_res < 0, "FAILED TO ACQUIRE NEXT SWAPCHAIN IMAGE", DO_NOTHING)

        if(frame_resized)
        {
            update_frame();
            return true;
        }

        vkResetFences(device_handle, 1, &frame_rendered);

        frame_draw_data_t draw_data{};
        draw_data.swpch_img_available = idx_data.swapchain_img_acquired;
        draw_data.cmd_buffer = idx_data.cmdbuffer;
        draw_data.swpch_framebuffer = data.swapchain_framebuffers[swpch_img_idx];
        draw_data.framebuffer_renderpass = data.swapchain_framebuffers[swpch_img_idx]->owner;
        draw_data.draw_area.extent = {draw_data.swpch_framebuffer->width, draw_data.swpch_framebuffer->height};
        draw_data.draw_area.offset = {0, 0};
        draw_data.submit_queue = window.owner->graphics_queue; //HACK 
        render_callback(idx_data.s_rendering_finished, idx_data.f_rendering_finished,
        draw_data, render_data, throws);

        VkPresentInfoKHR swpch_present_info{};
        swpch_present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        VkSemaphore sem = idx_data.s_rendering_finished;
        swpch_present_info.waitSemaphoreCount = 1, swpch_present_info.pWaitSemaphores = &sem;
        swpch_present_info.swapchainCount = 1, swpch_present_info.pSwapchains = &window.swapchain.handle, swpch_present_info.pImageIndices = &swpch_img_idx;

        EXIT_IF(vkQueuePresentKHR(device_handle.present_queue, &swpch_present_info) < 0, "FRAME SUBMIT FAILED", DO_NOTHING);

        return true;
    }

};

//remember, this is a thin vulkan abstraction :>
int main()
{
    vulkan_context context;
    context.start();    //this will enforce correct destruction order
    std::shared_ptr<const device_t> mydev = std::make_shared<const device_t>();

    constexpr uint FRAMES_IN_FLIGHT = 2;

    renderpass_t::description desc;
    {
        desc.attachments.resize(1);
        desc.attachments[0].format = mydev->phys_device.get_surface_features().surface_format.format;
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
        desc.subpass_descriptions[0].color_attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //layout during subpass

        desc.subpass_dependencies.resize(1);
        desc.subpass_dependencies[0].dependencyFlags = 0;   
        desc.subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        desc.subpass_dependencies[0].dstSubpass = 0;
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
        desc.subpass_dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;            //wait for nothing
        desc.subpass_dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//this is where the color attachment load op and store op happens
        desc.subpass_dependencies[0].srcAccessMask = 0; //no access flags
        desc.subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    std::shared_ptr<const renderpass_t> renderpass = std::make_shared<const renderpass_t>(desc, mydev);

    frame myframe(100, 100, "t", FRAMES_IN_FLIGHT, renderpass, mydev);

    std::shared_ptr<const graphics_pipeline_t> triangle_pipeline;
    graphics_pipeline_t::description triangle_pipeline_d;
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

        triangle_pipeline_d.renderpass = *renderpass;
        triangle_pipeline_d.subpass_index = 0;
        triangle_pipeline_d.pipeline_cache = VK_NULL_HANDLE;
        triangle_pipeline_d.rasterization_info.polygon_mode = VK_POLYGON_MODE_FILL;
        triangle_pipeline_d.rasterization_info.rasterization_discard = VK_FALSE;
        triangle_pipeline_d.rasterization_info.depth_bias_enable = VK_FALSE;
        triangle_pipeline_d.rasterization_info.depth_clamp_enable = VK_FALSE;
        triangle_pipeline_d.rasterization_info.front_face = VK_FRONT_FACE_CLOCKWISE;
        triangle_pipeline_d.rasterization_info.cull_mode = VK_CULL_MODE_NONE;

        //these need to stay alive until the graphics pipeline is created!
        shader_module_t::description frag_d, vert_d;
        std::vector<char> fragment_code, vertex_code;
        read_binary_file({"shaders/"}, "triangle_no_input_frag.spv", fragment_code);
        read_binary_file({"shaders/"}, "triangle_no_input_vert.spv", vertex_code);
        frag_d.byte_code = fragment_code, vert_d.byte_code = vertex_code;
        frag_d.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        vert_d.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_module_t frag(frag_d, mydev), vert(vert_d, mydev);

        triangle_pipeline_d.shader_stages_info.resize(2);
        triangle_pipeline_d.shader_stages_info[0].module = vert;
        triangle_pipeline_d.shader_stages_info[0].stage  = vert.record.stage;
        triangle_pipeline_d.shader_stages_info[1].module = frag;
        triangle_pipeline_d.shader_stages_info[1].stage  = frag.record.stage;
        triangle_pipeline_d.shader_stages_info[0].entry_point = "main";
        triangle_pipeline_d.shader_stages_info[1].entry_point = "main";

        triangle_pipeline_d.viewport_state_info.scissors.resize(1);
        triangle_pipeline_d.viewport_state_info.viewports.resize(1);

        triangle_pipeline_d.vertex_input_info.attrib_descriptions.resize(0);
        triangle_pipeline_d.vertex_input_info.binding_descriptions.resize(0);

        triangle_pipeline = std::make_shared<const graphics_pipeline_t>(triangle_pipeline_d, renderpass);
    }

    render_data_t render_data{};
    render_data.clear_values.color = {0.0, 0.0, 0.0, 1.0};
    render_data.clear_values.depthStencil = VkClearDepthStencilValue{};
    render_data.graphics_pipeline = triangle_pipeline;

    while(!glfwWindowShouldClose(myframe.window.window_ptr))
    {
        glfwPollEvents();
        myframe.draw_frames(render_triangles, render_data);
    }
    vkDeviceWaitIdle(*mydev);
}