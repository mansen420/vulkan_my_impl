#include "vulkan_handle.h"

#include <memory>

/*
deprecated due to RAII
#define DEL(handle_t) [](handle_t* X)mutable{ \
    if(*X){X->destroy();}                     \
    else{INFORM_ERR("WARNING : destroying empty " << typeid(X).name());}\
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
*/

#define CONST_SHARED_DECL(TYPENAME) typedef std::shared_ptr<const vk_handle:: TYPENAME> shared_ ## TYPENAME ;
namespace vk_handle
{
    CONST_SHARED_DECL(instance)
    CONST_SHARED_DECL(surface)
    CONST_SHARED_DECL(device)
    CONST_SHARED_DECL(swapchain)
    CONST_SHARED_DECL(debug_messenger)
    CONST_SHARED_DECL(image_view)
    CONST_SHARED_DECL(renderpass)
    CONST_SHARED_DECL(framebuffer)
    CONST_SHARED_DECL(shader_module)
    CONST_SHARED_DECL(graphics_pipeline)
    CONST_SHARED_DECL(cmd_pool)
    CONST_SHARED_DECL(cmd_buffers)
    CONST_SHARED_DECL(semaphore)
    CONST_SHARED_DECL(fence)
    CONST_SHARED_DECL(buffer)
}


#undef MAKE_SHARED
#undef DEL