#pragma once

#include <cstring>    //strcmp
#include <algorithm> //std:clamp
#include <limits>   //for max uint32

#include "vulkan_handle_description.h"
#include "ignore.h"
#include "eng_log.h"
#include "debug.h"

//Note for future user (me) it is a bad idea to include this header file in more than one translation unit
//I exiled these functions here to keep clutter down and because I will eventually stop using this
//this is nothing more than a code dump, k?


inline VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback_fun(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, 
VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data, void* p_user_data)
{
    ignore(p_user_data);

    const char* message_type_text;
    switch (message_type)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            message_type_text = "VALIDATION";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            message_type_text = "GENERAL";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            message_type_text = "PERFORMANCE";
            break;
        default:
            message_type_text = "UNDETERMINED TYPE";
            break;
    }

    const char* message_severity_text;
    switch (message_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        message_severity_text = "WARNING";
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        message_severity_text = "ERROR";
        break;
    default:
        message_severity_text = "UNDETERMINED SEVERITY";
        break;
    }
    ENG_ERR_LOG << "VALIDATION LAYER : " << p_callback_data->pMessage << " (SEVERITY : "<< message_severity_text << ", TYPE : "
    << message_type_text << ')' << std::endl;

    return VK_FALSE;
}

inline std::vector<const char*> get_physical_device_required_extension_names()
{
    std::vector<const char*> required_extension_names;

    required_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    return required_extension_names;
}
inline VkApplicationInfo get_app_info(const char* app_name)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_0;
    app_info.engineVersion = VK_MAKE_VERSION(1.0, 0.0, 0.0);
    app_info.applicationVersion = VK_MAKE_VERSION(1.0, 0.0, 0.0);
    app_info.pApplicationName = app_name;
    return app_info;
}
inline VkDebugUtilsMessengerCreateInfoEXT get_debug_create_info()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};

    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback_fun;
    create_info.pUserData = nullptr;

    return create_info;
}
inline std::vector<VkPhysicalDevice> get_physical_devices(VkInstance instance)
{
    uint32_t phys_devices_count;
    vkEnumeratePhysicalDevices(instance, &phys_devices_count, nullptr);
    std::vector<VkPhysicalDevice> phys_devices(phys_devices_count);
    vkEnumeratePhysicalDevices(instance,&phys_devices_count, phys_devices.data());
    return phys_devices;
}

inline bool check_support(const size_t available_name_count, const char* const* available_names, const char* const* required_names, const size_t required_name_count)
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
inline bool check_support(const std::vector<const char*> available_names, const std::vector<const char*> required_names)
{
    return check_support(available_names.size(), available_names.data(), required_names.data(), required_names.size());
}

inline std::vector<const char*> get_extension_names(VkPhysicalDevice device)
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
inline std::vector<VkQueueFamilyProperties> get_queue_properties(VkPhysicalDevice device)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
    return properties;
}
inline std::vector<VkImage> get_swapchain_images(VkSwapchainKHR swapchain, VkDevice device)
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


inline VkPipelineVertexInputStateCreateInfo get_empty_vertex_input_state()
{
    VkPipelineVertexInputStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.vertexBindingDescriptionCount = 0, info.vertexAttributeDescriptionCount = 0;
    return info;
}
inline VkPipelineInputAssemblyStateCreateInfo get_input_assemly_state(VkPrimitiveTopology primitives, VkBool32 primitive_restart_enabled)
{
    VkPipelineInputAssemblyStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.topology = primitives, info.primitiveRestartEnable = primitive_restart_enabled;
    return info;
}
inline VkPipelineRasterizationStateCreateInfo get_simple_rasterization_info(VkPolygonMode polygon_mode, float line_width)
{
    VkPipelineRasterizationStateCreateInfo info{};

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    info.polygonMode = polygon_mode, info.lineWidth = line_width;

    info.depthBiasEnable = VK_FALSE,  info.rasterizerDiscardEnable = VK_FALSE, info.depthClampEnable = VK_FALSE;

    info.cullMode = VK_CULL_MODE_BACK_BIT, info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    
    return info;
}
inline VkPipelineMultisampleStateCreateInfo get_disabled_multisample_info()
{
    VkPipelineMultisampleStateCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    info.sampleShadingEnable = VK_FALSE; info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    return info;
}
inline VkPipelineColorBlendAttachmentState get_color_no_blend_attachment(VkColorComponentFlags color_write_mask)
{
    VkPipelineColorBlendAttachmentState state{};
    state.colorWriteMask = color_write_mask;
    state.blendEnable = VK_FALSE;
    return state;
}
inline vk_handle::description::color_blend_desc get_color_no_blend_state_descr(std::vector<VkPipelineColorBlendAttachmentState> states)
{
    vk_handle::description::color_blend_desc description{};

    description.attachment_states = states;
    description.logic_op_enabled = VK_FALSE;

    return description;
}
inline std::vector<VkPipelineShaderStageCreateInfo> get_shader_stages(std::vector<vk_handle::description::shader_module_desc> module_descriptions, std::vector<VkShaderModule> module_handles)
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

