#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <optional>
#include <type_traits>
#include <set>
#include <algorithm>

/************************************************GLOBALS************************************************/

#define APP_NAME "Vulkan Prototype"

#ifdef NDEBUG   //make sure to use the correct CMAKE_BUILD_TYPE!
    const bool DEBUG_MODE = false;
#else
    const bool DEBUG_MODE = true;
#endif

constexpr uint32_t VK_UINT32_MAX = 0xFFFFFFFF;

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
//Stores indices of queue families that support support various operations
struct queue_family_indices
{
    std::optional<uint32_t> graphics_fam_index, present_fam_index;
};
enum queue_support_flag_bits
{
    GRAPHICS_BIT = 0b0001,
    PRESENT_BIT  = 0b0010
};
//Stores family index, count, and uses of a logical device queue
struct device_queue
{
    uint32_t family_index;
    uint32_t count;
    uint32_t flags;
};
struct vk_extension_info
{
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
};
struct swapchain_features
{
    VkSurfaceFormatKHR             surface_format;
    VkPresentModeKHR                 present_mode;
    VkExtent2D                             extent;
    VkSurfaceCapabilitiesKHR surface_capabilities;
};
struct image_view_features
{
    image_view_features()
    {
        view_type = VK_IMAGE_VIEW_TYPE_2D;
        component_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        component_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        component_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        component_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;

        subresources_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresources_range.baseArrayLayer = 0;
        subresources_range.layerCount     = 1;
        subresources_range.baseMipLevel   = 0;
        subresources_range.levelCount     = 1;
    }
    image_view_features(swapchain_features swp_features) : image_view_features()
    {
        format = swp_features.surface_format.format;
    }
    VkImageViewType                  view_type;
    VkFormat                            format;
    VkComponentMapping       component_mapping;
    VkImageSubresourceRange subresources_range;
};
struct subpass_description
{
    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    std::vector<VkAttachmentReference>          color_attachment_refs;
    std::vector<VkAttachmentReference>          input_attachment_refs;
    std::optional<VkAttachmentReference> depth_stencil_attachment_ref;

    VkSubpassDescription get_subpass_description() const
    {
        VkSubpassDescription subpass_desc{};

        subpass_desc.colorAttachmentCount    =    static_cast<uint32_t>(color_attachment_refs.size());
        subpass_desc.inputAttachmentCount    =    static_cast<uint32_t>(input_attachment_refs.size());

        subpass_desc.pColorAttachments       = color_attachment_refs.data();
        subpass_desc.pInputAttachments       = input_attachment_refs.data();
        subpass_desc.pDepthStencilAttachment = depth_stencil_attachment_ref.has_value() ? &depth_stencil_attachment_ref.value() : nullptr;

        subpass_desc.pipelineBindPoint = bind_point;

        return subpass_desc;
    }
};
struct renderpass_description
{
    std::vector<VkAttachmentDescription>      attachments;
    std::vector<subpass_description> subpass_descriptions;
    std::vector<VkSubpassDescription> get_subpasses() const
    {
        std::vector<VkSubpassDescription> descriptions;
        descriptions.reserve(subpass_descriptions.size());
        for(const auto& subpass : subpass_descriptions)
            descriptions.push_back(subpass.get_subpass_description());
        return descriptions;
    }
    std::vector<VkSubpassDependency> subpass_dependencies;
};

renderpass_description get_simple_renderpass_description(swapchain_features swp_features)
{
    VkAttachmentDescription attachment{};
    attachment.initialLayout =       VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.loadOp        =     VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp       =    VK_ATTACHMENT_STORE_OP_STORE;
    attachment.samples       =           VK_SAMPLE_COUNT_1_BIT;
    attachment.format        = swp_features.surface_format.format;

    VkAttachmentReference attachment_ref{};
    attachment_ref.attachment = 0; //index
    attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    subpass_description subpass;
    subpass.color_attachment_refs.push_back(attachment_ref);

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL, dependency.dstSubpass = 0;//index
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0, dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    renderpass_description description;
    description.attachments.push_back(attachment);
    description.subpass_descriptions.push_back(subpass);
    description.subpass_dependencies.push_back(dependency);

    return description;
}

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
std::vector<device_queue> get_device_queues(queue_family_indices fam_indices)
{
    std::set<uint32_t> indices = {fam_indices.graphics_fam_index.value(), fam_indices.present_fam_index.value()};

    std::vector<device_queue> device_queues;

    for(const auto& index: indices)
    {
        device_queue queue;
        queue.count = 1, queue.family_index = index;
        if(index == fam_indices.graphics_fam_index)
            queue.flags |= GRAPHICS_BIT;
        if(index == fam_indices.present_fam_index)
            queue.flags |= PRESENT_BIT;
        device_queues.push_back(queue);
    }
    return device_queues;
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
std::vector<VkImage> get_swapchain_images(VkSwapchainKHR swapchain, VkDevice device)
{
    //retrieve image handles. Remember : image count specified in swapchain creation is only a minimum!
    uint32_t swapchain_image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count,
    nullptr);
    std::vector<VkImage> swapchain_images(swapchain_image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count,
    swapchain_images.data());

    return swapchain_images;
}

