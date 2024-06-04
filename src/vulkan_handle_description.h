#pragma once

#include "volk.h"
#include "vk_mem_alloc.h"

#include <optional>
#include <vector>
#include <set>
#include <array>
#include <stdexcept>
#include <string>
#include <map>

#include "debug.h"


namespace vk_handle
{
    namespace description
    {
        struct instance_extensions
        {
            std::vector<std::string> extensions;
            std::vector<std::string> layers;
        };
        struct instance_desc
        {
            instance_extensions ext_info;
            VkApplicationInfo app_info{};
            std::optional<VkInstanceCreateFlags> flags;
            std::optional<VkDebugUtilsMessengerCreateInfoEXT> debug_messenger_ext;
            VkInstanceCreateInfo get_create_info()
            {
                VkInstanceCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                info.pApplicationInfo = &app_info;
                info.enabledExtensionCount = static_cast<uint32_t>(ext_info.extensions.size());

                extension_names.reserve(ext_info.extensions.size());
                for(auto& name : ext_info.extensions)
                    extension_names.push_back(name.c_str());

                info.ppEnabledExtensionNames = extension_names.data();

                info.enabledLayerCount = static_cast<uint32_t>(ext_info.layers.size());

                layer_names.reserve(ext_info.layers.size());
                for(auto& name : ext_info.layers)
                    layer_names.push_back(name.c_str());
                 
                info.ppEnabledLayerNames = layer_names.data();

                info.flags = flags.value_or(0);
                info.pNext = debug_messenger_ext.has_value() ? &debug_messenger_ext.value() : nullptr;
                return info;
            }
            private:
            std::vector<const char*> extension_names;
            std::vector<const char*> layer_names;
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
            struct ::GLFWwindow;
            GLFWwindow* glfw_interface;
        };
        enum   queue_support_flag_bits
        {
            GRAPHICS_BIT           = 0b0001,
            PRESENT_BIT            = 0b0010,
            TRANSFER_BIT           = 0b0100,
            COMPUTE_BIT            = 0b1000
        };
        struct device_queue  
        {
            uint32_t           family_index;
            VkQueueFlags queue_family_flags;
            float                  priority;

            uint32_t           count;
            uint32_t           flags;
        };
        struct family_index
        {
            int index      = -1;
            uint32_t flags =  0;
            operator uint32_t() const {return index;}
            family_index() : index(-1), flags(0) {}
            family_index(uint32_t index, uint32_t flags) : index(index), flags(flags) {}
            family_index(const family_index& other)
            {
                *this = other;
            }
            family_index& operator =(const family_index& rhs)
            {
                if(this == &rhs)
                    return *this;
                this->index = rhs.index;
                this->flags |= rhs.flags;
                return *this;
            }
            bool operator < (const family_index& rhs)
            {
                return this->index < rhs.index;
            }
        };
        struct family_indices
        {
            std::optional<family_index> graphics, compute, present, transfer;
        };
        struct queue_desc
        {
            uint32_t index_in_family;
            family_index fam_idx;
        };
        struct device_desc
        {
            VkPhysicalDevice phys_device;
            std::vector<device_queue>     device_queues{};
            std::vector<std::string> enabled_extensions{};
            VkPhysicalDeviceFeatures   enabled_features{};
            
            queue_desc graphics_queue{};
            queue_desc transfer_queue{};
            queue_desc  compute_queue{};
            queue_desc  present_queue{};

            VkDeviceCreateInfo get_create_info()
            {
                VkDeviceCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

                for(const auto& queue : device_queues)
                {
                    VkDeviceQueueCreateInfo create_info{};
                    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                    /*
                        per the spec :
                        The queueFamilyIndex member of each element of pQueueCreateInfos must be unique within pQueueCreateInfos
                    */
                    create_info.queueCount = queue.count, create_info.queueFamilyIndex = queue.family_index;
                    create_info.pQueuePriorities = &queue.priority;

                    queue_create_infos.push_back(create_info);
                }

                info.enabledExtensionCount   = static_cast<uint32_t>(enabled_extensions.size());

                //TODO factor thus out
                extension_names.reserve(enabled_extensions.size());
                for(auto& name : enabled_extensions)
                    extension_names.push_back(name.c_str());

                info.ppEnabledExtensionNames = extension_names.data();

                info.enabledLayerCount = 0;

                info.pEnabledFeatures = &enabled_features;

                info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
                info.pQueueCreateInfos    = queue_create_infos.data();

                return info;
            }
        private:
            std::vector<const char*> extension_names;
            std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        };
        struct surface_support
        {
            VkSurfaceCapabilitiesKHR       surface_capabilities;
            std::vector<VkSurfaceFormatKHR>     surface_formats;
            std::vector<VkPresentModeKHR> surface_present_modes;
        };
        struct surface_features
        {
            VkSurfaceFormatKHR             surface_format;
            VkPresentModeKHR                 present_mode;
            VkExtent2D                             extent;
            VkSurfaceCapabilitiesKHR surface_capabilities;
        };
        struct swapchain_desc
        {
            VkDevice parent;

            surface_features             features;
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

