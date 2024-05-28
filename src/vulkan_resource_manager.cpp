#include "volk.h"
#include "GLFW/glfw3.h" //for surface

#include "vulkan_handle.h"
#include "vulkan_handle_util.h"
#include "debug.h"

#include <map>
#include <set>
#include <algorithm>

//In theory, this module handles all communication with vulkan...(?)

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

struct window_t
{
    vk_handle::surface     surface;
    vk_handle::swapchain swapchain;
    GLFWwindow*         window_ptr;
};

struct instance_t
{
    vk_handle::instance handle;
        
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
            get_physical_device_data(device.handle, device.properties, device.features, device.memory_properties,
            device.queue_fams, device.available_extensions);
            physical_devices.push_back(device);
        }
        return physical_devices;
    }
    static bool supports_extensions(physical_device_t device, std::vector<std::string> extensions)
    {
        return check_support(device.available_extensions, extensions);
    }
    static physical_device_t pick_best_physical_device(std::vector<physical_device_t> devices)
    {
        std::map<VkDeviceSize, physical_device_t> device_memory_size; //sorted ascending
        for(size_t i = 0; i < devices.size(); ++i)
        {
            device_memory_size.insert({get_local_memory_size(devices[i]), devices[i]});
        }
        for(auto itr = device_memory_size.rbegin(); itr != device_memory_size.rend(); ++itr)
        {
            if ((*itr).second.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            || (*itr).second.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                return (*itr).second;
        }
        return (*device_memory_size.rbegin()).second;
    }
    static VkDeviceSize get_local_memory_size(physical_device_t physical_device)
    {
        VkDeviceSize device_memory_size{};
        for(uint32_t j = 0; j < physical_device.memory_properties.memoryHeapCount; ++j)
            if(physical_device.memory_properties.memoryHeaps[j].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                device_memory_size += physical_device.memory_properties.memoryHeaps[j].size;
        return device_memory_size;
    }
    
    VkPhysicalDevice                     handle;
    VkPhysicalDeviceProperties           properties;
    VkPhysicalDeviceFeatures             features;
    VkPhysicalDeviceMemoryProperties     memory_properties;
    std::vector<std::string>             available_extensions;
    std::vector<VkQueueFamilyProperties> queue_fams;

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

private:

    static void get_physical_device_data(VkPhysicalDevice phys_device, VkPhysicalDeviceProperties& properties, VkPhysicalDeviceFeatures& features,
    VkPhysicalDeviceMemoryProperties& memory_properties, std::vector<VkQueueFamilyProperties>& queue_fams, std::vector<std::string>& available_extensions)
    {
        vkGetPhysicalDeviceProperties(phys_device, &properties);
        vkGetPhysicalDeviceFeatures(phys_device, &features);
        vkGetPhysicalDeviceMemoryProperties(phys_device, &memory_properties);

        uint32_t count;
        vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &count, nullptr);
        VkExtensionProperties* ptr = new VkExtensionProperties[count];
        vkEnumerateDeviceExtensionProperties(phys_device, nullptr, &count, ptr);
        available_extensions.resize(count);
        for(size_t i = 0; i < count; ++i)
            available_extensions[i] = std::string(ptr[i].extensionName);
        delete[] ptr;

        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &count, nullptr);
        queue_fams.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &count, queue_fams.data());
    }
};


struct device_t
{
    struct family_index
    {
        uint32_t index;
        vk_handle::description::queue_support_flag_bits flags;
        operator uint32_t() const {return index;}
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
    physical_device_t phys_device;
    vk_handle::device handle;
    queue_t graphics_queue;
    queue_t transfer_queue;
    queue_t  compute_queue;
    queue_t  present_queue;

