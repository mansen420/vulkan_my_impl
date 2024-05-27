#include "volk.h"
#include "GLFW/glfw3.h" //for surface

#include "vulkan_handle.h"
#include "vulkan_handle_util.h"


//In theory, this module handles all communication with vulkan...(?)


#ifndef NDEBUG
    #include <typeinfo>
    #define DEBUG_LOG(MSG) ENG_ERR_LOG << __FILE__ << '\t' << "line :" << __LINE__ << '\n' << MSG << std::endl
#else
    #define DEBUG_LOG(MSG)
#endif
#define THROW(ERROR) throw std::runtime_error(ERROR)
#define DO(STATEMENTS) [=]()mutable{STATEMENTS}

struct destruction_queue
{
    void reserve_extra(size_t functions)
    {
        queue.reserve(queue.size() + functions);
    }
    void push(std::function<void()> statement)
    {
        queue.push_back(statement);
    }
    void flush()
    {
        //call destruction functions in LIFO order 
        for(auto itr = queue.rbegin(); itr != queue.rend(); itr++)
            (*itr)();
    }
private:
    std::vector<std::function<void()>> queue;
};
destruction_queue MAIN_DESTRUCTION_QUEUE, TERMINATION_QUEUE;

vk_handle::instance VULKAN;

//terminates 3rd party libraries and all Vulkan objects
void terminate()
{
    TERMINATION_QUEUE.flush();
}


#define EXIT_IF(COND, MESSAGE, CLEANUP) \
        if(COND)                        \
        {                               \
            CLEANUP();                  \
            DEBUG_LOG("CRITICAL ERROR");\
            if(throws)                  \
                THROW(MESSAGE);         \
            return false;               \
        }                               \

//Aesthetic Interactive Computing Engine
//愛子ーアイコ　
namespace AiCo
{
    //client code
    class instance
    {
        //public info
    private:
        instance_t* impl;
    };
    //server side 
    struct  instance_t
    {
        //vulkan stuff 
    };
};

void DO_NOTHING(){}

using namespace vk_handle;
using namespace vk_handle::description;
//Initializes all third party dependencies, as well as the Vulkan instance, debugger, and physical device.
//In case of failure, returns false, and, if throws is set, throws runtime error.
bool init(const char* app_name, AiCo::instance& AiCo_instance, bool throws = true)
{
    //this is vulkan baby
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    EXIT_IF(glfwInit() == GLFW_FALSE, "GLFW INIT FAILED", terminate);

    TERMINATION_QUEUE.push(DO(glfwTerminate();));
    
    EXIT_IF(volkInitialize(), "VOLK INIT FAILED", terminate);

    TERMINATION_QUEUE.push(DO(volkFinalize();));

    instance_desc description{};
    try
    {
        description = get_instance_description(app_name);
    }
    catch(const std::exception& e)
    {
        EXIT_IF(true, e.what(), terminate);
    }

    EXIT_IF(VULKAN.init(description) ,"VULKAN INSTANTIATION FAILED", terminate);
    
    TERMINATION_QUEUE.push(DO(VULKAN.destroy();));

    volkLoadInstance(VULKAN);

    if(DEBUG_MODE)
    {
        debug_messenger db;
        EXIT_IF(db.init({VULKAN, get_debug_create_info()}), "DEBUGGER INIT FAILED", terminate);

        TERMINATION_QUEUE.push(DO(db.destroy();));
    }

    TERMINATION_QUEUE.push(DO(MAIN_DESTRUCTION_QUEUE.flush();));

    MAIN_DESTRUCTION_QUEUE.reserve_extra(15);

#ifdef COOL
    ENG_LOG << "Vulkan speaking, yes?\n";
    ENG_LOG << "This is vulkan , baby!\n";
#endif
    return true;
}

bool create_window(int width, int height, const char* title, bool throws = true)
{
    auto window_ptr = glfwCreateWindow(width, height, title, nullptr, nullptr);
    surface srf;
    EXIT_IF(srf.init({VULKAN, window_ptr}), "FAILED TO CREATE WINDOW", DO_NOTHING)
    
}

int main()
{
    AiCo::instance myApp;
    init("myApp", myApp);

    terminate();
}