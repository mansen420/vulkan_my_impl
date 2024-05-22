#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "read_file.h"

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

/************************************************HELPER VK API FUNCS************************************************/

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

bool check_support(const size_t available_name_count, const char* const* available_names, const char* const* required_names, const size_t required_name_count)
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
bool check_support(const std::vector<const char*> available_names, const std::vector<const char*> required_names)
{
    return check_support(available_names.size(), available_names.data(), required_names.data(), required_names.size());
}

std::vector<const char*> get_extension_names(VkPhysicalDevice device)
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> properties(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, properties.data());
//HACK the above calls could fail
    std::vector<const char*> property_names;
    property_names.reserve(count); //NEVER EVER USE THE FILL CONSTRUCTOR IT HAS FUCKED ME TWICE NOW
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

/************************************************BASE CLASSES************************************************/

class destroyable
{
public:
    virtual void destroy() = 0;
};

//Be warned that vk_objects transfer ownership of the underlying vulkan object through the copy constructor
template <class handle_t, class description_t> 
class vk_object : virtual public destroyable
{
protected:
    virtual void free_obj(){}
    virtual VkResult create_obj(description_t desc){return VK_NOT_READY;}
    mutable handle_t handle   = VK_NULL_HANDLE;
public:
    description_t description;

    //Be warned that initializing an object without a well defined description will cause fuck-ups!
    virtual VkResult init(description_t desc) final {this->description = desc; return init();}
    //Be warned that initializing an object without a well defined description will cause fuck-ups!
    virtual VkResult init() final
    {
        return create_obj(description);
    }
    
    virtual void destroy() override final 
    {
        if(empty())
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

    virtual bool     empty()      final {return handle == VK_NULL_HANDLE;}
    virtual handle_t get_handle() const final {return handle;}

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


/************************************************MODULES & DATA************************************************/


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
    bool should_close()
    {
        return glfwWindowShouldClose(WINDOW_PTR);
    }
    void poll_events()
    {
        glfwPollEvents();
    }
private:
    GLFWwindow* WINDOW_PTR;
};


struct queue_family_indices //Stores indices of queue families that support support various operations
{
    std::optional<uint32_t> graphics_fam_index, present_fam_index;
};
struct extension_info
{
    std::vector<const char*> extensions;
    std::vector<const char*> layers;
};


struct instance_desc
{
    extension_info ext_info;
    VkApplicationInfo app_info{};
    VkInstanceCreateFlags flags = 0;
    std::optional<VkDebugUtilsMessengerCreateInfoEXT> debug_messenger_ext;
    VkInstanceCreateInfo get_create_info()
    {
        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &app_info;
        info.enabledExtensionCount = static_cast<uint32_t>(ext_info.extensions.size());
        info.ppEnabledExtensionNames = ext_info.extensions.data();
        info.enabledLayerCount = static_cast<uint32_t>(ext_info.layers.size());
        info.ppEnabledLayerNames = ext_info.layers.data();
        info.flags = flags;
        info.pNext = debug_messenger_ext.has_value() ? &debug_messenger_ext.value() : nullptr;
        return info;
    }
};
struct debug_messenger_desc
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    VkDebugUtilsMessengerCreateInfoEXT get_create_info()
    {
        return create_info;
    }
};
struct surface_desc
{
    GLFW_window_interface glfw_interface;
};
enum queue_support_flag_bits
{
    GRAPHICS_BIT = 0b0001,
    PRESENT_BIT  = 0b0010
};
struct device_queue //Stores family index, count, uses, and priority of a logical device queue 
{
    uint32_t family_index;
    uint32_t count;
    uint32_t flags;
    float priority;
};
struct device_desc
{
    VkPhysicalDevice phys_device;
    std::vector<device_queue>     device_queues;
    std::vector<const char*> enabled_extensions;
    VkPhysicalDeviceFeatures   enabled_features{};

    VkDeviceCreateInfo get_create_info()
    {
        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        for(const auto& queue : device_queues)
        {
            VkDeviceQueueCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            create_info.queueCount = queue.count, create_info.queueFamilyIndex = queue.family_index;
            create_info.pQueuePriorities = &queue.priority;

            queue_create_infos.push_back(create_info);
        }

        info.enabledExtensionCount   = static_cast<uint32_t>(enabled_extensions.size());
        info.ppEnabledExtensionNames = enabled_extensions.data();

        info.enabledLayerCount = 0;

        info.pEnabledFeatures = &enabled_features;

        info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        info.pQueueCreateInfos    = queue_create_infos.data();

        return info;
    }
private:
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
};
struct swapchain_support
{
    VkSurfaceCapabilitiesKHR       surface_capabilities;
    std::vector<VkSurfaceFormatKHR>     surface_formats;
    std::vector<VkPresentModeKHR> surface_present_modes;
};
struct swapchain_features
{
    VkSurfaceFormatKHR             surface_format;
    VkPresentModeKHR                 present_mode;
    VkExtent2D                             extent;
    VkSurfaceCapabilitiesKHR surface_capabilities;
};
struct swapchain_desc
{
    swapchain_features             features;
    VkSurfaceKHR                    surface;
    std::vector<device_queue> device_queues;

