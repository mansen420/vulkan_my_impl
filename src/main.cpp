#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <optional>
#include <thread>
#include <type_traits>

/************************************************GLOBALS************************************************/

#define APP_NAME "Vulkan Prototype"

#ifdef NDEBUG   //make sure to use the correct CMAKE_BUILD_TYPE!
    const bool DEBUG_MODE = false;
#else
    const bool DEBUG_MODE = true;
#endif

std::ostream& ENG_LOG     = std::cout;
std::ostream& ENG_ERR_LOG = std::cerr;

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

/************************************************HELPER FUNCTIONS, PRIVATE STRUCTS************************************************/

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback_fun(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, 
VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data, void* p_user_data)
{
    const char* message_severity_text;
    switch (message_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        message_severity_text = "WARNING";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        message_severity_text = "ERROR";
    default:
        message_severity_text = "UNDETERMINED SEVERITY";
        break;
    }
    ENG_ERR_LOG << "VALIDATION LAYER : " << p_callback_data->pMessage << " (SEVERITY : "<< message_severity_text << ')'
    << std::endl;
    return VK_FALSE;
}
    
struct queue_indices
{
    std::optional<uint32_t> graphics_queue_index, present_queue_index;
};
struct vk_extension_info
{
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
};
std::vector<const char*> get_extension_info(VkPhysicalDevice device)
{
    std::vector<const char*> required_extension_names;

    required_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    vk_extension_info info{};
    info.extensions = required_extension_names;

    return required_extension_names;
}
VkApplicationInfo get_app_info(const char* app_name)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_0;
    app_info.engineVersion = VK_MAKE_VERSION(1.0, 0.0, 0.0);
    app_info.applicationVersion = VK_MAKE_VERSION(1.0, 0.0, 0.0);
    app_info.pApplicationName = app_name;
    return app_info;
}
VkDebugUtilsMessengerCreateInfoEXT get_debug_create_info()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};

    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback_fun;
    create_info.pUserData = nullptr;

    return create_info;
}
queue_indices get_queue_indices(VkPhysicalDevice phys_device)
{
    queue_indices result;

    uint32_t property_count;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &property_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(property_count);
    for(size_t i = 0; i < property_count; i++)
    {
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            result.graphics_queue_index = (uint32_t) i;

    }
    return result;
}
std::vector<VkPhysicalDevice> get_physical_devices(VkInstance instance)
{
    uint32_t phys_devices_count;
    vkEnumeratePhysicalDevices(instance, &phys_devices_count, nullptr);
    std::vector<VkPhysicalDevice> phys_devices(phys_devices_count);
    vkEnumeratePhysicalDevices(instance,&phys_devices_count, phys_devices.data());
    return phys_devices;
}


bool check_support(size_t available_name_count, const char** available_names, const char** required_names, size_t required_name_count)
{
    if(required_name_count == 0)
        return true;
    if(available_property_count == 0)
    {
        ENG_LOG << "WARNING : using a zero-sized array\n";
        return false;
    }
    if(available_names == nullptr || required_names == nullptr)
    {
        ENG_ERR_LOG << "WARNING : using nullptr\n";
        return false;
    }
    
    bool result = true;
    for(size_t i = 0; i < required_name_count; i++)
    {
        bool found = false;
        for(size_t j = 0; j < available_property_count; j++)
        {
            if(strcmp(required_names[i], available_names[j]) == 0)
            {
                ENG_LOG << required_names[i] << " SUPPORTED.\n";
                found = true;
                break;
            }
        }
        result &= found;
        if(!found)
            ENG_LOG << required_names[i] << " NOT SUPPORTED.\n";
    }
    return result;
};
bool check_support(std::vector<const char*> available_names, std::vector<const char*> required_names)
{
    return check_support(available_names.size(), available_names.data(), required_names.data(), required_names.size());
}