    //sets the queue members of device and the value you should pass to the handle's description
    static bool determine_queues(VkInstance instance, physical_device_t phys_device, device_t device, std::vector<vk_handle::description::device_queue>& device_queues,
    bool throws = true)
    {
        auto& queue_fams = phys_device.queue_fams;
        family_indices_t indices;

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
            vkGetPhysicalDeviceSurfaceSupportKHR(phys_device.handle, i, dummy_surface, &supports_present);
            if(supports_present)
                indices.present = family_index{i, PRESENT_BIT};
        }
        //cleanup
        glfwDestroyWindow(glfw_window);
        dummy_surface.destroy();
        //find fallbacks
        if(!indices.transfer.has_value())   //use a graphics queue
            indices.transfer = indices.graphics;
        if(!indices.compute.has_value())    //find ANY compute queue
            for(uint32_t i = 0; i < queue_fams.size(); ++i)
                if(queue_fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    using namespace vk_handle::description;
                    indices.compute = family_index{i, COMPUTE_BIT};
                }
        bool found_all_families = indices.compute.has_value() && indices.graphics.has_value() && indices.present.has_value() && indices.transfer.has_value();
        EXIT_IF(!found_all_families, "FAILED TO FIND QUEUE FAMILIES", DO_NOTHING);
        //determine device queues 
        /*
        the spec states that each device queue should refer to a unique family index.
         Since the family indices above are not necessarily unique, we must check for that
        */
        std::set<family_index> unique_indices{indices.compute.value(), indices.graphics.value(), indices.present.value(), indices.transfer.value()};
        for(const auto& index : unique_indices)
        {
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
            device_queue.family_index = index.index;
            device_queue.flags = index.flags;
            device_queue.queue_family_flags = queue_fams[index].queueFlags; //eh why not
            device_queues.push_back(device_queue);
        }
        //determine priority
        for(auto& queue : device_queues)
        {
            float priority = 0.f;
            using namespace vk_handle::description;
            if(queue.flags & GRAPHICS_BIT)
                priority += 1.f;
            if(queue.flags & TRANSFER_BIT)
                priority += 0.75f;
            if(queue.flags & COMPUTE_BIT)
                priority += 0.75f;
            if(queue.flags & PRESENT_BIT)
                priority += 0.25f;
            std::clamp(priority, 0.f, 1.f);
            queue.priority = priority;
        }
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

destruction_queue MAIN_DESTRUCTION_QUEUE, TERMINATION_QUEUE;

vk_handle::instance VULKAN;
std::vector<physical_device_t> PHYSICAL_DEVICES;

static bool INIT = false;

            /***************************************END***************************************/


        /***************************************PROCEDURES***************************************/



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


    TERMINATION_QUEUE.push(DO(MAIN_DESTRUCTION_QUEUE.flush();));

    MAIN_DESTRUCTION_QUEUE.reserve_extra(25); //reserve some space for object destruction

    //check for swapchain support by default
    auto candidates = physical_device_t::find_physical_devices(VULKAN);
    for(const auto& candidate : candidates)
        if(physical_device_t::supports_extensions(candidate, physical_device_t::get_required_extension_names(physical_device_t::SWAPCHAIN)))
            PHYSICAL_DEVICES.push_back(candidate);

    EXIT_IF(PHYSICAL_DEVICES.size() == 0, "FAILED TO FIND PHYSICAL DEVICE", terminate);

    INIT = true;
#ifdef COOL
    ENG_LOG << "Vulkan speaking, yes?\n";
    ENG_LOG << "This is vulkan , baby!\n";
#endif
    return true;
}

//a device is just something that takes rendering commands (window agnostic)
bool create_device(device_t& device, bool throws = true)
{
    device.phys_device = physical_device_t::pick_best_physical_device(PHYSICAL_DEVICES);
    vk_handle::description::device_desc description{};
    description.enabled_features   = device.phys_device.features;
    description.phys_device        = device.phys_device.handle;
    description.enabled_extensions = physical_device_t::get_required_extension_names(physical_device_t::SWAPCHAIN);
    device_t::determine_queues(VULKAN, device.phys_device, device, description.device_queues);
    EXIT_IF(device.handle.init(description), "FAILED TO INIT DEVICE", DO_NOTHING);

    return true;
}

bool create_window(int width, int height, const char* title, window_t& window, bool throws = true)
{

using namespace vk_handle;
using namespace vk_handle::description;

    window.window_ptr = glfwCreateWindow(width, height, title, nullptr, nullptr);
    surface srf;
    EXIT_IF(srf.init({VULKAN, window.window_ptr}), "FAILED TO CREATE WINDOW", DO_NOTHING)
    window.surface = srf;

}

int main()
{
    init();
    device_t mydev;
    create_device(mydev);
    terminate();
}