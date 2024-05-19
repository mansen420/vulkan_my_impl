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

struct swapchain_support
{
    VkSurfaceCapabilitiesKHR       surface_capabilities;
    std::vector<VkSurfaceFormatKHR>     surface_formats;
    std::vector<VkPresentModeKHR> surface_present_modes;
};
struct queue_indices
{
    std::optional<uint32_t> graphics_queue_index, present_queue_index;
};
struct vk_extension_info
{
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
};
std::vector<const char*> get_required_extension_names(VkPhysicalDevice device)
{
    std::vector<const char*> required_extension_names;

    required_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

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
std::vector<VkPhysicalDevice> get_physical_devices(VkInstance instance)
{
    uint32_t phys_devices_count;
    vkEnumeratePhysicalDevices(instance, &phys_devices_count, nullptr);
    std::vector<VkPhysicalDevice> phys_devices(phys_devices_count);
    vkEnumeratePhysicalDevices(instance,&phys_devices_count, phys_devices.data());
    return phys_devices;
}


bool check_support(const size_t available_name_count, const char** available_names, const char** required_names, const size_t required_name_count)
{
    if(required_name_count == 0)
        return true;
    if(available_name_count == 0)
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
        for(size_t j = 0; j < available_name_count; j++)
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

std::vector<const char*> get_extension_names(VkPhysicalDevice device)
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, properties.data());
//HACK the above calls could fail
    std::vector<const char*> property_names;
    property_names.reserve(count); //XXX NEVER EVER USE THE FILL CONSTRUCTOR IT HAS FUCKED ME TWICE NOW
    for (const auto& property : properties)
        property_names.push_back(property.extensionName);

    return property_names;
}
std::vector<VkQueueFamilyProperties> get_queue_properties(VkPhysicalDevice device)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

    return properties;
}
swapchain_support get_swapchain_support(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    swapchain_support support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface,
    &support.surface_capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface,
    &format_count, nullptr);
    if(format_count > 0)
    {
        support.surface_formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface,
        &format_count, support.surface_formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface,
    &present_mode_count, nullptr);
    if(present_mode_count > 0)
    {
        support.surface_present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface,
        &present_mode_count, support.surface_present_modes.data());
    }
    return support;
}

bool is_complete(queue_indices indices)
{
    return indices.graphics_queue_index.has_value() && indices.present_queue_index.has_value();
}
//Attempts to find a complete queue family in phys_device.
//Be warned that this function may return indices that do not pass is_complete().
queue_indices find_queue_family(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    const auto queue_families = get_queue_properties(phys_device);
    queue_indices indices;
    for(size_t i = 0; i < queue_families.size(); i++)
    {
        uint32_t i32 = static_cast<uint32_t>(i);
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics_queue_index = i32;
        VkBool32 supports_surface = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i32, surface, &supports_surface);
        if(supports_surface)
            indices.present_queue_index = i32;
        if(is_complete(indices))
            break;
    }
    return indices;
}
bool is_adequate(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    queue_indices indices = find_queue_family(phys_device, surface);

    auto avl_names = get_extension_names(phys_device);
    
    auto req_names = get_required_extension_names(phys_device);
    bool extensions_supported = check_support(avl_names, req_names);

    swapchain_support device_support = get_swapchain_support(phys_device, surface);
    
    bool supports_swapchain = !(device_support.surface_formats.empty() || device_support.surface_present_modes.empty());

    return extensions_supported && is_complete(indices) && supports_swapchain;
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

        static vk_instance instance;
        init_instance(instance.handle ,init_info.app_name);

        if(DEBUG_MODE)
            init_validation_layer(&instance);

        static vk_surface surface_obj(&instance);
        init_surface(instance.handle, surface_obj.handle);

        static vk_phys_device phys_device;
        init_phys_device(phys_device.handle, instance.handle, surface_obj.handle);

        DESTROY_QUEUE.push_back(&instance);
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
    static vk_extension_info get_required_extension_names(VkInstance instance)
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

    void init_instance(VkInstance& instance ,const char* app_name)
    {
        auto create_info = get_debug_create_info();
        void* ext_ptr = &create_info;

        if(!DEBUG_MODE)
            ext_ptr = nullptr;

        init_vk_instance(instance, get_required_extension_names(instance), get_app_info(app_name), ext_ptr);
    }
    void init_phys_device(VkPhysicalDevice& phys_device, VkInstance instance, VkSurfaceKHR surface)
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
        phys_device = candidates[0];
    }
    
    void init_validation_layer(vk_instance* instance_obj_ptr)
    {
        if(instance_obj_ptr == nullptr)
            throw std::runtime_error("nullptr received");

        static vk_debug_messenger debug_messenger(instance_obj_ptr);

        auto create_info = get_debug_create_info();

        auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance_obj_ptr->handle, "vkCreateDebugUtilsMessengerEXT");
        if(fun == nullptr)
            throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");
        if(fun(instance_obj_ptr->handle, &create_info, nullptr, &debug_messenger.handle) != VK_SUCCESS)
            throw std::runtime_error("Failed to create debug util messenger object.");

        DESTROY_QUEUE.push_back(&debug_messenger);   
    }

    void init_surface(VkInstance instance, VkSurfaceKHR& surface)
    {
        if (GLFW_INTERFACE.init_vk_surface(instance, surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to initialize vulkan surface");
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