                for(const auto& queue : device_queues)
                {
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

            std::vector<VkAttachmentDescription>      attachments{};
            std::vector<subpass_description> subpass_descriptions{};
            std::vector<VkSubpassDependency> subpass_dependencies{};
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
        struct input_assembly_desc
        {
            VkPrimitiveTopology topology;
            VkBool32 primitive_restart_enabled;
            VkPipelineInputAssemblyStateCreateInfo get_input_assembly_info()
            {
                VkPipelineInputAssemblyStateCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                info.primitiveRestartEnable = primitive_restart_enabled;
                info.topology = topology;
                info.flags = 0;
                info.pNext = nullptr;
                return info;
            }
        };
        struct multisample_desc
        {
            VkSampleCountFlagBits rasterization_samples;
            VkBool32 sample_shading_enable;
            VkPipelineMultisampleStateCreateInfo get_multisample_info()
            {
                VkPipelineMultisampleStateCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                info.pNext = nullptr;
                info.flags = 0;
                info.sampleShadingEnable = sample_shading_enable;
                info.rasterizationSamples = rasterization_samples;
                return info;
            }
        };
        struct rasterization_desc
        {
            VkBool32 rasterization_discard;
            VkPolygonMode polygon_mode;
            float line_width;
            VkFrontFace front_face;
            VkCullModeFlags cull_mode;
            VkBool32 depth_clamp_enable;
            VkBool32 depth_bias_enable;
            float depth_bias_clamp, depth_bias_slope_factor, depth_bias_constant_factor;
            VkPipelineRasterizationStateCreateInfo get_rasterization_info()
            {
                VkPipelineRasterizationStateCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
                info.rasterizerDiscardEnable = rasterization_discard;
                info.polygonMode = polygon_mode;
                info.lineWidth = line_width;
                info.frontFace = front_face;
                info.cullMode  = cull_mode;
                info.depthClampEnable = depth_clamp_enable;
                info.depthBiasEnable  = depth_bias_enable;
                info.depthBiasClamp   = depth_bias_clamp;
                info.depthBiasSlopeFactor = depth_bias_slope_factor;
                info.depthBiasConstantFactor = depth_bias_constant_factor;
                return info;
            }

        };
        struct depth_stencil_desc
        {
            VkPipelineDepthStencilStateCreateInfo get_depth_stencil_info()
            {
                VkPipelineDepthStencilStateCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                info.pNext = nullptr;
                info.flags = 0;
                return info;
            }
        };
        struct shader_stage_desc
        {
            VkShaderModule module;
            const char* entry_point;
            std::optional<VkSpecializationInfo> specialization_info;
            VkShaderStageFlagBits stage;
            VkPipelineShaderStageCreateInfo get_shader_stage_info()
            {
                VkPipelineShaderStageCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                info.module = module;
                info.pName  = entry_point;
                info.pNext  = nullptr;
                info.pSpecializationInfo = specialization_info.has_value() ? &specialization_info.value() : nullptr;
                info.stage  = stage;
                return info;
            }
        };
        struct graphics_pipeline_desc
        {
            VkDevice parent;

            VkPipelineLayout              pipeline_layout;
            VkRenderPass                       renderpass;
            uint32_t                        subpass_index;

            std::optional<VkPipelineCache>                   pipeline_cache;
            std::optional<depth_stencil_desc>            depth_stencil_info;
            std::vector<shader_stage_desc>               shader_stages_info;
            vertex_input_desc                             vertex_input_info;
            input_assembly_desc                         input_assembly_info;
            dynamic_state_desc                           dynamic_state_info;
            viewport_state_desc                         viewport_state_info;
            rasterization_desc                           rasterization_info;
            multisample_desc                               multisample_info;
            color_blend_desc                               color_blend_info;
            VkGraphicsPipelineCreateInfo get_create_info()
            {
                VkGraphicsPipelineCreateInfo pipeline_info{};
                pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                pipeline_info.stageCount = static_cast<uint32_t>(shader_stages_info.size());
                for(auto shader : shader_stages_info)
                    shader_stages_state.push_back(shader.get_shader_stage_info());

                pipeline_info.pStages = shader_stages_state.data();

                vertex_input_state = vertex_input_info.get_info();
                pipeline_info.pVertexInputState   =   &vertex_input_state;

                input_assembly_state = input_assembly_info.get_input_assembly_info();
                pipeline_info.pInputAssemblyState = &input_assembly_state;

                viewport_state = viewport_state_info.get_info();
                pipeline_info.pViewportState = &viewport_state;

                raster_state = rasterization_info.get_rasterization_info();
                pipeline_info.pRasterizationState =  &raster_state;

                multisample_state = multisample_info.get_multisample_info();
                pipeline_info.pMultisampleState   =    &multisample_state;
                
                if(depth_stencil_info.has_value())
                {
                    depth_stencil_state = depth_stencil_info.value().get_depth_stencil_info();
                    pipeline_info.pDepthStencilState  =  &depth_stencil_state;
                }
                else
                    pipeline_info.pDepthStencilState = nullptr;

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
            std::vector<VkPipelineShaderStageCreateInfo> shader_stages_state;
            VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
            VkPipelineRasterizationStateCreateInfo raster_state;
            VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
            VkPipelineVertexInputStateCreateInfo vertex_input_state;
            VkPipelineViewportStateCreateInfo    viewport_state;
            VkPipelineColorBlendStateCreateInfo  color_blend_state;
            VkPipelineDynamicStateCreateInfo     dynamic_state;
            VkPipelineMultisampleStateCreateInfo multisample_state;
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

            uint32_t       queue_fam_index;

            VkCommandPoolCreateFlags flags;

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

            VmaAllocator allocator;
            VmaAllocationCreateInfo alloc_info{};

            VkDeviceSize size;
            VkBufferUsageFlags usage;

            std::vector<uint32_t> queue_fam_indices;



            std::optional<VkSharingMode> sharing_mode;
            std::optional<VkBufferCreateFlags> flags;

            //we store this object here just for VMA. Don't mess with this.
            VmaAllocation allocation_object;

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
        struct memory_desc
        {
            VkDevice parent;

            VkDeviceSize     size;
            uint32_t memory_type_index;
            VkMemoryAllocateInfo get_info()
            {
                VkMemoryAllocateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                info.pNext = nullptr;
                info.allocationSize  = size;
                info.memoryTypeIndex = memory_type_index;
                return info;
            }
        };
        }

} 
    