#include "vulkan_handle.h"

#include <memory>


#define DEL(handle_t) [](handle_t* X)mutable{ \
    if(*X){X->destroy();}                     \
    else{INFORM_ERR("WARNING : destroying empty : " << typeid(X).name());}\
    }                                         \

template <typename handle_t> std::shared_ptr<handle_t> make_shared()
{                                                               
    return std::shared_ptr<handle_t>(new handle_t, DEL(handle_t));  
}

#define MAKE_SHARED(handle)                                     \
template <> std::shared_ptr<handle> make_shared<handle>()\
{                                                               \
    return std::shared_ptr<handle>(new handle, DEL(handle));    \
}                                                               \

MAKE_SHARED(vk_handle::instance)
MAKE_SHARED(vk_handle::surface)
MAKE_SHARED(vk_handle::device)
MAKE_SHARED(vk_handle::swapchain)
MAKE_SHARED(vk_handle::debug_messenger)
MAKE_SHARED(vk_handle::image_view)
MAKE_SHARED(vk_handle::renderpass)
MAKE_SHARED(vk_handle::framebuffer)
MAKE_SHARED(vk_handle::shader_module)
MAKE_SHARED(vk_handle::graphics_pipeline)
MAKE_SHARED(vk_handle::cmd_pool)
MAKE_SHARED(vk_handle::cmd_buffers)
MAKE_SHARED(vk_handle::semaphore)
MAKE_SHARED(vk_handle::fence)
MAKE_SHARED(vk_handle::buffer)


#undef MAKE_SHARED
#undef DEL