bool is_complete(queue_family_indices indices)
{
    return indices.graphics_fam_index.has_value() && indices.present_fam_index.has_value();
}
//Attempts to find a complete queue family in phys_device.
//Be warned that this function may return indices that do not pass is_complete().
queue_family_indices find_queue_family(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    const auto queue_families = get_queue_properties(phys_device);
    queue_family_indices indices;
    for(size_t i = 0; i < queue_families.size(); i++)
    {
        uint32_t i32 = static_cast<uint32_t>(i);
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics_fam_index = i32;
        VkBool32 supports_surface = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i32, surface, &supports_surface);
        if(supports_surface)
            indices.present_fam_index = i32;
        if(is_complete(indices))
            break;
    }
    return indices;
}
bool is_adequate(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    queue_family_indices indices = find_queue_family(phys_device, surface);

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

//Be warned that vk_objects transfer ownership of the underlying vulkan object through the copy constructor
template <class handle_t> class vk_object : virtual public destroyable
{
protected:
    virtual void free_obj(){};
public:
    mutable handle_t handle = VK_NULL_HANDLE;
    virtual void destroy() override final 
    {
        if(handle == VK_NULL_HANDLE)
            return;
        free_obj();
        handle = VK_NULL_HANDLE;
    };
    vk_object(const vk_object& copy)
    {
        this->handle =    copy.handle;
        copy.handle  = VK_NULL_HANDLE;
    }
    vk_object(){}
    vk_object(handle_t handle){this->handle = handle;}
    virtual ~vk_object(){destroy();}
};

class parent : virtual public destroyable
{
protected:
    virtual void destroy_children() final
    {
        const size_t SIZE = children.size(); 
        for(size_t i = 0; i < SIZE; i++)
            children[SIZE - i - 1]->destroy();
    }
    std::vector<destroyable*> children;
public:
    virtual ~parent() {children.clear();}
    void remove_child(destroyable* child)
    {
        auto itr = std::find(children.begin(), children.end(), child);
        children.erase(itr);
    }
    void add_child(destroyable* child)
    {
        children.push_back(child);
    }
};
template <class parent_t, std::enable_if_t<std::is_base_of<parent, parent_t>::value && !std::is_same<parent_t, parent>::value, int> = 0 >
class child : virtual public destroyable
{
public:
    child() = delete;
    child& operator= (const child& rhs)
    {
        if(this == &rhs)
            return *this;
        this->parent_ptr = rhs.parent_ptr;
        parent* p = (parent*)parent_ptr;
        p->add_child(this);
        return *this;
    }
    child& operator= (const child&& rhs)
    {
        this->parent_ptr = rhs.parent_ptr;
        parent* p = (parent*)parent_ptr;
        p->add_child(this);
        return *this;
    }
    child(const child& copy)
    {
        *this = copy;
    }
    
    child(parent_t* parent_ptr) 
    {
        this->parent_ptr = parent_ptr;
        parent* p = (parent*)parent_ptr;
        p->add_child(this);
    }
    virtual ~child()
    {
        parent* p = (parent*)parent_ptr;
        p->remove_child(this);
    }
    parent_t* parent_ptr;
};

class vk_instance        : public vk_object<VkInstance>              , public parent
{
public:
    virtual ~vk_instance() final {destroy();}
private:
    virtual void free_obj() override final
    {
        destroy_children();
        vkDestroyInstance(handle, nullptr);
    }
};
class vk_debug_messenger : public vk_object<VkDebugUtilsMessengerEXT>, public child<vk_instance>
{
    virtual void free_obj() override final{destroy_debug_messenger(parent_ptr->handle, this->handle);}
public:
    vk_debug_messenger(vk_instance* parent) : child<vk_instance>(parent) {}
    virtual ~vk_debug_messenger() final {destroy();}
};
class vk_surface         : public vk_object<VkSurfaceKHR>            , public child<vk_instance>
{
    virtual void free_obj(){vkDestroySurfaceKHR(parent_ptr->handle, this->handle, nullptr);}
public:
    vk_surface(vk_instance* parent) : child<vk_instance>(parent) {}
    virtual ~vk_surface() final {destroy();}
};
class vk_device          : public vk_object<VkDevice>                , public parent
{
public:
    std::vector<device_queue> device_queues;
    virtual ~vk_device() final {destroy();}
private:
    virtual void free_obj() override final
    {
        destroy_children();
        vkDestroyDevice(handle, nullptr);
    }
};
class vk_swapchain       : public vk_object<VkSwapchainKHR>          , public child<vk_device>
{
    virtual void free_obj() override final
    {
        vkDestroySwapchainKHR(parent_ptr->handle, handle, nullptr);
    }
public:
    swapchain_features features;
    vk_swapchain(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_swapchain() final {destroy();}
};
class vk_image_view      : public vk_object<VkImageView>             , public child<vk_device>
{
    virtual void free_obj() override final
    {
        vkDestroyImageView(parent_ptr->handle, this->handle, nullptr);
    }
public:
    image_view_features features;
    vk_image_view(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_image_view() final {destroy();}
};
class vk_renderpass      : public vk_object<VkRenderPass>            , public child<vk_device>
{
    virtual void free_obj() override final
    {
        vkDestroyRenderPass(parent_ptr->handle, this->handle, nullptr);
    }
public:
    renderpass_description description;
    vk_renderpass(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_renderpass() final {destroy();}
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
        {
            static vk_debug_messenger debug_messenger(&instance);
            init_validation_layer(instance.handle, debug_messenger.handle);
        }

        static vk_surface surface(&instance);
        init_surface(instance.handle, surface.handle, GLFW_INTERFACE);

        static vk_phys_device phys_device;
        init_phys_device(phys_device.handle, instance.handle, surface.handle);

        DESTROY_QUEUE.push_back(&instance);

        static vk_device device;
        device.device_queues = get_device_queues(find_queue_family(phys_device.handle, surface.handle));
        init_device(phys_device.handle, device.device_queues, device.handle);

        static vk_swapchain swapchain(&device);
        swapchain_support swp_support = get_swapchain_support(phys_device.handle, surface.handle);
        swapchain.features = get_swapchain_features(swp_support, GLFW_INTERFACE);
        init_swapchain(device.handle, device.device_queues, surface.handle, swapchain.handle, swapchain.features);

        auto swapchain_images = get_swapchain_images(swapchain.handle, device.handle);
        static std::vector<vk_image_view> swapchain_image_views; 
        for(size_t i = 0; i < swapchain_images.size(); i++)
        {
            auto& image = swapchain_images[i];

            vk_image_view image_view(&device);

            image_view.features  = image_view_features(swapchain.features);
            auto result = init_image_view(image, device.handle, image_view.handle, image_view.features);
            if(result != VK_SUCCESS)
                throw std::runtime_error("Failed to create image views.");

            swapchain_image_views.push_back(image_view);
        }

        static vk_renderpass renderpass(&device);
        renderpass.description = get_simple_renderpass_description(swapchain.features);
        if(init_render_pass(device.handle, renderpass.handle, renderpass.description) != VK_SUCCESS)
            throw std::runtime_error("Failed to create renderpass.");

        DESTROY_QUEUE.push_back(&device);
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
        VkExtent2D get_window_extent()
        {
            int width, height;
            glfwGetFramebufferSize(WINDOW_PTR, &width, &height);

            VkExtent2D window_extent;
            window_extent.height = static_cast<uint32_t>(height);
            window_extent.width = static_cast<uint32_t>(width);

            return window_extent;
        }
    private:
        GLFWwindow* WINDOW_PTR;
    };
    
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
    
    static void init_instance(VkInstance& instance ,const char* app_name)
    {
        auto create_info = get_debug_create_info();
        void* ext_ptr = &create_info;

        if(!DEBUG_MODE)
            ext_ptr = nullptr;

        init_vk_instance(instance, get_required_extension_names(instance), get_app_info(app_name), ext_ptr);
    }
    static void init_phys_device(VkPhysicalDevice& phys_device, VkInstance instance, VkSurfaceKHR surface)
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
    
    static void init_validation_layer(VkInstance instance ,VkDebugUtilsMessengerEXT& debug_messenger)
    {
        auto create_info = get_debug_create_info();

        auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");

        if(fun == nullptr)
            throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");
        if(fun(instance, &create_info, nullptr, &debug_messenger) != VK_SUCCESS)
            throw std::runtime_error("Failed to create debug util messenger object.");
    }

    static void init_surface(VkInstance instance, VkSurfaceKHR& surface, GLFW_window_interface glfw_interface)
    {
        if (glfw_interface.init_vk_surface(instance, surface) != VK_SUCCESS)
            throw std::runtime_error("Failed to initialize vulkan surface");
    }
    
    static void init_device(VkPhysicalDevice phys_device, std::vector<device_queue> device_queues, VkDevice& device)
    {
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

        const float priority = 1.f;
        for(const auto& queue : device_queues)
        {
            VkDeviceQueueCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            create_info.queueCount = queue.count, create_info.queueFamilyIndex = queue.family_index;
            create_info.pQueuePriorities = &priority;

            queue_create_infos.push_back(create_info);
        }

        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        auto extensions = ::get_required_extension_names(phys_device); //call function from global scope
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        device_create_info.ppEnabledExtensionNames = extensions.data();

        device_create_info.enabledLayerCount = 0;

        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();

        VkPhysicalDeviceFeatures no_features{};
        device_create_info.pEnabledFeatures = &no_features;

        if(vkCreateDevice(phys_device, &device_create_info, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("Failed to create device.");
    }
    
    static swapchain_features get_swapchain_features(swapchain_support swp_support, GLFW_window_interface glfw_interface)
    {
        VkSurfaceFormatKHR surface_format = swp_support.surface_formats[0];
        for(const auto& surface_format_candidate : swp_support.surface_formats)
            if(surface_format_candidate.format == VK_FORMAT_B8G8R8_SRGB && surface_format_candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                surface_format = surface_format_candidate;
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for(const auto& present_mode_candidate : swp_support.surface_present_modes)
            if(present_mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR)
                present_mode = present_mode_candidate;
        
        VkExtent2D extent = determine_extent(swp_support.surface_capabilities, glfw_interface);
        return {surface_format, present_mode, extent, swp_support.surface_capabilities};
    }
    static void init_swapchain(VkDevice device, std::vector<device_queue> device_queues, VkSurfaceKHR surface, VkSwapchainKHR& swapchain, swapchain_features features)
    {
        VkSurfaceFormatKHR& surface_format = features.surface_format;
        VkExtent2D&                 extent =         features.extent;
        VkPresentModeKHR&     present_mode =   features.present_mode;
        VkSurfaceCapabilitiesKHR& surface_capabilities = features.surface_capabilities;

        uint32_t min_image_count = surface_capabilities.minImageCount + 1;
        if(surface_capabilities.maxImageCount > 0 && min_image_count > surface_capabilities.maxImageCount)
            min_image_count = surface_capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType   =  VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface          =                             surface;
        create_info.minImageCount    =                     min_image_count;
        create_info.imageFormat      =               surface_format.format;
        create_info.imageColorSpace  =           surface_format.colorSpace;
        create_info.imageExtent      =                              extent;
        create_info.presentMode      =                        present_mode;
        create_info.imageArrayLayers =                                   1;
        create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        create_info.preTransform     = surface_capabilities.currentTransform;
        create_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.clipped          = VK_TRUE;
        create_info.oldSwapchain     = VK_NULL_HANDLE;

//HACK Assumption : each device has 1 graphics queue and 1 present queue, which may be the same queue
        std::set<uint32_t> sharing_families; 
        for(const auto& queue : device_queues)
        {
            if(queue.flags & GRAPHICS_BIT)
                sharing_families.insert(queue.family_index);
            if(queue.flags & PRESENT_BIT)
                sharing_families.insert(queue.family_index);
        }
        if(sharing_families.size() == 1)
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        else
        {
            create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount =    sharing_families.size(); //always 2?
            create_info.pQueueFamilyIndices   = &*sharing_families.begin();
        }
        if(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) != VK_SUCCESS)
            throw std::runtime_error("Failed to create swapchain.");
    }
    static VkExtent2D determine_extent(VkSurfaceCapabilitiesKHR surface_capabilities, GLFW_window_interface glfw_interface)
    {
        if(surface_capabilities.currentExtent.width != VK_UINT32_MAX) 
            return surface_capabilities.currentExtent;
        VkExtent2D window_extent = glfw_interface.get_window_extent();
        window_extent.height = std::clamp(window_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        window_extent.width  = std::clamp(window_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);

        return window_extent;
    }
    
    static VkResult init_image_view(VkImage image, VkDevice device, VkImageView& image_view, image_view_features features)
    {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = image;

        create_info.viewType = features.view_type;
        create_info.format   =    features.format;
        
        create_info.components = features.component_mapping;

        create_info.subresourceRange = features.subresources_range;

        return vkCreateImageView(device, &create_info, nullptr, &image_view);
    }

    static VkResult init_render_pass(VkDevice device, VkRenderPass& renderpass, renderpass_description description)
    {
        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        create_info.attachmentCount = static_cast<uint32_t>(description.attachments.size());
        create_info.subpassCount    = static_cast<uint32_t>(description.subpass_descriptions.size());
        create_info.pAttachments    = description.attachments.data();

        std::vector<VkSubpassDescription> subpasses = description.get_subpasses();
        create_info.pSubpasses = subpasses.data();

        create_info.dependencyCount = static_cast<uint32_t>(description.subpass_dependencies.size());
        create_info.pDependencies   = description.subpass_dependencies.data();

        return vkCreateRenderPass(device, &create_info, nullptr, &renderpass);
    }

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