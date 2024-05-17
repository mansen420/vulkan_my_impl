#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <optional>
#include <thread>

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
struct vulkan_communicaion_instance_init_info
{
    window_info window_parameters;
    const char* app_name;
};


/************************************************HELPER FUNCTIONS, PRIVATE STRUCTS************************************************/

struct queue_indices
{
    std::optional<uint32_t> graphics_queue_index, present_queue_index;
};
struct vk_extension_info
{
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
};
//Attempts to create vulkan instance in instance_ref.
//Also checks for extension and layer support, throwing a std::runtime_error in case of failure.
VkResult init_vk_instance(VkInstance& instance_ref, vk_extension_info ext_info, const VkApplicationInfo app_info, void* next_ptr)
{
    //first, check for extension and layer support
    std::function<bool(size_t available_property_count, const char** available_names, const char** required_names,
    size_t required_name_count)> check_support;
    check_support = [](size_t available_property_count, const char** available_names, const char** required_names,
    size_t required_name_count) -> bool
    {
        if(required_name_count == 0)
        {
            return true;
        }
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
bool is_adequate(VkPhysicalDevice phys_device)
{
    queue_indices indices = get_queue_indices(phys_device);

    return indices.graphics_queue_index.has_value();
}
/************************************************MODULES************************************************/
class destroyable
{
public:
    virtual void destroy() = 0;
};
//parent kills children before it kills itself : generic data structure
class instance : public destroyable
{
public:
    VkInstance handle;
    std::vector<destroyable*> children;
    void destroy() override final
    {
        for(auto& child : children)
            child->destroy();
        vkDestroyInstance(handle, nullptr);
    }
};
class debug_messenger : public destroyable
{
public:
    debug_messenger(instance* parent){this->parent = parent; parent->children.push_back(this);}
    instance* parent;
    VkDebugUtilsMessengerEXT handle;
    void destroy() override final
    {
        destroy_debug_messenger(parent->handle);
    }
    void destroy_debug_messenger(VkInstance instance_handle)
    {
        if(!DEBUG_MODE)
            return;
        auto fun = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance_handle, "vkDestroyDebugUtilsMessengerEXT");
        if(fun != nullptr)
            fun(instance_handle, handle, nullptr);
        else
            throw std::runtime_error("Failed to find function pointer \"vkDestroyDebugUtilsMessengerEXT.\"");
    }
};
//only call the vulkan API here
class vulkan_communication_instance
{
public:

    //run this once at the start
    void init(vulkan_communicaion_instance_init_info init_info)
    {
        GLFW_INTERFACE.init(init_info.window_parameters);

        init_instance(init_info.app_name);
        if(DEBUG_MODE)
            init_validation_layer();
        init_phys_device();
    }
    //run this inside the render loop
    void invoke()
    {

    }
    //run this once at the end
    //note that a vulkan_communication_layer object can not be restarted after termination
    void terminate()
    {
        destroy_debug_messenger();
        vkDestroyInstance(INSTANCE, nullptr);
        GLFW_INTERFACE.terminate();
    }
private:
//placeholder
    vk_extension_info get_extension_info()
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

        init_vk_instance(INSTANCE, get_extension_info(), get_app_info(app_name), ext_ptr);
    }
    void init_phys_device()
    {
        uint32_t phys_devices_count;
        vkEnumeratePhysicalDevices(INSTANCE, &phys_devices_count, nullptr);
        std::vector<VkPhysicalDevice> phys_devices(phys_devices_count);
        vkEnumeratePhysicalDevices(INSTANCE,&phys_devices_count, phys_devices.data());

        for( const auto& device : phys_devices)
        {
            if(is_adequate(device))
            {
                PHYS_DEVICE = device;
                break;
            }
        }
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
    void init_validation_layer()
    {
        auto create_info = get_debug_create_info();

        auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        INSTANCE, "vkCreateDebugUtilsMessengerEXT");
        if(fun == nullptr)
            throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");
        if(fun(INSTANCE, &create_info, nullptr, &DEBUG_MESSENGER) != VK_SUCCESS)
            throw std::runtime_error("Failed to create debug util messenger object.");
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback_fun(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, 
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
    void destroy_debug_messenger()
    {
        if(!DEBUG_MODE)
            return;
        auto fun = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        INSTANCE, "vkDestroyDebugUtilsMessengerEXT");
        if(fun != nullptr)
            fun(INSTANCE, DEBUG_MESSENGER, nullptr);
        else
            throw std::runtime_error("Failed to find function pointer \"vkDestroyDebugUtilsMessengerEXT.\"");
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
    private:
        GLFWwindow* WINDOW_PTR;
    };
    GLFW_window_interface GLFW_INTERFACE;


    VkInstance                      INSTANCE;
    VkPhysicalDevice             PHYS_DEVICE;
    VkDebugUtilsMessengerEXT DEBUG_MESSENGER;
};


int main()
{
    vulkan_communication_instance instance;
    vulkan_communicaion_instance_init_info init_info{{800, 600, "Vulkan Prototype"}, "Vulkan Prototype"};

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