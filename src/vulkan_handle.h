#pragma once

#include "volk.h"
#include "GLFW/glfw3.h"

#include <optional>
#include <vector>
#include <set>
#include <array>
#include <stdexcept>

namespace vk_handle
{
    namespace description
    {
        struct queue_fam_property
        {
            uint32_t           index;
            VkQueueFlags       flags;
            uint32_t max_queue_count;
        };
        struct queue_families //Stores indices of queue families that support support various operations
        {
            std::optional<queue_fam_property> graphics_fam, present_fam, dedicated_transfer_fam;
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
            std::optional<VkInstanceCreateFlags> flags;
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
                info.flags = flags.value_or(0);
                info.pNext = debug_messenger_ext.has_value() ? &debug_messenger_ext.value() : nullptr;
                return info;
            }
        };
        struct debug_messenger_desc
        {
            VkInstance parent;
            VkDebugUtilsMessengerCreateInfoEXT create_info{};
            VkDebugUtilsMessengerCreateInfoEXT get_create_info()
            {
                return create_info;
            }
        };
        struct surface_desc
        {
            VkInstance parent;
            GLFWwindow* glfw_interface;
        };
        enum   queue_support_flag_bits
        {
            GRAPHICS_BIT           = 0b0001,
            PRESENT_BIT            = 0b0010,
            DEDICATED_TRANSFER_BIT = 0b0100
        };
        struct device_queue //Stores family index, count, uses, and priority of a logical device queue 
        {
            uint32_t family_index;
            uint32_t        count;
            uint32_t        flags;
            float        priority;
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
            VkDevice parent;

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
            VkDevice parent;

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
            VkDevice parent;

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
            VkDevice parent;

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
        struct vertex_input_desc
        {
            std::vector<VkVertexInputBindingDescription>   binding_descriptions;
            std::vector<VkVertexInputAttributeDescription> attrib_descriptions;
            std::optional<VkPipelineVertexInputStateCreateFlags> flags;
            VkPipelineVertexInputStateCreateInfo get_info()
            {
                VkPipelineVertexInputStateCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                info.flags = flags.value_or(0);
                info.vertexBindingDescriptionCount   = static_cast<uint32_t>(binding_descriptions.size());
                info.pVertexBindingDescriptions      = binding_descriptions.data();
                info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrib_descriptions.size());
                info.pVertexAttributeDescriptions    = attrib_descriptions.data();
                info.pNext = nullptr;
                return info;
            }
        };
        struct graphics_pipeline_desc
        {
            VkDevice parent;

            VkPipelineLayout              pipeline_layout;
            VkRenderPass                       renderpass;
            uint32_t                        subpass_index;

            std::optional<VkPipelineCache>                           pipeline_cache;
            std::optional<VkPipelineDepthStencilStateCreateInfo> depth_stencil_info;
            std::vector<VkPipelineShaderStageCreateInfo> shader_stages_info;
            vertex_input_desc                             vertex_input_info;
            VkPipelineInputAssemblyStateCreateInfo      input_assembly_info;
            dynamic_state_desc                           dynamic_state_info;
            viewport_state_desc                         viewport_state_info;
            VkPipelineRasterizationStateCreateInfo       rasterization_info;
            VkPipelineMultisampleStateCreateInfo           multisample_info;
            color_blend_desc                               color_blend_info;
            std::optional<uint32_t> count;
            VkGraphicsPipelineCreateInfo get_create_info()
            {
                VkGraphicsPipelineCreateInfo pipeline_info{};
                pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                pipeline_info.stageCount = static_cast<uint32_t>(shader_stages_info.size()), pipeline_info.pStages = shader_stages_info.data();

                vertex_input_state = vertex_input_info.get_info();
                pipeline_info.pVertexInputState   =   &vertex_input_state;
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
            VkPipelineVertexInputStateCreateInfo vertex_input_state;
            VkPipelineViewportStateCreateInfo    viewport_state;
            VkPipelineColorBlendStateCreateInfo  color_blend_state;
            VkPipelineDynamicStateCreateInfo     dynamic_state;
        };
        struct pipeline_layout_desc
        {
            VkDevice parent;

            VkPipelineLayoutCreateInfo get_create_info()
            {
                VkPipelineLayoutCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

                return info;        
            }
        };
        struct framebuffer_desc
        {
            VkDevice parent;

            std::vector<VkImageView> attachments;
            VkRenderPass              renderpass;
            uint32_t width, height;
            std::optional<uint32_t> layers ;
            std::optional<VkFramebufferCreateFlags> flags;

            VkFramebufferCreateInfo get_create_info()
            {
                VkFramebufferCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

                info.attachmentCount = static_cast<uint32_t>(attachments.size());
                info.pAttachments    = attachments.data();

                info.renderPass = renderpass;

                info.width  =  width; info.height = height;
                info.layers = layers.value_or(1);
                info.flags  =  flags.value_or(0);

                info.pNext = nullptr;

                return info;
            }
        };
        struct cmd_pool_desc
        {
            VkDevice parent;

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
            VkDevice parent;

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
            VkDevice parent;

            VkSemaphoreCreateInfo get_create_info()
            {
                VkSemaphoreCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                return info;
            }
        };
        struct fence_desc
        {
            VkDevice parent;

            std::optional<VkFenceCreateFlags> flags;
            VkFenceCreateInfo get_create_info()
            {
                VkFenceCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                info.flags = flags.value_or(VK_FENCE_CREATE_SIGNALED_BIT);
                return info;
            }
        };
        struct buffer_desc
        {
            VkDevice parent;

            VkDeviceSize size;
            VkBufferUsageFlags usage;

            std::vector<uint32_t> queue_fam_indices;
            
            std::optional<VkSharingMode> sharing_mode;
            std::optional<VkBufferCreateFlags> flags;
            VkBufferCreateInfo get_create_info()
            {
                VkBufferCreateInfo create_info{};
                create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

                create_info.flags = flags.value_or(0);
                create_info.pNext = nullptr;

                create_info.size  = size;
                create_info.usage = usage;

                create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_fam_indices.size());
                create_info.pQueueFamilyIndices   = queue_fam_indices.data();

                create_info.sharingMode = sharing_mode.value_or(create_info.queueFamilyIndexCount > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE);

                return create_info;
            }
        };
    }


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
        operator description_t() const {return description;}

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