    std::optional<VkSurfaceTransformFlagBitsKHR> pre_transform;
    std::optional<VkCompositeAlphaFlagBitsKHR> composite_alpha;
    std::optional<VkBool32>                            clipped;
    std::optional<VkSwapchainKHR>                old_swapchain;
    std::optional<VkImageUsageFlags>               image_usage;
    std::optional<uint32_t>                 image_array_layers;

    VkSwapchainCreateInfoKHR get_create_info()
    {
        VkSurfaceFormatKHR&       surface_format       =       features.surface_format;
        VkExtent2D&               extent               =               features.extent;
        VkPresentModeKHR&         present_mode         =         features.present_mode;
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
        create_info.imageArrayLayers =                                 image_array_layers.value_or(1);
        create_info.imageUsage       =      image_usage.value_or(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        create_info.preTransform     = pre_transform.value_or(surface_capabilities.currentTransform);
        create_info.compositeAlpha   =    composite_alpha.value_or(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
        create_info.clipped          =                                      clipped.value_or(VK_TRUE);
        create_info.oldSwapchain     =                         old_swapchain.value_or(VK_NULL_HANDLE);

//HACK Assumption : each device has 1 graphics queue and 1 present queue, which may be the same queue
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
        return create_info;
    }
private:    //this data needs to stay alive!
    std::set<uint32_t> sharing_families;
};
struct image_view_desc
{
    VkFormat format;
    VkImage   image;
    std::optional<VkImageViewType>                  view_type;
    std::optional<VkComponentMapping>       component_mapping;
    std::optional<VkImageSubresourceRange> subresources_range;
    std::optional<VkImageViewCreateFlags> flags;
    VkImageViewCreateInfo get_create_info()
    {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        
        info.viewType = view_type.value_or(VK_IMAGE_VIEW_TYPE_2D);

        VkComponentMapping identity_mapping{};
        identity_mapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        identity_mapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        identity_mapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        identity_mapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;

        info.components   = component_mapping.value_or(identity_mapping);

        VkImageSubresourceRange basic_range{};

        basic_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        basic_range.baseArrayLayer = 0;
        basic_range.layerCount     = 1;
        basic_range.baseMipLevel   = 0;
        basic_range.levelCount     = 1;

        info.subresourceRange = subresources_range.value_or(basic_range);

        info.flags  = flags.value_or(0);
        
        info.format = format;
        info.image  =  image;

        return info;
    }
};
struct subpass_description
{
    std::optional<VkPipelineBindPoint> bind_point;

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

        subpass_desc.pipelineBindPoint = bind_point.value_or(VK_PIPELINE_BIND_POINT_GRAPHICS);

        return subpass_desc;
    }
};
struct renderpass_desc
{
    std::vector<VkAttachmentDescription>      attachments;
    std::vector<subpass_description> subpass_descriptions;
    std::vector<VkSubpassDependency> subpass_dependencies;
    VkRenderPassCreateInfo get_create_info()
    {
        VkRenderPassCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        create_info.subpassCount    = static_cast<uint32_t>(subpass_descriptions.size());
        create_info.pAttachments    = attachments.data();

        subpasses = get_subpasses();
        create_info.pSubpasses = subpasses.data();

        create_info.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
        create_info.pDependencies   = subpass_dependencies.data();

        return create_info;
    }
private:
    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDescription> get_subpasses() const
    {
        std::vector<VkSubpassDescription> descriptions;
        descriptions.reserve(subpass_descriptions.size());
        for(const auto& subpass : subpass_descriptions)
            descriptions.push_back(subpass.get_subpass_description());
        return descriptions;
    }
};
struct shader_module_desc
{
    VkShaderStageFlagBits stage;
    const char* entry_point_name = "main";

    std::vector<char> byte_code;
    VkShaderModuleCreateInfo get_create_info()
    {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.pCode    = reinterpret_cast<const uint32_t*>(byte_code.data());
        create_info.codeSize = static_cast<uint32_t>(byte_code.size());
        return create_info;
    }
};
struct viewport_state_desc
{
    viewport_state_desc(){}
    viewport_state_desc(std::vector<VkViewport> viewports, std::vector<VkRect2D> scissors)
    {
        this->viewports = viewports;
        this->scissors  = scissors;
    }
    
    VkPipelineViewportStateCreateInfo get_info()
    {
        VkPipelineViewportStateCreateInfo info{};

        this->viewports = viewports;
        this->scissors  = scissors;

        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

        info.scissorCount  = static_cast<uint32_t>(scissors.size());
        info.pScissors     = scissors.data();

        info.viewportCount = static_cast<uint32_t>(viewports.size());
        info.pViewports    = viewports.data();

        return info;
    }
    
