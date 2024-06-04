#include "volk.h"
#include "GLFW/glfw3.h"

#include "vulkan_handle.h"
#include "ignore.h"

VkResult vk_handle::init(VkInstance& handle, description::instance_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateInstance(&info, nullptr, &handle);
}
VkResult vk_handle::init(VkDevice& handle, description::device_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateDevice(desc.phys_device, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkSurfaceKHR& handle, description::surface_desc desc)
{
    return glfwCreateWindowSurface(desc.parent, desc.glfw_interface, nullptr, &handle);
}
VkResult vk_handle::init(VkSwapchainKHR& handle, description::swapchain_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateSwapchainKHR(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkDebugUtilsMessengerEXT& handle, description::debug_messenger_desc desc)
{
    const auto info = desc.get_create_info();
    
    //fetch function address in runtime 
    auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    desc.parent, "vkCreateDebugUtilsMessengerEXT");
    
    if(fun == nullptr)
        throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");
    
    return fun(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkImageView& handle, description::image_view_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateImageView(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkRenderPass& handle, description::renderpass_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateRenderPass(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkShaderModule& handle, description::shader_module_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateShaderModule(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(std::vector<VkPipeline>& handle, std::vector<vk_handle::description::graphics_pipeline_desc> desc)
{
    handle.resize(desc.size());
    std::vector<VkGraphicsPipelineCreateInfo> infos;
    infos.reserve(desc.size());
    for(auto& d : desc)
        infos.push_back(d.get_create_info());
    return vkCreateGraphicsPipelines(desc[0].parent, desc[0].pipeline_cache.value_or(VK_NULL_HANDLE), desc.size(), infos.data(),
    nullptr, handle.data());
}
VkResult vk_handle::init(VkPipelineLayout& handle, description::pipeline_layout_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreatePipelineLayout(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkFramebuffer& handle, description::framebuffer_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateFramebuffer(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkCommandPool& handle, description::cmd_pool_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateCommandPool(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(std::vector<VkCommandBuffer>& handle, description::cmd_buffers_desc desc)
{
    auto info = desc.get_alloc_info();
    handle.resize(desc.buffer_count);
    return vkAllocateCommandBuffers(desc.parent, &info, handle.data());
}
VkResult vk_handle::init(VkSemaphore& handle, description::semaphore_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateSemaphore(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkFence& handle, description::fence_desc desc)
{
    auto info = desc.get_create_info();
    return vkCreateFence(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VkBuffer& handle, description::buffer_desc& desc)
{
    auto info = desc.get_create_info();
    return vmaCreateBuffer(desc.allocator, &info, &desc.alloc_info,
    &handle, &desc.allocation_object, nullptr);
}
VkResult vk_handle::init(VkDeviceMemory& handle, description::memory_desc desc)
{
    auto info = desc.get_info();
    return vkAllocateMemory(desc.parent, &info, nullptr, &handle);
}
VkResult vk_handle::init(VmaAllocator& handle, VmaAllocatorCreateInfo description)
{
    return vmaCreateAllocator(&description, &handle);
}


void vk_handle::destroy(VkInstance handle, description::instance_desc desc)
{
    ignore(desc);
    vkDestroyInstance(handle, nullptr);
}
void vk_handle::destroy(VkDevice handle, description::device_desc desc)
{
    ignore(desc);
    vkDestroyDevice(handle, nullptr);
}
void vk_handle::destroy(VkSurfaceKHR handle, description::surface_desc desc)
{
    vkDestroySurfaceKHR(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkSwapchainKHR handle, description::swapchain_desc desc)
{
    vkDestroySwapchainKHR(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkDebugUtilsMessengerEXT handle, description::debug_messenger_desc desc)
{
    auto fun = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
    desc.parent, "vkDestroyDebugUtilsMessengerEXT");
    if(fun != nullptr)
        fun(desc.parent, handle, nullptr);
    else
        throw std::runtime_error("Failed to find function pointer \"vkDestroyDebugUtilsMessengerEXT.\"");
}
void vk_handle::destroy(VkImageView handle, description::image_view_desc desc)
{
    vkDestroyImageView(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkRenderPass handle, description::renderpass_desc desc)
{
    vkDestroyRenderPass(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkShaderModule handle, description::shader_module_desc desc)
{
    vkDestroyShaderModule(desc.parent, handle, nullptr);
}
void vk_handle::destroy(std::vector<VkPipeline> handle, std::vector<description::graphics_pipeline_desc> desc)
{
    if(desc.empty())
        INFORM_ERR("WARNING : destroying graphics pipeline with 0 descriptions!");
    if(desc.size() != handle.size())
        INFORM_ERR("WARNING : unequal number of graphics pipelines and descriptions!");
    for(size_t i = 0; i < handle.size(); ++i)
        vkDestroyPipeline(desc[i].parent, handle[i], nullptr);
}
void vk_handle::destroy(VkPipelineLayout handle, description::pipeline_layout_desc desc)
{
    vkDestroyPipelineLayout(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkFramebuffer handle, description::framebuffer_desc desc)
{
    vkDestroyFramebuffer(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkCommandPool handle, description::cmd_pool_desc desc)
{
    vkDestroyCommandPool(desc.parent, handle, nullptr);
}
void vk_handle::destroy(std::vector<VkCommandBuffer> handle, description::cmd_buffers_desc desc)
{
    if(desc.buffer_count != handle.size())
        INFORM_ERR("WARNING : buffer count not equal to buffer vector size!");
    vkFreeCommandBuffers(desc.parent, desc.cmd_pool, desc.buffer_count, handle.data());
}
void vk_handle::destroy(VkSemaphore handle, description::semaphore_desc desc)
{
    vkDestroySemaphore(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkFence handle, description::fence_desc desc)
{
    vkDestroyFence(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VkBuffer handle, description::buffer_desc desc)
{
    vmaDestroyBuffer(desc.allocator, handle, desc.allocation_object);
}
void vk_handle::destroy(VkDeviceMemory handle, description::memory_desc desc)
{
    vkFreeMemory(desc.parent, handle, nullptr);
}
void vk_handle::destroy(VmaAllocator handle, [[maybe_unused]] VmaAllocatorCreateInfo description)
{
    vmaDestroyAllocator(handle);
}
