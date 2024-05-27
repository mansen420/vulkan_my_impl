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

using namespace vk_handle;
using namespace vk_handle::description;

instance VULKAN;

//terminates 3rd party libraries and all Vulkan objects
void terminate()
{
    TERMINATION_QUEUE.flush();
}


//just for init()
#define EXIT_IF(COND, MESSAGE)       \
        if(COND)                     \
        {                            \
            terminate();             \
            DEBUG_LOG("INIT FAILED");\
            if(throws)               \
                THROW(MESSAGE);      \
            return false;            \
        }                            \

//Initializes all third party dependencies, as well as the Vulkan instance, debugger, and physical device.
//In case of failure, returns false, and, if throws is set, throws runtime error.
bool init(const char* app_name, bool throws = true)
{
    //this is vulkan baby
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    EXIT_IF(glfwInit() == GLFW_FALSE, "GLFW INIT FAILED");

    TERMINATION_QUEUE.push(DO(glfwTerminate();));
    
    EXIT_IF(volkInitialize(), "VOLK INIT FAILED");

    TERMINATION_QUEUE.push(DO(volkFinalize();));

    instance_desc description{};
    try
    {
        description = get_instance_description(app_name);
    }
    catch(const std::exception& e)
    {
        EXIT_IF(true, e.what());
    }

    EXIT_IF(VULKAN.init(description) ,"VULKAN INSTANTIATION FAILED");
    
    TERMINATION_QUEUE.push(DO(VULKAN.destroy();));

    volkLoadInstance(VULKAN);

    if(DEBUG_MODE)
    {
        debug_messenger db;
        debug_messenger_desc db_desc{};
        db_desc.parent = VULKAN;
        db_desc.create_info = get_debug_create_info();
        EXIT_IF(db.init(db_desc), "DEBUGGER INIT FAILED");

        TERMINATION_QUEUE.push(DO(db.destroy();));
    }

#ifdef COOL
    ENG_LOG << "Vulkan speaking, yes?\n";
    ENG_LOG << "This is vulkan , baby!\n";
#endif
    return true;
#undef EXIT
}

int main()
{
    init("myApp");

    terminate();
}