//Attempts to create vulkan instance in instance_ref.
//Also checks for extension and layer support, throwing a std::runtime_error in case of failure.
VkResult init_vk_instance(VkInstance& instance_ref, vk_extension_info ext_info, const VkApplicationInfo app_info, void* next_ptr)
{
    //first, check for extension and layer support
    uint32_t instance_property_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
    nullptr);
    VkExtensionProperties instance_properties[instance_property_count];
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count, instance_properties);

    const char* instance_extension_names[instance_property_count];
    for(size_t i = 0; i < instance_property_count; i++)
        instance_extension_names[i] = instance_properties[i].extensionName;

    if(!check_support((size_t) instance_property_count, instance_extension_names, ext_info.extensions.data(), ext_info.extensions.size()))
        throw std::runtime_error("Failed to find required instance extensions");
    
    uint32_t instance_layer_count;
    vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
    VkLayerProperties instance_layer_properties[instance_layer_count];
    vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties);

    const char* instance_layer_names[instance_layer_count];
    for(size_t i = 0; i < instance_layer_count; i++)
        instance_layer_names[i] = instance_layer_properties[i].layerName;

    if(!check_support((size_t) instance_layer_count, instance_layer_names, ext_info.layers.data(), ext_info.layers.size()))
        throw std::runtime_error("Failed to find required instance layers");

    //create instance 
    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = static_cast<uint32_t>(ext_info.extensions.size());
    instance_create_info.ppEnabledExtensionNames = ext_info.extensions.data();
    instance_create_info.enabledLayerCount = static_cast<uint32_t>(ext_info.layers.size());
    instance_create_info.ppEnabledLayerNames = ext_info.layers.data();
    instance_create_info.pNext = next_ptr;
    
    return vkCreateInstance(&instance_create_info, nullptr, &instance_ref);
}

std::vector<const char*> get_properties(VkPhysicalDevice device)
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, properties.data());
//HACK the above calls could fail
    std::vector<const char*> property_names(count);
    for (const auto& property : properties)
        property_names.push_back(property.extensionName);
    return property_names;
}
bool is_complete(queue_indices indices)
{
    return indices.graphics_queue_index.has_value() && indices.present_queue_index.has_value();
}
bool is_adequate(VkPhysicalDevice phys_device)
{
    queue_indices indices = get_queue_indices(phys_device);

    std::vector available_ext = get_properties(phys_device);
    std::vector required_ext  = get_extension_info(phys_device);

    bool extensions_supported = check_support(available_ext, required_ext);

    return extensions_supported;
}

void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger)
{
    if(!DEBUG_MODE)
        return;
    auto fun = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
    if(fun != nullptr)
        fun(instance, debug_messenger, nullptr);
    else
        throw std::runtime_error("Failed to find function pointer \"vkDestroyDebugUtilsMessengerEXT.\"");
}
/************************************************MODULES************************************************/

class destroyable
{
public:
    virtual void destroy() = 0;
};

template <class handle_t> class vk_object : virtual public destroyable
{
protected:
    virtual void free_obj(){};
public:
    handle_t handle = VK_NULL_HANDLE;
    virtual void destroy() override final 
    {
        if(handle == VK_NULL_HANDLE)
            return;
        free_obj();
        handle = VK_NULL_HANDLE;
    };
    virtual ~vk_object(){destroy();}
};

class parent : virtual public destroyable
{
protected:
    virtual void destroy_children() final
    {
        for(auto& child : children)
            child->destroy();
    }
    std::vector<destroyable*> children;
public:
    void add_child(destroyable* child)
    {
        children.push_back(child);
    }
};
template <class parent_t, std::enable_if_t<std::is_base_of<parent, parent_t>::value && !std::is_same<parent_t, parent>::value, int> = 0 >
class child : virtual public destroyable
{
public:
    child(parent_t* parent_ptr) 
    {
        this->parent_ptr = parent_ptr;
        parent* p = (parent*)parent_ptr;
        p->add_child(this);
    }
    parent_t* parent_ptr;
};