    std::vector<VkViewport>          viewports;
    std::vector<VkRect2D>             scissors;
};
struct color_blend_desc
{
    color_blend_desc(){}
    VkPipelineColorBlendStateCreateInfo get_info()
    {
        VkPipelineColorBlendStateCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        info.attachmentCount = static_cast<uint32_t>(attachment_states.size()), info.pAttachments = attachment_states.data();

        info.logicOpEnable = logic_op_enabled;
        if(info.logicOpEnable)
            info.logicOp = logic_op;
        if(blend_constants.has_value())
            for(size_t i = 0; i < 4; i++)
                info.blendConstants[i] = blend_constants.value()[i];
        return info;
    }
    VkBool32 logic_op_enabled;
    std::optional<std::array<float, 4>> blend_constants;
    VkLogicOp logic_op;
    std::vector<VkPipelineColorBlendAttachmentState> attachment_states;
};
struct dynamic_state_desc
{
    dynamic_state_desc(){}
    dynamic_state_desc(std::vector<VkDynamicState> dynamic_state_list){this->dynamic_state_list = dynamic_state_list;}
    std::vector<VkDynamicState> dynamic_state_list;
    VkPipelineDynamicStateCreateInfo get_info()
    {
        VkPipelineDynamicStateCreateInfo info{};

        info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        info.pDynamicStates = dynamic_state_list.data();
        info.dynamicStateCount = static_cast<uint32_t>(dynamic_state_list.size());

        return info;
    };
};
struct graphics_pipeline_desc
{
    VkPipelineLayout              pipeline_layout;
    VkRenderPass                       renderpass;
    uint32_t                        subpass_index;
    std::optional<VkPipelineCache> pipeline_cache;
    std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_info;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages_info;
    VkPipelineVertexInputStateCreateInfo          vertex_input_info;
    VkPipelineInputAssemblyStateCreateInfo      input_assembly_info;
    dynamic_state_desc                           dynamic_state_info;
    viewport_state_desc                         viewport_state_info;
    VkPipelineRasterizationStateCreateInfo       rasterization_info;
    VkPipelineMultisampleStateCreateInfo           multisample_info;
    color_blend_desc                               color_blend_info;
    VkGraphicsPipelineCreateInfo get_create_info()
    {
        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages_info.size()), pipeline_info.pStages = shader_stages_info.data();

        pipeline_info.pVertexInputState   =   &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly_info;

        viewport_state = viewport_state_info.get_info();
        pipeline_info.pViewportState = &viewport_state;

        pipeline_info.pRasterizationState =  &rasterization_info;
        pipeline_info.pMultisampleState   =    &multisample_info;
        pipeline_info.pDepthStencilState  =  depth_stencil_info.has_value() ? &depth_stencil_info.value() : nullptr;

        color_blend_state = color_blend_info.get_info();
        pipeline_info.pColorBlendState = &color_blend_state;

        dynamic_state = dynamic_state_info.get_info();
        pipeline_info.pDynamicState = &dynamic_state;

        pipeline_info.layout     = pipeline_layout;
        pipeline_info.renderPass =      renderpass;
        pipeline_info.subpass    =   subpass_index;

        return pipeline_info;
    }
private:
    VkPipelineViewportStateCreateInfo      viewport_state;
    VkPipelineColorBlendStateCreateInfo color_blend_state;
    VkPipelineDynamicStateCreateInfo        dynamic_state;
};
struct pipeline_layout_desc
{
    VkPipelineLayoutCreateInfo get_create_info()
    {
        VkPipelineLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        return info;        
    }
};
struct framebuffer_desc
{
    std::vector<VkImageView> attachments;
    VkRenderPass              renderpass;
    uint32_t width, height;
    uint32_t layers = 1;
    VkFramebufferCreateFlags flags;

    VkFramebufferCreateInfo get_create_info()
    {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        info.attachmentCount = static_cast<uint32_t>(attachments.size());
        info.pAttachments    = attachments.data();

        info.renderPass = renderpass;

        info.width  =  width, info.height = height;
        info.layers = layers;
        info.flags  =  flags;

        return info;
    }
};
struct cmd_pool_desc
{
    VkCommandPoolCreateFlags flags;
    uint32_t       queue_fam_index;

