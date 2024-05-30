#pragma once

#include "vulkan_handle_description.h"
#include "debug.h"

namespace vk_handle
{

#define INIT_DECLARATION(handle_t, description_t) VkResult init(handle_t& handle, description::description_t description);

    INIT_DECLARATION(VkInstance              , instance_desc)
    INIT_DECLARATION(VkDevice                , device_desc)
    INIT_DECLARATION(VkDebugUtilsMessengerEXT, debug_messenger_desc)
    INIT_DECLARATION(VkSurfaceKHR            , surface_desc)
    INIT_DECLARATION(VkSwapchainKHR          , swapchain_desc)
    INIT_DECLARATION(VkImageView             , image_view_desc)    
    INIT_DECLARATION(VkRenderPass            , renderpass_desc)    
    INIT_DECLARATION(VkShaderModule          , shader_module_desc)
    VkResult init(std::vector<VkPipeline>& handle, std::vector<description::graphics_pipeline_desc> description);
    INIT_DECLARATION(VkPipelineLayout, pipeline_layout_desc)    
    INIT_DECLARATION(VkFramebuffer, framebuffer_desc)    
    INIT_DECLARATION(VkCommandPool, cmd_pool_desc)
    INIT_DECLARATION(std::vector<VkCommandBuffer>, cmd_buffers_desc)    
    INIT_DECLARATION(VkSemaphore, semaphore_desc)    
    INIT_DECLARATION(VkFence, fence_desc)    
    INIT_DECLARATION(VkBuffer, buffer_desc)    


#undef INIT_DECLARATION

#define DEST_DECLARATION(handle_t, description_t) void destroy(handle_t handle, description::description_t desc);

    DEST_DECLARATION(VkInstance, instance_desc)
    DEST_DECLARATION(VkDevice  , device_desc)
    DEST_DECLARATION(VkDebugUtilsMessengerEXT, debug_messenger_desc)
    DEST_DECLARATION(VkSurfaceKHR, surface_desc)
    DEST_DECLARATION(VkSwapchainKHR, swapchain_desc)
    DEST_DECLARATION(VkImageView, image_view_desc)
    DEST_DECLARATION(VkRenderPass, renderpass_desc)
    DEST_DECLARATION(VkShaderModule, shader_module_desc)
    void destroy(std::vector<VkPipeline> handle, std::vector<description::graphics_pipeline_desc> description);
    DEST_DECLARATION(VkPipelineLayout, pipeline_layout_desc)
    DEST_DECLARATION(VkFramebuffer, framebuffer_desc)
    DEST_DECLARATION(VkCommandPool, cmd_pool_desc)
    DEST_DECLARATION(std::vector<VkCommandBuffer>, cmd_buffers_desc)
    DEST_DECLARATION(VkSemaphore, semaphore_desc)
    DEST_DECLARATION(VkRenderPass, renderpass_desc)
    DEST_DECLARATION(VkFence, fence_desc)
    DEST_DECLARATION(VkBuffer, buffer_desc)

#undef DEST_DECLARATION 

    template<typename handle_t, typename desc_t>
    class vk_obj_wrapper
    {
        desc_t   description_record{};

        public :

        handle_t handle{VK_NULL_HANDLE};

        explicit operator bool() const {return handle != handle_t{VK_NULL_HANDLE};};
        operator handle_t() const {return handle;}

        vk_obj_wrapper() :  description_record(desc_t{}), handle{VK_NULL_HANDLE} {}

        VkResult init(desc_t description)
        {
            if(*this)
                INFORM_ERR("WARNING : double initialization of : " << typeid(handle).name());
            auto res = vk_handle::init(handle, description);
            if(res == VK_SUCCESS) //or res >= 0
                description_record = description;
            return res;
        }
        void destroy()
        {
            vk_handle::destroy(handle, description_record);
        }
        desc_t get_description(){return description_record;}
    };

    typedef vk_obj_wrapper<VkInstance, description::instance_desc> instance;
    typedef vk_obj_wrapper<VkDevice, description::device_desc> device;
    typedef vk_obj_wrapper<VkDebugUtilsMessengerEXT, description::debug_messenger_desc> debug_messenger;
    typedef vk_obj_wrapper<VkSwapchainKHR, description::swapchain_desc> swapchain;
    typedef vk_obj_wrapper<VkSurfaceKHR, description::surface_desc> surface;
    typedef vk_obj_wrapper<VkImageView, description::image_view_desc> image_view;
    typedef vk_obj_wrapper<VkRenderPass, description::renderpass_desc> renderpass;
    typedef vk_obj_wrapper<VkShaderModule, description::shader_module_desc> shader_module;
    typedef vk_obj_wrapper<std::vector<VkPipeline>, std::vector<description::graphics_pipeline_desc>> graphics_pipeline;
    typedef vk_obj_wrapper<VkPipelineLayout, description::pipeline_layout_desc> pipeline_layout;
    typedef vk_obj_wrapper<VkFramebuffer, description::framebuffer_desc> framebuffer;
    typedef vk_obj_wrapper<VkCommandPool, description::cmd_pool_desc> cmd_pool;
    typedef vk_obj_wrapper<std::vector<VkCommandBuffer>, description::cmd_buffers_desc> cmd_buffers;
    typedef vk_obj_wrapper<VkSemaphore, description::semaphore_desc> semaphore;
    typedef vk_obj_wrapper<VkFence, description::fence_desc> fence;
    typedef vk_obj_wrapper<VkBuffer, description::buffer_desc> buffer;
}