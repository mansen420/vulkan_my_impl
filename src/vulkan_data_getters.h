#pragma once

#include "vulkan_handle.h"
#include "vulkan_handle_description.h"
#include "vulkan_handle_util.h"

#include "GLFW/glfw3.h"


namespace vk_handle::data_getters
{
    namespace instance
    {
        enum extension_enable_flag_bits
        {
            GLFW              = 0b0001,
            DEBUG             = 0b0010,
        };
        std::vector<std::string> get_required_extension_names(uint flags)
        {
            std::vector<std::string> names;
            if(flags & GLFW)
            {
                uint32_t count;
                auto ptr = glfwGetRequiredInstanceExtensions(&count);
                names.reserve(names.size() + count);
                for(uint32_t i = 0; i < count; ++i)
                    names.push_back(std::string(ptr[i]));
            }
            if(flags & DEBUG)
                names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            return names;
        }
        std::vector<std::string> get_required_layer_names(uint flags)
        {
            std::vector<std::string> names;
            if(flags & DEBUG)
                names.push_back("VK_LAYER_KHRONOS_validation");
            return names;
        }
        std::vector<std::string> get_available_instance_extension_names()
        {
            uint32_t count;
            vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
            VkExtensionProperties* ptr = new VkExtensionProperties[count]; //never use a pointer uninitialized!! THE COMPILER WANRED ME ABOUT THIS
            vkEnumerateInstanceExtensionProperties(nullptr, &count, ptr);
            std::vector<std::string> names;
            names.reserve(count);
            for(size_t i = 0; i < count; ++i)
                names.push_back(std::string(ptr[i].extensionName));
            delete[] ptr;
            return names;
        }
        std::vector<std::string> get_available_instance_layers_names()
        {
            uint32_t count;
            vkEnumerateInstanceLayerProperties(&count, nullptr);
            VkLayerProperties* ptr = new VkLayerProperties[count];
            vkEnumerateInstanceLayerProperties(&count, ptr);
            std::vector<std::string> names(count);
            for(size_t i = 0; i < count; ++i)
                names[i] = std::string(ptr[i].layerName);
            delete[] ptr;
            return names;
        }
        vk_handle::description::instance_desc get_instance_description(uint ext_flags, uint layer_flags)
        {
            vk_handle::description::instance_desc description{};

            std::vector<std::string> req_ext;
            req_ext = get_required_extension_names(ext_flags);
            
            std::vector<std::string> req_layers;
            req_layers = get_required_layer_names(layer_flags);

            auto names = get_available_instance_extension_names();
            if(!check_support(names, req_ext))
                INFORM_ERR("requested instance extension not supported!");
            names = get_available_instance_layers_names();
            if(!check_support(names, req_layers))
                INFORM_ERR("requested instance layer not supported!");

            description.ext_info.extensions = req_ext;
            description.ext_info.layers     = req_layers;
            description.app_info            = get_app_info();

            if((layer_flags & DEBUG) xor (ext_flags & DEBUG))
                INFORM("WARNING : conflicting flags in extension and layers!");
            if((layer_flags & DEBUG )|| (ext_flags & DEBUG))
                description.debug_messenger_ext = get_debug_create_info();
            
            return description;
        }
    };