    VkCommandPoolCreateInfo get_create_info()
    {
        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.queueFamilyIndex = queue_fam_index;
        info.flags = flags;

        return info;
    }
};
struct cmd_buffers_desc
{
    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    uint32_t  buffer_count = 0;
    std::optional<VkCommandBufferLevel> level;
    VkCommandBufferAllocateInfo get_alloc_info()
    {
        VkCommandBufferAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = cmd_pool;
        info.commandBufferCount = buffer_count;
        info.level = level.value_or(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        return info;
    }
};
struct semaphore_desc
{
    VkSemaphoreCreateInfo get_create_info()
    {
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        return info;
    }
};
struct fence_desc
{
    std::optional<VkFenceCreateFlags> flags;
    VkFenceCreateInfo get_create_info()
    {
        VkFenceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.flags = flags.value_or(VK_FENCE_CREATE_SIGNALED_BIT);
        return info;
    }
};

VkPipelineVertexInputStateCreateInfo get_empty_vertex_input_state()
{
    VkPipelineVertexInputStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.vertexBindingDescriptionCount = 0, info.vertexAttributeDescriptionCount = 0;
    return info;
}
VkPipelineInputAssemblyStateCreateInfo get_input_assemly_state(VkPrimitiveTopology primitives, VkBool32 primitive_restart_enabled)
{
    VkPipelineInputAssemblyStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.topology = primitives, info.primitiveRestartEnable = primitive_restart_enabled;
    return info;
}
VkPipelineRasterizationStateCreateInfo get_simple_rasterization_info(VkPolygonMode polygon_mode, float line_width)
{
    VkPipelineRasterizationStateCreateInfo info{};

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    info.polygonMode = polygon_mode, info.lineWidth = line_width;

    info.depthBiasEnable = VK_FALSE,  info.rasterizerDiscardEnable = VK_FALSE, info.depthClampEnable = VK_FALSE;

    info.cullMode = VK_CULL_MODE_BACK_BIT, info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    
    return info;
}
VkPipelineMultisampleStateCreateInfo get_disabled_multisample_info()
{
    VkPipelineMultisampleStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    info.sampleShadingEnable = VK_FALSE; info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    return info;
}
VkPipelineColorBlendAttachmentState get_color_no_blend_attachment(VkColorComponentFlags color_write_mask)
{
    VkPipelineColorBlendAttachmentState state{};
    state.colorWriteMask = color_write_mask;
    state.blendEnable = VK_FALSE;
    return state;
}
color_blend_desc get_color_no_blend_state_descr(std::vector<VkPipelineColorBlendAttachmentState> states)
{
    color_blend_desc description{};

    description.attachment_states = states;
    description.logic_op_enabled = VK_FALSE;

    return description;
}
std::vector<VkPipelineShaderStageCreateInfo> get_shader_stages(std::vector<shader_module_desc> module_descriptions, std::vector<VkShaderModule> module_handles)
{
    std::vector<VkPipelineShaderStageCreateInfo> stage_infos;
    stage_infos.reserve(module_descriptions.size());
    for(size_t i = 0; i < module_descriptions.size(); i++)
    {
        VkPipelineShaderStageCreateInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage  = module_descriptions[i].stage;
        info.pName  = module_descriptions[i].entry_point_name;
        info.module = module_handles[i];
        stage_infos.push_back(info);
    }
    return stage_infos;
}

renderpass_desc get_simple_renderpass_description(swapchain_features swp_features)
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

    renderpass_desc description;
    description.attachments.push_back(attachment);
    description.subpass_descriptions.push_back(subpass);
    description.subpass_dependencies.push_back(dependency);

