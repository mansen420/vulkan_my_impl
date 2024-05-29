#pragma once

#include "vulkan_handle_description.h"
#include "debug.h"

namespace vk_handle
{
    struct empty_dependency{};
    class destroyable
    {
    public:
        virtual void destroy() = 0;
        virtual ~destroyable() = default;       //otherwise calling the destructor through base pointers is UB
    };
    template <typename return_t, typename creation_dependency = vk_handle::empty_dependency> 
    class creatable
    {
    public:
        virtual return_t init(creation_dependency dependency = creation_dependency{}) = 0;
        virtual ~creatable() = default;
    };

    //Note to self : this class is absolutely NOT responsible for the lifetime of vulkan objects.
    //ONLY the destruction and creation of some handle. NOTHING more.
    //Feel free to copy its handles everywhere and change them whenever you want
    template <typename hndl_t, typename description_t> 
    class vk_hndl : public destroyable, public creatable<VkResult, description_t>
    {
    protected:
        description_t description;
        virtual VkResult init() = 0;
    public:
        hndl_t handle;

        explicit operator bool()   const {return handle != hndl_t{VK_NULL_HANDLE};}
        operator hndl_t() const {return handle;}

        //record description
        virtual VkResult init(description_t description) override {this->description = description; return init();}
        virtual description_t get_description() const final {return description;}

        vk_hndl() : description(description_t{}), handle(hndl_t{VK_NULL_HANDLE}) {}
        vk_hndl(hndl_t handle) : description(description_t{}), handle(handle) {}

        virtual ~vk_hndl() = default;
    };

    class instance          : public vk_hndl<VkInstance, vk_handle::description::instance_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            const auto info = description.get_create_info();
            return vkCreateInstance(&info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyInstance(handle, nullptr);
        }
    };
    class debug_messenger   : public vk_hndl<VkDebugUtilsMessengerEXT, vk_handle::description::debug_messenger_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            const auto info = description.get_create_info();

            //fetch function address in runtime 
            auto fun = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            description.parent, "vkCreateDebugUtilsMessengerEXT");

            if(fun == nullptr)
                throw std::runtime_error("Failed to get function pointer : \"vkCreateDebugUtilsMessengerEXT.\"");

            return fun(description.parent, &info, nullptr, &handle);
        }
        void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger)
        {
            auto fun = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
            if(fun != nullptr)
                fun(instance, debug_messenger, nullptr);
            else
                throw std::runtime_error("Failed to find function pointer \"vkDestroyDebugUtilsMessengerEXT.\"");
        }
        virtual void destroy() override final
        {
            destroy_debug_messenger(description.parent, this->handle);
        }
    };
    class surface           : public vk_hndl<VkSurfaceKHR, vk_handle::description::surface_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            return glfwCreateWindowSurface(description.parent, description.glfw_interface, nullptr, &handle);
        }
        virtual void destroy()
        {
            vkDestroySurfaceKHR(description.parent, this->handle, nullptr);
        }
    };
    class device            : public vk_hndl<VkDevice, vk_handle::description::device_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateDevice(description.phys_device, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyDevice(this->handle, nullptr);
        }
    };
    class swapchain         : public vk_hndl<VkSwapchainKHR, vk_handle::description::swapchain_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateSwapchainKHR(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroySwapchainKHR(description.parent, this->handle, nullptr);
        }
    };
    class image_view        : public vk_hndl<VkImageView, vk_handle::description::image_view_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateImageView(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyImageView(description.parent, this->handle, nullptr);
        }
    };
    class renderpass        : public vk_hndl<VkRenderPass, vk_handle::description::renderpass_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateRenderPass(description.parent, &info, nullptr, &handle);
        } 
        virtual void destroy() override final
        {
            vkDestroyRenderPass(description.parent, this->handle, nullptr);
        }
    };
    class shader_module     : public vk_hndl<VkShaderModule, vk_handle::description::shader_module_desc>
    {
    public:
        using vk_hndl::init;
        virtual void destroy() override final
        {
            vkDestroyShaderModule(description.parent, this->handle, nullptr);
        }
        virtual VkResult init() override final 
        {
            const auto info = description.get_create_info(); 
            return vkCreateShaderModule(description.parent, &info, nullptr, &handle);
        }
    };
    class graphics_pipeline : public vk_hndl<std::vector<VkPipeline>, vk_handle::description::graphics_pipeline_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            handle.resize(description.count.value_or(1));

            auto info = description.get_create_info();
            return vkCreateGraphicsPipelines(description.parent, description.pipeline_cache.value_or(VK_NULL_HANDLE),
            description.count.value_or(1), &info, nullptr, handle.data());
        }
        virtual void destroy() override final
        {
            for(size_t i = 0; i < this->handle.size(); i++)
                vkDestroyPipeline(description.parent, this->handle[i], nullptr);
        }
    };
    class pipeline_layout   : public vk_hndl<VkPipelineLayout, vk_handle::description::pipeline_layout_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreatePipelineLayout(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyPipelineLayout(description.parent, this->handle,  nullptr);
        }
    };
    class framebuffer       : public vk_hndl<VkFramebuffer, vk_handle::description::framebuffer_desc>
    {   
    public:
        using vk_hndl::init;

        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateFramebuffer(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyFramebuffer(description.parent, this->handle, nullptr);
        }
    };
    class cmd_pool          : public vk_hndl<VkCommandPool, vk_handle::description::cmd_pool_desc>
    {
    public:
        using vk_hndl::init;

        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateCommandPool(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyCommandPool(description.parent, this->handle, nullptr);
        }
    };
    class cmd_buffers       : public vk_hndl<std::vector<VkCommandBuffer>, vk_handle::description::cmd_buffers_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            handle.resize(description.buffer_count);
            auto info = description.get_alloc_info();
            return vkAllocateCommandBuffers(description.parent, &info, handle.data());
        }
        virtual void destroy() override final
        {
            vkFreeCommandBuffers(description.parent, description.cmd_pool, 
            description.buffer_count, this->handle.data());
        }
    };
    class semaphore         : public vk_hndl<VkSemaphore, vk_handle::description::semaphore_desc>
    {
    public: 
        using vk_hndl::init;

        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateSemaphore(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroySemaphore(description.parent, this->handle, nullptr);
        }
    };
    class fence             : public vk_hndl<VkFence, vk_handle::description::fence_desc>
    {
    public: 
        using vk_hndl::init;

        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateFence(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyFence(description.parent, this->handle, nullptr);
        }
    };
    class buffer            : public vk_hndl<VkBuffer, vk_handle::description::buffer_desc>
    {
    public:
        using vk_hndl::init;
        virtual VkResult init() override final
        {
            auto info = description.get_create_info();
            return vkCreateBuffer(description.parent, &info, nullptr, &handle);
        }
        virtual void destroy() override final
        {
            vkDestroyBuffer(description.parent, handle, nullptr);
        }
    };

}