inline vk_handle::description::renderpass_desc get_simple_renderpass_description(vk_handle::description::swapchain_features swp_features, VkDevice parent)
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

    vk_handle::description::subpass_description subpass;
    subpass.color_attachment_refs.push_back(attachment_ref);

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL, dependency.dstSubpass = 0;//index
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0, dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vk_handle::description::renderpass_desc description;
    description.parent = parent;
    description.attachments.push_back(attachment);
    description.subpass_descriptions.push_back(subpass);
    description.subpass_dependencies.push_back(dependency);

    return description;
}
inline std::vector<vk_handle::description::device_queue> get_device_queues(vk_handle::description::queue_families fam_indices)
{
    std::set<uint32_t> indices = {fam_indices.graphics_fam.value().index, fam_indices.present_fam.value().index,
    fam_indices.dedicated_transfer_fam.has_value() ? fam_indices.dedicated_transfer_fam.value().index : 
    fam_indices.graphics_fam.value().index}; //if dedicated transfer has no value, this will be ignored
    
    std::vector<vk_handle::description::device_queue> device_queues;
    
    for(const auto& index: indices)
    {
        vk_handle::description::device_queue queue;
        queue.family_index = index; //FIXME chek that count isn't too big
        queue.count = 1;
        if(fam_indices.dedicated_transfer_fam.has_value() && index == fam_indices.dedicated_transfer_fam.value().index)
            queue.flags |= vk_handle::description::DEDICATED_TRANSFER_BIT;
        if(index == fam_indices.graphics_fam.value().index)
        {
            queue.flags |= vk_handle::description::GRAPHICS_BIT;
            queue.count += fam_indices.dedicated_transfer_fam.has_value() ? 0 : 1;
            queue.count = std::clamp(queue.count, (uint32_t)0, fam_indices.graphics_fam.value().max_queue_count);
        }
        if(index == fam_indices.present_fam.value().index)
            queue.flags |= vk_handle::description::PRESENT_BIT;
        device_queues.push_back(queue);
    }
    return device_queues;
}
inline bool is_adequate(vk_handle::description::queue_families indices)
{
    return indices.graphics_fam.has_value() && indices.present_fam.has_value();
}
inline bool is_complete(vk_handle::description::queue_families indices)
{
    return is_adequate(indices) && indices.dedicated_transfer_fam.has_value();
}
inline vk_handle::description::swapchain_support get_swapchain_support(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    vk_handle::description::swapchain_support support;
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
inline vk_handle::description::queue_families find_queue_family(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    const auto queue_family_prperties = get_queue_properties(phys_device);

    vk_handle::description::queue_families indices;

    for(size_t i = 0; i < queue_family_prperties.size(); i++)
    {
        uint32_t i32 = static_cast<uint32_t>(i);
        if(queue_family_prperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics_fam = vk_handle::description::queue_fam_property{};
            indices.graphics_fam.value().index = i32;   //implicit transfer family
            indices.graphics_fam.value().flags = queue_family_prperties[i].queueFlags;
            indices.graphics_fam.value().max_queue_count = queue_family_prperties[i].queueCount;
        }
        else if((queue_family_prperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && !(queue_family_prperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            indices.dedicated_transfer_fam = vk_handle::description::queue_fam_property{};
            indices.dedicated_transfer_fam.value().index = i32;
            indices.dedicated_transfer_fam.value().flags = queue_family_prperties[i].queueFlags;
            indices.dedicated_transfer_fam.value().max_queue_count = queue_family_prperties[i].queueCount;
        }
        VkBool32 supports_surface = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i32, surface, &supports_surface);
        if(supports_surface)
        {
            indices.present_fam = vk_handle::description::queue_fam_property{};
            indices.present_fam.value().index = i32;
            indices.present_fam.value().flags = queue_family_prperties[i].queueFlags;
            indices.present_fam.value().max_queue_count = queue_family_prperties[i].queueCount;
        }
        
        if(is_complete(indices))
            break;
    }
    return indices;
}
inline bool is_adequate(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
{
    vk_handle::description::queue_families indices = find_queue_family(phys_device, surface);

    if(!is_adequate(indices))
        throw std::runtime_error("incomplete family indices");

    auto avl_names = get_extension_names(phys_device);
    
    auto req_names = get_physical_device_required_extension_names();
    bool extensions_supported = check_support(avl_names, req_names);

    vk_handle::description::swapchain_support device_support = get_swapchain_support(phys_device, surface);
    
    bool supports_swapchain = !(device_support.surface_formats.empty() || device_support.surface_present_modes.empty());

    return extensions_supported && is_adequate(indices) && supports_swapchain;
}


//determines index in physical device memory properties for this buffer's requirements with a user-defined bitmask
inline uint32_t get_memory_type_index(uint32_t memory_type_bitmask, VkMemoryRequirements mem_reqs, VkPhysicalDevice phys_dev)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(phys_dev, &mem_properties);

    //from the spec
    /*memoryTypeBits is a bitmask and contains one bit set for every supported memory type for the resource.
    Bit i is set if and only if the memory type i in the VkPhysicalDeviceMemoryProperties structure for the physical
    device is supported for the resource.*/

    for(size_t i = 0; i < mem_properties.memoryTypeCount; ++i)
    {
        if((mem_reqs.memoryTypeBits & (0b1 << i)) && ((mem_properties.memoryTypes[i].propertyFlags & memory_type_bitmask) 
        == memory_type_bitmask))
            return i;
    }
    throw std::runtime_error("Failed to find memory index for buffer");
}
inline VkMemoryAllocateInfo get_mem_alloc_info(uint32_t memory_type_bitmask, VkBuffer buffer, VkDevice parent, VkPhysicalDevice phys_dev)
{
    VkMemoryAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(parent, buffer, &mem_reqs);
    info.memoryTypeIndex = get_memory_type_index(memory_type_bitmask, mem_reqs, phys_dev);
    info.allocationSize  = mem_reqs.size;
    info.pNext = nullptr;
    return info;
}
inline VkExtent2D get_window_extent(GLFWwindow* window_ptr)
{
    int width, height;
    glfwGetFramebufferSize(window_ptr, &width, &height);

    VkExtent2D window_extent;
    window_extent.height = static_cast<uint32_t>(height);
    window_extent.width = static_cast<uint32_t>(width);

    return window_extent;
}
inline VkExtent2D get_extent(VkSurfaceCapabilitiesKHR surface_capabilities, GLFWwindow* window_ptr)
{
    if(surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
        return surface_capabilities.currentExtent;
    VkExtent2D window_extent = get_window_extent(window_ptr);
    window_extent.height = std::clamp(window_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    window_extent.width  = std::clamp(window_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);

    return window_extent;
}    
inline vk_handle::description::swapchain_features get_swapchain_features(vk_handle::description::swapchain_support swp_support, GLFWwindow* window_ptr)
{
    VkSurfaceFormatKHR surface_format = swp_support.surface_formats[0];
    for(const auto& surface_format_candidate : swp_support.surface_formats)
        if(surface_format_candidate.format == VK_FORMAT_B8G8R8_SRGB && surface_format_candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            surface_format = surface_format_candidate;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for(const auto& present_mode_candidate : swp_support.surface_present_modes)
        if(present_mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR)
            present_mode = present_mode_candidate;
    
    VkExtent2D extent = get_extent(swp_support.surface_capabilities, window_ptr);

    return {surface_format, present_mode, extent, swp_support.surface_capabilities};
}
inline std::vector<const char*> get_glfw_required_extensions()
{
    uint32_t count;
    const char** data = glfwGetRequiredInstanceExtensions(&count);
    //Apparently pointers can function like iterators: vector(c_array, c_array + size). nice!
    std::vector<const char*> extensions(data, data + count);
    return extensions;
}
inline vk_handle::description::extension_info get_instance_required_extension_names()
{
    const bool& ENABLE_VALIDATION_LAYERS = DEBUG_MODE;

    std::vector<const char*> required_extension_names;
    std::vector<const char*>     required_layer_names;

    required_extension_names = get_glfw_required_extensions();
//HACK hardcoding 
    if(ENABLE_VALIDATION_LAYERS)
    {
        required_layer_names.push_back("VK_LAYER_KHRONOS_validation");
        required_extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk_handle::description::extension_info info{};
    info.extensions = required_extension_names;
    info.layers     =     required_layer_names;

    return info;
}
inline vk_handle::description::instance_desc get_instance_description(const char* app_name)
{
    vk_handle::description::instance_desc description{};
    if(DEBUG_MODE)
        description.debug_messenger_ext = get_debug_create_info();
    
    //check extension support
    const auto ext_info = get_instance_required_extension_names();
    //first, check for extension and layer support
    uint32_t instance_property_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
    nullptr);
    std::vector<VkExtensionProperties> instance_properties(instance_property_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &instance_property_count,
    instance_properties.data());

    std::vector<const char*> instance_extension_names(instance_property_count);
    for(size_t i = 0; i < instance_property_count; i++)
        instance_extension_names[i] = instance_properties[i].extensionName;

    if(!check_support((size_t) instance_property_count, instance_extension_names.data(),
    ext_info.extensions.data(), ext_info.extensions.size()))
        throw std::runtime_error("Failed to find required instance extensions");
    
    uint32_t instance_layer_count;
    vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
    std::vector<VkLayerProperties> instance_layer_properties(instance_layer_count);
    vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties.data());

    std::vector<const char*> instance_layer_names(instance_layer_count);
    for(size_t i = 0; i < instance_layer_count; i++)
        instance_layer_names[i] = instance_layer_properties[i].layerName;

    if(!check_support((size_t) instance_layer_count, instance_layer_names.data(),
    ext_info.layers.data(), ext_info.layers.size()))
        throw std::runtime_error("Failed to find required instance layers");

    description.ext_info = ext_info;
    description.app_info = get_app_info(app_name);
    
    return description;
}
inline VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface)
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
    auto phys_device = candidates[0];

    return phys_device;
}