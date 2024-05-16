#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <iostream>
#include <vector>
#include <functional>
#include <cstring>

/************************************************GLOBALS************************************************/

#define APP_NAME "Vulkan Prototype"

#ifdef NDEBUG   //make sure to use the correct CMAKE_BUILD_TYPE!
    const bool DEBUG_MODE = false;
#else
    const bool DEBUG_MODE = true;
#endif

std::ostream& ENG_LOG     = std::cout;
std::ostream& ENG_ERR_LOG = std::cerr;

/************************************************STRUCTS************************************************/

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


/************************************************HELPER FUNCTIONS************************************************/


//Attempts to create vulkan instance in instance_ref.
//Also checks for extension and layer support, throwing a std::runtime_error in case of failure.
VkResult init_vk_instance(VkInstance& instance_ref, uint32_t extension_count, const char** extension_names, uint32_t layer_count,
const char** layer_names, const VkApplicationInfo app_info)
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

    if(!check_support((size_t) instance_property_count, instance_extension_names, extension_names, (size_t)extension_count))
        throw std::runtime_error("Failed to find required instance extensions");
    
    uint32_t instance_layer_count;
    vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
    VkLayerProperties instance_layer_properties[instance_layer_count];
    vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties);

    const char* instance_layer_names[instance_layer_count];
    for(size_t i = 0; i < instance_layer_count; i++)
        instance_layer_names[i] = instance_layer_properties[i].layerName;

    if(!check_support((size_t) instance_layer_count, instance_layer_names, layer_names, (size_t)layer_count))
        throw std::runtime_error("Failed to find required instance layers");

    //create instance 
    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = extension_count, instance_create_info.ppEnabledExtensionNames = extension_names;
    instance_create_info.enabledLayerCount = layer_count, instance_create_info.ppEnabledLayerNames = layer_names;
    
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

/************************************************MODULES************************************************/


//only call the vulkan API here
class vulkan_communication_instance
{
public:

    //run this once at the start
    void init(vulkan_communicaion_instance_init_info init_info)
    {
        GLFW_INTERFACE.init(init_info.window_parameters);

        const bool& ENABLE_VALIDATION_LAYERS = DEBUG_MODE;
        std::vector<const char*> required_extension_names;
        std::vector<const char*>     required_layer_names;

        required_extension_names = GLFW_window_interface::get_glfw_required_extensions();
//HACK hardcoding 
        if(ENABLE_VALIDATION_LAYERS)
            required_layer_names.push_back("VK_LAYER_KHRONOS_validation");

        init_vk_instance(INSTANCE, static_cast<uint32_t>(required_extension_names.size()),
        required_extension_names.data(),static_cast<uint32_t>(required_layer_names.size()),
        required_layer_names.data(), get_app_info(init_info.app_name));
    }
    //run this inside the render loop
    void invoke()
    {

    }
    //run this once at the end
    //note that a vulkan_communication_layer object can not be restarted after termination
    void terminate()
    {
        vkDestroyInstance(INSTANCE, nullptr);
        GLFW_INTERFACE.terminate();
    }
private:
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

    VkInstance  INSTANCE;
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
    }
    
    return 0;    
}