    namespace physical_device
    {
        std::vector<VkPhysicalDevice> find_physical_devices(VkInstance instance)
        {
            std::vector<VkPhysicalDevice> physical_devices;
            auto handles = get_physical_device_handles(instance);
            physical_devices.reserve(handles.size());
            for(const auto& handle : handles)
            {
                physical_devices.push_back(handle);
                auto props = get_properties(handle);
                INFORM("Physical device determined : " << props.deviceName);
            }
            return physical_devices;
        }
        bool supports_extensions(VkPhysicalDevice device, std::vector<std::string> extensions)
        {
            auto props = get_properties(device);
            INFORM(props.deviceName << " : ");
            auto avlbl_ext = get_available_extensions(device);
            return check_support(avlbl_ext, extensions);
        }
        VkPhysicalDevice pick_best_physical_device(std::vector<VkPhysicalDevice> devices)
        {
            std::map<VkDeviceSize, VkPhysicalDevice> device_memory_size; //sorted ascending
            for(size_t i = 0; i < devices.size(); ++i)
            {
                device_memory_size.insert({get_local_memory_size(devices[i]), devices[i]});
            }
            
            auto picked_device = (*device_memory_size.rbegin()).second;
            auto device_mem_size = (*device_memory_size.rbegin()).first;
            for(auto itr = device_memory_size.rbegin(); itr != device_memory_size.rend(); ++itr)
            {
                auto props = get_properties((*itr).second);
                if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                || props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                {
                    picked_device = (*itr).second;
                    device_mem_size = (*itr).first;
                }
            }
            auto props = get_properties(picked_device);
            INFORM("Picked " << props.deviceName << "\nWith " << device_mem_size << " Bytes of local memory.");

            return picked_device;
        }
        VkDeviceSize get_local_memory_size(VkPhysicalDevice physical_device)
        {
            VkDeviceSize device_memory_size{};
            auto mem_props = get_memory_properties(physical_device);
            for(uint32_t j = 0; j < mem_props.memoryHeapCount; ++j)
                if(mem_props.memoryHeaps[j].flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    device_memory_size += mem_props.memoryHeaps[j].size;
            return device_memory_size;
        }
        
        enum extension_enable_flag_bits
        {
            SWAPCHAIN         = 0b001
        };
        std::vector<std::string> get_required_extension_names(uint flags)
        {
            std::vector<std::string> names;
            if(flags & SWAPCHAIN)
                names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            return names;
        }

        std::vector<VkQueueFamilyProperties> get_queue_fams(VkPhysicalDevice handle)
        {
            std::vector<VkQueueFamilyProperties> queue_fams;
            uint32_t count;
            vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, nullptr);
            queue_fams.resize(count);
            vkGetPhysicalDeviceQueueFamilyProperties(handle, &count, queue_fams.data());
            return queue_fams;
        }
        std::vector<std::string> get_available_extensions(VkPhysicalDevice handle)
        {
            std::vector<std::string> available_extensions;
            uint32_t count;
            vkEnumerateDeviceExtensionProperties(handle, nullptr, &count, nullptr);
            VkExtensionProperties* ptr = new VkExtensionProperties[count];
            vkEnumerateDeviceExtensionProperties(handle, nullptr, &count, ptr);

            available_extensions.resize(count);
            for(size_t i = 0; i < count; ++i)
                available_extensions[i] = std::string(ptr[i].extensionName);
            delete[] ptr;
            return available_extensions;
        }
        VkPhysicalDeviceMemoryProperties get_memory_properties(VkPhysicalDevice handle)
        {
            VkPhysicalDeviceMemoryProperties memory_properties;
            vkGetPhysicalDeviceMemoryProperties(handle, &memory_properties);
            return memory_properties;
        }
        VkPhysicalDeviceProperties get_properties(VkPhysicalDevice handle)
        {
            VkPhysicalDeviceProperties f;
            vkGetPhysicalDeviceProperties(handle, &f);
            return f;
        }
        VkPhysicalDeviceFeatures get_features(VkPhysicalDevice handle)
        {
            VkPhysicalDeviceFeatures f;
            vkGetPhysicalDeviceFeatures(handle, &f);
            return f;
        }
        //don't call this frequently
        vk_handle::description::surface_features get_surface_features(VkInstance instance, VkPhysicalDevice handle)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            auto glfw = glfwCreateWindow(1, 1, "", nullptr, nullptr);
            vk_handle::surface srf(vk_handle::description::surface_desc{instance, glfw});
            auto result = get_swapchain_features(get_swapchain_support(handle, srf), glfw);   
            glfwDestroyWindow(glfw);
            return result;
        }
    };

    namespace device
    {
        VkQueue queue_handle(const VkDevice device, const vk_handle::description::queue_desc queue_desc)
        {
            VkQueue handle;
            vkGetDeviceQueue(device, queue_desc.fam_idx, queue_desc.index_in_family, &handle);
            return handle;
        }
        bool find_queue_indices(const VkInstance instance, const VkPhysicalDevice phys_device, vk_handle::description::family_indices& indices,
        bool throws = true)
        {
            auto queue_fams = physical_device::get_queue_fams(phys_device);

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            auto glfw_window = glfwCreateWindow(1, 1, "dummy", nullptr, nullptr);
            vk_handle::surface dummy_surface{vk_handle::description::surface_desc{instance, glfw_window}}; //just to check support

            //find family indices
            for(uint32_t i = 0; i < queue_fams.size(); ++i)
            {
                using namespace vk_handle::description;
                if(queue_fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    indices.graphics = family_index{i, GRAPHICS_BIT};
                if(queue_fams[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
                    if((queue_fams[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0)
                        indices.transfer = family_index{i, TRANSFER_BIT};
                if(queue_fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                    if((queue_fams[i].queueCount & VK_QUEUE_GRAPHICS_BIT) == 0)
                        indices.compute  = family_index{i, COMPUTE_BIT};

                VkBool32 supports_present;
                vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, dummy_surface, &supports_present);
                if(supports_present)
                    indices.present = family_index{i, PRESENT_BIT};
            }
            //cleanup
            glfwDestroyWindow(glfw_window);
            //find fallbacks
            if(!indices.transfer.has_value())   //use a graphics queue
            {
                indices.transfer = indices.graphics;
                indices.transfer.value().flags = vk_handle::description::TRANSFER_BIT;
            }
            if(!indices.compute.has_value())    //find ANY compute queue
                for(uint32_t i = 0; i < queue_fams.size(); ++i)
                    if(queue_fams[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                    {
                        using namespace vk_handle::description;
                        indices.compute = family_index{i, COMPUTE_BIT};
                    }
            bool found_all_families = indices.compute.has_value() && indices.graphics.has_value() && indices.present.has_value() && indices.transfer.has_value();
            EXIT_IF(!found_all_families, "FAILED TO FIND QUEUE FAMILIES", DO_NOTHING);
            return true;
        }

        //sets the queue members of device and the value you should pass to the handle's description
        //this function does too much. It should be split into two functions
        bool determine_queues(const VkInstance instance, vk_handle::description::device_desc& device, VkPhysicalDevice phys_dev,
        std::vector<vk_handle::description::device_queue>& device_queues, bool throws = true)
        {
            using namespace vk_handle::description;

            family_indices indices;
            find_queue_indices(instance, phys_dev, indices, throws);
            //determine device queues 
            /*
            the spec states that each device queue should refer to a unique family index.
            Since the family indices above are not necessarily unique, we must check for that
            */
            auto queue_fams = physical_device::get_queue_fams(phys_dev);

            //combine non-unique indices
            std::vector<family_index> unique_indices({indices.graphics.value(), indices.compute.value(), indices.transfer.value(), indices.present.value()});
            for(auto itri = unique_indices.begin(); itri != unique_indices.end(); ++itri)
            {
                auto& index = (*itri);
                for(auto itrj = unique_indices.begin(); itrj != unique_indices.end(); ++itrj)
                {
                    if(itrj == itri)
                        continue;
                    auto& candidate = itrj;
                    if(index.index == (*candidate).index)
                    {
                        index = *candidate; //combines the flags 
                        (*candidate).flags = 0; //"erase" candidate
                    }
                }
            }
            for(const auto& index : unique_indices)
            {
                if(index.flags == 0)
                    continue;
                using namespace vk_handle::description;
                uint32_t index_queue_count = 0;
                if(index.flags & GRAPHICS_BIT)
                {
                    index_queue_count++;
                    if(queue_fams[index].queueCount < index_queue_count)
                        index_queue_count--;
                    device.graphics_queue.fam_idx = index;
                    device.graphics_queue.index_in_family = index_queue_count - 1;

                }
                if(index.flags & COMPUTE_BIT)
                {
                    index_queue_count++;
                    if(queue_fams[index].queueCount < index_queue_count)
                        index_queue_count--;
                    device.compute_queue.fam_idx = index;
                    device.compute_queue.index_in_family = index_queue_count - 1;
                }
                if(index.flags & PRESENT_BIT)
                {
                    index_queue_count++;
                    if(queue_fams[index].queueCount < index_queue_count)
                        index_queue_count--;
                    device.present_queue.fam_idx = index;
                    device.present_queue.index_in_family = index_queue_count - 1;
                }
                if(index.flags & TRANSFER_BIT)
                {
                    index_queue_count++;
                    if(queue_fams[index].queueCount < index_queue_count)
                        index_queue_count--;
                    device.transfer_queue.fam_idx = index;
                    device.transfer_queue.index_in_family = index_queue_count - 1;
                }
                vk_handle::description::device_queue device_queue{};
                device_queue.count = index_queue_count;
                if(index.index < 0)
                    INFORM_ERR("WARNING : using negative family index");
                device_queue.family_index = index.index;
                device_queue.flags = index.flags;
                device_queue.queue_family_flags = queue_fams[index].queueFlags; //eh why not
                device_queues.push_back(device_queue);
            }
            //determine priority
            for(auto& queue : device_queues)
            {
                float priority = 0.0f;
                using namespace vk_handle::description;
                if(queue.flags & GRAPHICS_BIT)
                    priority += 1.0f;
                if(queue.flags & TRANSFER_BIT)
                    priority += 0.5f;
                if(queue.flags & COMPUTE_BIT)
                    priority += 0.5f;
                if(queue.flags & PRESENT_BIT)
                    priority += 0.25f;
                priority = std::clamp(priority, 0.0f, 1.0f);
                queue.priority = priority;
            }
            
            return true;
        }
        void report_device_queues(const vk_handle::device& device)
        {
            auto list_queue_props = [](const vk_handle::description::queue_desc& q)
            {
                INFORM("Family index : " << q.fam_idx.index << " Index within family : " << q.index_in_family);
            };
            auto desc = device.description;
            INFORM("Device queues\nGraphics Queue");
            list_queue_props(desc.graphics_queue);
            INFORM("Compute Queue");
            list_queue_props(desc.compute_queue);
            INFORM("Transfer Queue");
            list_queue_props(desc.transfer_queue);
            INFORM("Present Queue");
            list_queue_props(desc.present_queue);
        }
        
        vk_handle::description::device_desc description(const VkInstance instance, const VkPhysicalDevice phys_device)
        {
            vk_handle::description::device_desc description{};
            description.enabled_features   = physical_device::get_features(phys_device);
            description.phys_device        = phys_device;
            determine_queues(instance, description, phys_device, description.device_queues);
            //XXX watch out for lack of support here 
            description.enabled_extensions = physical_device::get_required_extension_names(physical_device::SWAPCHAIN);

            return description;
        }
    };

    namespace swapchain
    {
        vk_handle::description::swapchain_desc description(const vk_handle::surface& srf, const vk_handle::device& device, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE)
        {
            vk_handle::description::swapchain_desc desc{};
            desc.surface = srf;
            desc.features = get_swapchain_features(get_swapchain_support(device.description.phys_device, srf),
            srf.description.glfw_interface);
            desc.device_queues = device.description.device_queues;
            desc.parent = device;
            desc.old_swapchain = old_swapchain;
            return desc;
        }
    };

    namespace memory
    {
        //determines index in physical device memory properties for this buffer's requirements with a user-defined bitmask
        uint32_t get_memory_type_index(uint32_t memory_type_bitmask, VkMemoryRequirements mem_reqs, VkPhysicalDevice phys_dev)
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
        VkMemoryAllocateInfo get_mem_alloc_info(uint32_t memory_type_bitmask, VkBuffer buffer, VkDevice parent, VkPhysicalDevice phys_dev)
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
    }
}