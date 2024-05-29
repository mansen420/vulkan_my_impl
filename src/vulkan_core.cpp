#include "volk.h"
#include "GLFW/glfw3.h" //for surface

#include "vulkan_handle_util.h"
#include "vulkan_handle_make_shared.h"
#include "debug.h"

#include <map>
#include <algorithm>


//In theory, this module handles all communication with vulkan...(?)


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
            get_physical_device_data(device.handle, device.properties, device.features, device.memory_properties,
            device.queue_fams, device.available_extensions);
            physical_devices.push_back(device);
            INFORM("Physical device determined : " << device.properties.deviceName);
        }
        return physical_devices;
    }
    static bool supports_extensions(physical_device_t device, std::vector<std::string> extensions)
    {
        INFORM(device.properties.deviceName << " : ");
        return check_support(device.available_extensions, extensions);
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
            if ((*itr).second.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            || (*itr).second.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                picked_device = (*itr).second;
                device_mem_size = (*itr).first;
            }
        }

        INFORM("Picked " << picked_device.properties.deviceName << "\nWith " << device_mem_size << " Bytes of local memory.");

        return picked_device;
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
        auto& queue_fams = device.phys_device.queue_fams;

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
        auto queue_fams = device.phys_device.queue_fams;

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

    vk_handle::description::surface_features get_features(){return swapchain->get_description().features;}
    device_t owner;
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

            /***************************************END***************************************/


        /***************************************PROCEDURES***************************************/

//TODO listen bud, you can do this. You're gonna run down every function in this file, and I mean EVERY FUNCTION, and 
//factor out whatever you can. 'k, bud?
//Also, definitely use Vulkan.hpp next time...

//TODO implement RAII classes that wrap these bad boys up?
//I think it would be good to ensure no double init or such shenanigans happen

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

/*
    This class initializes 3rd party dependencies as well as the vulkan instance,
    finds all physical devices in the system, and enables the vulkan validation layers in debug mode.

    Since all engine objects rely on these resources, they must be destroyed last.
    It is important that no engine objects are created before calling start(), and none after this class is destroyed.

    It is safe to copy this class; Only the last copy will actually destroy the context.
    It is also safe to call start() multiple times.
    There is, however, only one context at any time.
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

//remember, this is a thin vulkan abstraction :>
int main()
{
    vulkan_context context;
    context.start();    //this will enforce correct destruction order
    
}