class vk_instance        : public vk_object<VkInstance>              , public parent
{
public:
    virtual void free_obj() override final
    {
        destroy_children();
        vkDestroyInstance(handle, nullptr);
    }
};
class vk_debug_messenger : public vk_object<VkDebugUtilsMessengerEXT>, public child<vk_instance>
{
public:
    vk_debug_messenger(vk_instance* parent) : child<vk_instance>(parent) {}
    virtual void free_obj() override final{destroy_debug_messenger(parent_ptr->handle, this->handle);}
};
class vk_surface         : public vk_object<VkSurfaceKHR>            , public child<vk_instance>
{
public:
    vk_surface(vk_instance* parent) : child<vk_instance>(parent) {}
    virtual void free_obj(){vkDestroySurfaceKHR(parent_ptr->handle, this->handle, nullptr);}
};
typedef vk_object<VkPhysicalDevice> vk_phys_device;


//only call the vulkan API here
class vulkan_communication_instance
{
public:
    //run this once at the start
    void init(vulkan_communication_instance_init_info init_info)
    {
        GLFW_INTERFACE.init(init_info.window_parameters);

        init_instance(init_info.app_name);
        if(DEBUG_MODE)
            init_validation_layer();
        init_phys_device();

        static vk_surface surface_obj(&INSTANCE_obj);
        if (GLFW_INTERFACE.init_vk_surface(INSTANCE, surface_obj.handle) != VK_SUCCESS)
            throw std::runtime_error("Failed to initialize vulkan window surface.");
        DESTROY_QUEUE.push_back(&surface_obj);
    }
    //run this inside the render loop
    void invoke()
    {

    }
    //run this once at the end
    //note that a vulkan_communication_layer object can not be restarted after termination
    void terminate()
    {
        const size_t SIZE = DESTROY_QUEUE.size();
        for(size_t i = 0; i < SIZE; i++)
            DESTROY_QUEUE[SIZE-i-1]->destroy();

        GLFW_INTERFACE.terminate();
    }
private:
    static vk_extension_info get_extension_info(VkInstance instance)
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

        vk_extension_info info{};
        info.extensions = required_extension_names;
        info.layers     =     required_layer_names;

        return info;
    }

    void init_instance(const char* app_name)
    {
        auto create_info = get_debug_create_info();
        void* ext_ptr = &create_info;

        if(!DEBUG_MODE)
            ext_ptr = nullptr;

        init_vk_instance(INSTANCE, get_extension_info(INSTANCE), get_app_info(app_name), ext_ptr);
        DESTROY_QUEUE.push_back(&INSTANCE_obj);
    }
    void init_phys_device()
    {
        const auto phys_devices = get_physical_devices(INSTANCE);

        std::vector<VkPhysicalDevice> candidates;
        for( const auto& device : phys_devices)
            if(is_adequate(device))
                candidates.push_back(device);

        DESTROY_QUEUE.push_back(&PHYS_DEVICE_obj);
    }
    
    void init_validation_layer()
    {
        static vk_debug_messenger debug_messenger(&INSTANCE_obj);

        auto create_info = get_debug_create_info();

        auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        INSTANCE, "vkCreateDebugUtilsMessengerEXT");
        if(fun == nullptr)
            throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");
        if(fun(INSTANCE, &create_info, nullptr, &debug_messenger.handle) != VK_SUCCESS)
            throw std::runtime_error("Failed to create debug util messenger object.");

        DESTROY_QUEUE.push_back(&debug_messenger);   
    }

    //only call GLFW functions here
    class GLFW_window_interface
    {
    public:
        void init(window_info window_parameters = window_info())
        {
            glfwInit();
            //we assume this is a vulkan application
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            WINDOW_PTR = glfwCreateWindow(window_parameters.width, window_parameters.height, window_parameters.title, nullptr, nullptr);
        };
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
    private:
        GLFWwindow* WINDOW_PTR;
    };
    GLFW_window_interface GLFW_INTERFACE;

    vk_instance                   INSTANCE_obj;
    vk_phys_device             PHYS_DEVICE_obj;

    VkInstance&       INSTANCE    =    INSTANCE_obj.handle;
    VkPhysicalDevice& PHYS_DEVICE = PHYS_DEVICE_obj.handle;

    std::vector<destroyable*>   DESTROY_QUEUE;
};


int main()
{
    vulkan_communication_instance instance;
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

    instance.terminate();

    return 0;    
}