    return description;
}
std::vector<device_queue> get_device_queues(queue_family_indices fam_indices)
{
    std::set<uint32_t> indices = {fam_indices.graphics_fam_index.value(), fam_indices.present_fam_index.value()};

    std::vector<device_queue> device_queues;
    
    for(const auto& index: indices)
    {
        device_queue queue;
        queue.count = 1, queue.family_index = index; //FIXME chek that count isn't too big
        if(index == fam_indices.graphics_fam_index)
        {
            queue.flags |= GRAPHICS_BIT;
        }
        if(index == fam_indices.present_fam_index)
        {
            queue.flags |= PRESENT_BIT;
        }
        device_queues.push_back(queue);
    }
    return device_queues;
}
bool is_complete(queue_family_indices indices)
{
    return indices.graphics_fam_index.has_value() && indices.present_fam_index.has_value();
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

class vk_instance          : public vk_object<VkInstance              , instance_desc>         , public parent
{
public:
    virtual ~vk_instance() final {destroy();}
private:
    virtual VkResult create_obj(instance_desc desc)
    {
        const auto info = desc.get_create_info();
        return vkCreateInstance(&info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        destroy_children();
        vkDestroyInstance(handle, nullptr);
    }
};
class vk_debug_messenger   : public vk_object<VkDebugUtilsMessengerEXT, debug_messenger_desc>  , public child<vk_instance>
{
    virtual VkResult create_obj(debug_messenger_desc desc) override final
    {
        const auto info = desc.get_create_info();

        //fetch function address in runtime 
        auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        parent_ptr->get_handle(), "vkCreateDebugUtilsMessengerEXT");

        if(fun == nullptr)
            throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");

        return fun(parent_ptr->get_handle(), &info, nullptr, &this->handle);
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
    virtual void free_obj() override final{destroy_debug_messenger(parent_ptr->get_handle(), this->handle);}
public:
    vk_debug_messenger(vk_instance* parent) : child<vk_instance>(parent) {}
    virtual ~vk_debug_messenger() final {destroy();}
};
class vk_surface           : public vk_object<VkSurfaceKHR            , surface_desc>          , public child<vk_instance>
{
    virtual VkResult create_obj(surface_desc desc) override final
    {
        return desc.glfw_interface.init_vk_surface(parent_ptr->get_handle(), this->handle);
    }
    virtual void free_obj(){vkDestroySurfaceKHR(parent_ptr->get_handle(), this->handle, nullptr);}
public:
    vk_surface(vk_instance* parent) : child<vk_instance>(parent) {}
    virtual ~vk_surface() final {destroy();}
};
class vk_device            : public vk_object<VkDevice                , device_desc>           , public parent
{
public:
    virtual ~vk_device() final {destroy();}
private:
    virtual VkResult create_obj(device_desc desc) override final
    {
        auto info = desc.get_create_info();
        return vkCreateDevice(desc.phys_device, &info, nullptr, &this->handle);
    }
    virtual void     free_obj() override final
    {
        destroy_children();
        vkDestroyDevice(handle, nullptr);
    }
};
class vk_swapchain         : public vk_object<VkSwapchainKHR          , swapchain_desc>        , public child<vk_device>
{
    virtual VkResult create_obj(swapchain_desc desc) override final
    {
        auto info = desc.get_create_info();
        return vkCreateSwapchainKHR(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroySwapchainKHR(parent_ptr->get_handle(), handle, nullptr);
    }
public:
    vk_swapchain(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_swapchain() final {destroy();}
};
class vk_image_view        : public vk_object<VkImageView             , image_view_desc>       , public child<vk_device>
{
    virtual VkResult create_obj(image_view_desc desc) override final
    {
        auto info = desc.get_create_info();
        return vkCreateImageView(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroyImageView(parent_ptr->get_handle(), this->handle, nullptr);
    }
public:
    vk_image_view(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_image_view() final {destroy();}
};
class vk_renderpass        : public vk_object<VkRenderPass            , renderpass_desc>       , public child<vk_device>
{
    virtual VkResult create_obj(renderpass_desc desc)
    {
        auto info = desc.get_create_info();
        return vkCreateRenderPass(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    } 
    virtual void free_obj() override final
    {
        vkDestroyRenderPass(parent_ptr->get_handle(), this->handle, nullptr);
    }
public:
    vk_renderpass(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_renderpass() final {destroy();}
};
class vk_shader_module     : public vk_object<VkShaderModule          , shader_module_desc>    , public child<vk_device>
{
    virtual void free_obj() override final
    {
        vkDestroyShaderModule(parent_ptr->get_handle(), this->handle, nullptr);
    }
    virtual VkResult create_obj(shader_module_desc desc) override final 
    {
        const auto info = desc.get_create_info(); 
        return vkCreateShaderModule(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
public:
    vk_shader_module(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_shader_module() final {destroy();}
};
class vk_graphics_pipeline : public vk_object<VkPipeline              , graphics_pipeline_desc>, public child<vk_device>
{
    virtual VkResult create_obj(graphics_pipeline_desc desc) override final
    {
        auto info = desc.get_create_info();
        return vkCreateGraphicsPipelines(parent_ptr->get_handle(), desc.pipeline_cache.value_or(VK_NULL_HANDLE), 1, &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroyPipeline(parent_ptr->get_handle(), this->handle, nullptr);
    }
public:
    vk_graphics_pipeline(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_graphics_pipeline() final {destroy();}
};
class vk_pipeline_layout   : public vk_object<VkPipelineLayout        , pipeline_layout_desc>  , public child<vk_device>
{
    virtual VkResult create_obj(pipeline_layout_desc desc)
    {
        auto info = desc.get_create_info();
        return vkCreatePipelineLayout(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroyPipelineLayout(parent_ptr->get_handle(), this->handle,  nullptr);
    }
public:
    vk_pipeline_layout(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_pipeline_layout() final {destroy();}
};
class vk_framebuffer       : public vk_object<VkFramebuffer           , framebuffer_desc>      , public child<vk_device>
{   
    virtual VkResult create_obj(framebuffer_desc desc)
    {
        auto info = desc.get_create_info();
        return vkCreateFramebuffer(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void     free_obj() override final
    {
        vkDestroyFramebuffer(parent_ptr->get_handle(), this->handle, nullptr);
    }
public:
    vk_framebuffer(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_framebuffer() final {destroy();}
};
class vk_cmd_pool          : public vk_object<VkCommandPool           , cmd_pool_desc>         , public child<vk_device>, public parent
{
    virtual VkResult create_obj(cmd_pool_desc desc)
    {
        auto info = desc.get_create_info();
        return vkCreateCommandPool(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroyCommandPool(parent_ptr->get_handle(), this->handle, nullptr);
    }
public:
    vk_cmd_pool(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_cmd_pool() final 
    {
        destroy_children();
        destroy();
    }
};
class vk_cmd_buffers       : public vk_object<VkCommandBuffer*        , cmd_buffers_desc>      , public child<vk_device>, public child<vk_cmd_pool>
{
    virtual VkResult create_obj(cmd_buffers_desc desc) override final
    {
        handle = new VkCommandBuffer[desc.buffer_count];
        auto info = desc.get_alloc_info();
        return vkAllocateCommandBuffers(child<vk_device>::parent_ptr->get_handle(), &info, this->handle);
    }
    virtual void free_obj() override final
    {
        vkFreeCommandBuffers(child<vk_device>::parent_ptr->get_handle(), child<vk_cmd_pool>::parent_ptr->get_handle(),
        description.buffer_count, this->handle);
        delete[] handle;
    }
public:
    vk_cmd_buffers(vk_device* parent_device, vk_cmd_pool* parent_cmd_pool) : child<vk_device>(parent_device),
    child<vk_cmd_pool>(parent_cmd_pool) {}
    virtual ~vk_cmd_buffers() final {destroy();}
};
class vk_semaphore         : public vk_object<VkSemaphore             , semaphore_desc>        , public child<vk_device>
{
    virtual VkResult create_obj(semaphore_desc desc) override final
    {
        auto info = desc.get_create_info();
        return vkCreateSemaphore(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroySemaphore(parent_ptr->get_handle(), this->handle, nullptr);
    }
public: 
    vk_semaphore(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_semaphore() final {destroy();}
};
class vk_fence             : public vk_object<VkFence                 , fence_desc>            , public child<vk_device>
{
    virtual VkResult create_obj(fence_desc desc) override final
    {
        auto info = desc.get_create_info();
        return vkCreateFence(parent_ptr->get_handle(), &info, nullptr, &this->handle);
    }
    virtual void free_obj() override final
    {
        vkDestroyFence(parent_ptr->get_handle(), this->handle, nullptr);
    }
public: 
    vk_fence(vk_device* parent) : child<vk_device>(parent) {}
    virtual ~vk_fence() final {destroy();}
};


class window_interface
{
public:
    window_interface() = delete;
    bool should_close()
    {
        return glfw->should_close();
    }
private:
    window_interface(GLFW_window_interface* glfw_ptr){glfw = glfw_ptr;}
    GLFW_window_interface* glfw;
    friend class vulkan_communication_instance;
};

//only call the vulkan API here
class vulkan_communication_instance
{
    struct command_buffer_data
    {
        VkFramebuffer     framebuffer;
        VkRenderPass       renderpass;
        VkExtent2D        draw_extent;
        VkOffset2D        draw_offset;
        VkClearValue      clear_value;
        VkViewport   dynamic_viewport;
        VkRect2D      dynamic_scissor;
        VkPipeline  graphics_pipeline;
    };
public:
    //run this once at the start
    void init(vulkan_communication_instance_init_info init_info)
    {
        GLFW_INTERFACE.init(init_info.window_parameters);

        static vk_instance instance;
        instance.description = get_instance_description(init_info.app_name);
        if(instance.init())
            throw std::runtime_error("Failed to create instance");

        if(DEBUG_MODE)
        {
            static vk_debug_messenger debug_messenger(&instance);
            debug_messenger.description.create_info = get_debug_create_info();
            if(debug_messenger.init())
            throw std::runtime_error("Failed to create debug messenger");
        }

        static vk_surface surface(&instance);
        surface.description.glfw_interface = GLFW_INTERFACE;
        if(surface.init())
            throw std::runtime_error("Failed to create surface");

        static VkPhysicalDevice phys_device;
        pick_physical_device(phys_device, instance.get_handle(), surface.get_handle());

        DESTROY_QUEUE.push_back(&instance);

        static vk_device device;
        device.description.device_queues      = get_device_queues(find_queue_family(phys_device, surface.get_handle()));
        device.description.phys_device        = phys_device;
        device.description.enabled_extensions = ::get_required_extension_names(phys_device);
        if(device.init())
            throw std::runtime_error("Failed to create device");

        static vk_swapchain swapchain(&device);
        swapchain_support swp_support = get_swapchain_support(phys_device, surface.get_handle());
        swapchain_features features   = get_swapchain_features(swp_support, GLFW_INTERFACE);
        swapchain.description.device_queues = device.description.device_queues;
        swapchain.description.features      = features;
        swapchain.description.surface       = surface.get_handle();
        if(swapchain.init())
            throw std::runtime_error("Failed to create swapchain");

        auto swapchain_images = get_swapchain_images(swapchain.get_handle(), device.get_handle());
        static std::vector<vk_image_view> swapchain_image_views; 
        for(size_t i = 0; i < swapchain_images.size(); i++)
        {
            auto& image = swapchain_images[i];

            vk_image_view image_view(&device);

            image_view.description.format = swapchain.description.features.surface_format.format;
            image_view.description.image  = image;
            if(image_view.init())
                throw std::runtime_error("Failed to create image view");

            swapchain_image_views.push_back(image_view);
        }

        static vk_renderpass renderpass(&device);
        renderpass.description = get_simple_renderpass_description(swapchain.description.features);
        if(renderpass.init())
            throw std::runtime_error("Failed to create render pass");

        std::vector<char> fragment_code, vertex_code;
        read_binary_file({"shaders/"}, "triangle_frag.spv", fragment_code);
        read_binary_file({"shaders/"}, "triangle_vert.spv", vertex_code);

        vk_shader_module fragment_module(&device), vertex_module(&device);
        fragment_module.description.byte_code = fragment_code;
        vertex_module.description.byte_code   =   vertex_code;
        vertex_module.description.stage = VK_SHADER_STAGE_VERTEX_BIT, fragment_module.description.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        if(fragment_module.init() || vertex_module.init())
            throw std::runtime_error("Failed to create shader modules");

        static vk_graphics_pipeline graphics_pipeline(&device);

        graphics_pipeline.description.shader_stages_info = get_shader_stages({vertex_module.description, fragment_module.description},
        {vertex_module.get_handle(), fragment_module.get_handle()});
        
        auto color_blend_attachment = get_color_no_blend_attachment(VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT);
        graphics_pipeline.description.color_blend_info = get_color_no_blend_state_descr({color_blend_attachment});

        dynamic_state_desc dynamic_state;
        dynamic_state.dynamic_state_list.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        dynamic_state.dynamic_state_list.push_back(VK_DYNAMIC_STATE_SCISSOR);
        graphics_pipeline.description.dynamic_state_info = dynamic_state;

        graphics_pipeline.description.vertex_input_info = get_empty_vertex_input_state();

        graphics_pipeline.description.input_assembly_info = get_input_assemly_state(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

        graphics_pipeline.description.multisample_info = get_disabled_multisample_info();

        graphics_pipeline.description.rasterization_info = get_simple_rasterization_info(VK_POLYGON_MODE_FILL, 1.f);

        graphics_pipeline.description.viewport_state_info = viewport_state_desc({{0.f, 0.f,
        (float)swapchain.description.features.extent.width, (float)swapchain.description.features.extent.height, 0.f, 1.f}}, 
        {{0, 0, swapchain.description.features.extent}});
        
        graphics_pipeline.description.renderpass = renderpass.get_handle();
        graphics_pipeline.description.subpass_index = 0;

        vk_pipeline_layout pipeline_layout(&device);
        if(pipeline_layout.init())
            throw std::runtime_error("Failed to init pipeline layout");

        graphics_pipeline.description.pipeline_layout = pipeline_layout.get_handle();

        if(graphics_pipeline.init())
            throw std::runtime_error("Failed to init graphics pipeline");

        static std::vector<vk_framebuffer> framebuffers;
        framebuffers.reserve(swapchain_image_views.size());
        for(size_t i = 0; i < swapchain_image_views.size(); i++)
        {
            vk_framebuffer framebuffer(&device);

            const auto& image_view = swapchain_image_views[i];

            framebuffer.description.attachments = {image_view.get_handle()};
            framebuffer.description.height = swapchain.description.features.extent.height;
            framebuffer.description.width  =  swapchain.description.features.extent.width;
            framebuffer.description.renderpass  = renderpass.get_handle();

            if(framebuffer.init())
                throw std::runtime_error("Failed to create framebuffer");

            framebuffers.push_back(framebuffer);
        }

        static vk_cmd_pool cmd_pool(&device);
        cmd_pool.description.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmd_pool.description.queue_fam_index = find_queue_family(phys_device, surface.get_handle()).graphics_fam_index.value();
        if (cmd_pool.init())
            throw std::runtime_error("Failed to create command pool");
        
        static vk_cmd_buffers cmd_buffers(&device, &cmd_pool);
        cmd_buffers.description.buffer_count = 1;
        cmd_buffers.description.cmd_pool = cmd_pool.get_handle();
        if(cmd_buffers.init())
            throw std::runtime_error("Failed to allocate command buffers");

        DESTROY_QUEUE.push_back(&device);

        static vk_fence inflight(&device);
        inflight.init();
        
        static vk_semaphore rendering_finished(&device), swapchain_image_available(&device);
        rendering_finished.init(), swapchain_image_available.init();

        vkGetDeviceQueue(device.get_handle(),
        find_queue_family(phys_device, surface.get_handle()).graphics_fam_index.value(),
        0, &DRAW_DATA.graphics_queue);

        DRAW_DATA.cmdbuffer = *cmd_buffers.get_handle();
        DRAW_DATA.device    =       device.get_handle();
        DRAW_DATA.inflight_fence = inflight.get_handle();
        DRAW_DATA.rendering_finished = rendering_finished.get_handle();
        DRAW_DATA.swapchain_image_available = swapchain_image_available.get_handle();
        DRAW_DATA.swapchain = swapchain.get_handle();
        DRAW_DATA.inflight_fence = inflight.get_handle();
        DRAW_DATA.swapchain_extent = swapchain.description.features.extent;

        DRAW_DATA.framebuffers.resize(framebuffers.size());
        for(size_t i = 0; i < framebuffers.size(); i++)
            DRAW_DATA.framebuffers[i] = framebuffers[i].get_handle();
        
        DRAW_DATA.renderpass = renderpass.get_handle();
        DRAW_DATA.graphics_pipeline = graphics_pipeline.get_handle();
    }

    //run this inside the render loop
    void invoke()
    {
        GLFW_INTERFACE.poll_events();
        draw();
    }
    void draw()
    {
        auto& G = DRAW_DATA;
        vkWaitForFences(G.device, 1, &G.inflight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(G.device, 1, &G.inflight_fence);

        uint32_t image_index;
        vkAcquireNextImageKHR(G.device, G.swapchain, UINT64_MAX, G.swapchain_image_available, VK_NULL_HANDLE,
        &image_index);

        vkResetCommandBuffer(G.cmdbuffer, 0);

        command_buffer_data data;
        data.clear_value = {{0.f, 0.f, 0.f, 1.f}};
        data.draw_extent = G.swapchain_extent;
        data.draw_offset = {0, 0};
        data.dynamic_scissor  = VkRect2D{{0, 0}, G.swapchain_extent};
        data.dynamic_viewport = VkViewport{0.f, 0.f, (float)G.swapchain_extent.width, (float)G.swapchain_extent.height, 0.f, 1.f};
        data.framebuffer = G.framebuffers[image_index];
        data.renderpass  = G.renderpass;
        data.graphics_pipeline = G.graphics_pipeline;

        if(record_command_buffer(G.cmdbuffer, data))
            throw std::runtime_error("Failed to record");

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1, submit_info.pCommandBuffers = &G.cmdbuffer;
        submit_info.waitSemaphoreCount = 1, submit_info.pWaitSemaphores = &G.swapchain_image_available;
        auto wait_stage = VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.pWaitDstStageMask = &wait_stage; //this is the stage that needs to wait. Other stages can execute
        submit_info.pSignalSemaphores = &G.rendering_finished, submit_info.signalSemaphoreCount = 1;

        if(vkQueueSubmit(G.graphics_queue, 1, &submit_info, G.inflight_fence))
            throw std::runtime_error("Failed to submit");

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1, present_info.pWaitSemaphores = &G.rendering_finished;
        present_info.swapchainCount = 1, present_info.pSwapchains = &G.swapchain, present_info.pImageIndices = &image_index;
        
        vkQueuePresentKHR(G.graphics_queue, &present_info);
    }
    VkResult record_command_buffer(VkCommandBuffer cmd_buffer, command_buffer_data data)
    {
        VkCommandBufferBeginInfo cmdbuffer_info{};
        cmdbuffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        auto err = vkBeginCommandBuffer(cmd_buffer, &cmdbuffer_info);
        if(err)
            return err;
        VkRenderPassBeginInfo renderpass_info{};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.clearValueCount = 1, renderpass_info.pClearValues = &data.clear_value;
        renderpass_info.framebuffer = data.framebuffer;
        renderpass_info.renderArea = VkRect2D{data.draw_offset, data.draw_extent};
        renderpass_info.renderPass = data.renderpass;
        
        vkCmdBeginRenderPass(cmd_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.graphics_pipeline);
        vkCmdSetViewport(cmd_buffer, 0, 1, &data.dynamic_viewport);
        vkCmdSetScissor(cmd_buffer, 0, 1, &data.dynamic_scissor);
        vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd_buffer);

        err = vkEndCommandBuffer(cmd_buffer);

        return err;
    }
    
    window_interface get_window_interface()
    {
        window_interface interface(&GLFW_INTERFACE);
        return interface;
    }
    //run this once at the end
    //note that a vulkan_communication_layer object can not be restarted after termination
    void terminate()
    {
        vkDeviceWaitIdle(DRAW_DATA.device);

        const size_t SIZE = DESTROY_QUEUE.size();
        for(size_t i = 0; i < SIZE; i++)
            DESTROY_QUEUE[SIZE-i-1]->destroy();

        GLFW_INTERFACE.terminate();
    }
private:
    static VkExtent2D get_extent(VkSurfaceCapabilitiesKHR surface_capabilities, GLFW_window_interface glfw_interface)
    {
        if(surface_capabilities.currentExtent.width != VK_UINT32_MAX) 
            return surface_capabilities.currentExtent;
        VkExtent2D window_extent = glfw_interface.get_window_extent();
        window_extent.height = std::clamp(window_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
        window_extent.width  = std::clamp(window_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);

        return window_extent;
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
        
        VkExtent2D extent = get_extent(swp_support.surface_capabilities, glfw_interface);

        return {surface_format, present_mode, extent, swp_support.surface_capabilities};
    }
    static extension_info get_instance_required_extension_names()
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

        extension_info info{};
        info.extensions = required_extension_names;
        info.layers     =     required_layer_names;

        return info;
    }
    static instance_desc get_instance_description(const char* app_name)
    {
        instance_desc description{};
        if(DEBUG_MODE)
            description.debug_messenger_ext = get_debug_create_info();
        
        //check extension support
        const auto ext_info = get_instance_required_extension_names();
        //first, check for extension and layer support
        uint32_t instance_property_count;
        vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
        nullptr);
        VkExtensionProperties instance_properties[instance_property_count];
        vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
        instance_properties);

        const char* instance_extension_names[instance_property_count];
        for(size_t i = 0; i < instance_property_count; i++)
            instance_extension_names[i] = instance_properties[i].extensionName;

        if(!check_support((size_t) instance_property_count, instance_extension_names,
        ext_info.extensions.data(), ext_info.extensions.size()))
            throw std::runtime_error("Failed to find required instance extensions");
        
        uint32_t instance_layer_count;
        vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
        VkLayerProperties instance_layer_properties[instance_layer_count];
        vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties);

        const char* instance_layer_names[instance_layer_count];
        for(size_t i = 0; i < instance_layer_count; i++)
            instance_layer_names[i] = instance_layer_properties[i].layerName;

        if(!check_support((size_t) instance_layer_count, instance_layer_names,
        ext_info.layers.data(), ext_info.layers.size()))
            throw std::runtime_error("Failed to find required instance layers");

        description.ext_info = ext_info;
        description.app_info = get_app_info(app_name);
        
        return description;
    }
    static void pick_physical_device(VkPhysicalDevice& phys_device, VkInstance instance, VkSurfaceKHR surface)
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
    
    GLFW_window_interface GLFW_INTERFACE;

    struct draw_data
    {
        VkFence                  inflight_fence;
        VkSemaphore   swapchain_image_available;
        VkSemaphore          rendering_finished;
        VkDevice                         device;
        VkCommandBuffer               cmdbuffer;
        VkSwapchainKHR                swapchain;
        VkQueue                  graphics_queue;
        VkExtent2D             swapchain_extent;
        VkRenderPass                 renderpass;
        VkPipeline            graphics_pipeline;
        std::vector<VkFramebuffer> framebuffers;
    };
    draw_data DRAW_DATA;
    
    std::vector<destroyable*>   DESTROY_QUEUE;
};


/************************************************APPLICATION************************************************/


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

    window_interface window = instance.get_window_interface();
    while(!window.should_close())
    {
        instance.invoke();       
    }

    instance.terminate();

    return 0;    
}