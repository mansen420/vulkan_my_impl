#pragma once

#include "eng_log.h"

#ifdef NDEBUG   //make sure to use the correct CMAKE_BUILD_TYPE!
    constexpr const bool DEBUG_MODE = false;
    #define DEBUG_LOG(MSG)
    #define TYPENAME(TYPE)
#else
    #include <typeinfo>
    #define TYPENAME(HANDLE) typeid(HANDLE).name()
    #define DEBUG_LOG(MSG) ENG_ERR_LOG << __FILE__ << '\t' << "line :" << __LINE__ << '\n' << MSG << std::endl
    constexpr bool DEBUG_MODE = true;
#endif

#define INFORM_ERR(MSG) ENG_ERR_LOG << MSG << std::endl
//use this for warnings or notifications
#define INFORM(MSG) ENG_LOG << MSG << '\n'
#define THROW(ERROR) throw std::runtime_error(ERROR)

#define EXIT_IF(COND, MESSAGE, CLEANUP)     \
        if(COND)                            \
        {                                   \
            CLEANUP();                      \
            if(throws){                     \
                DEBUG_LOG("CRITICAL ERROR");\
                THROW(MESSAGE);}            \
            return false;                   \
        }                                   \

inline void DO_NOTHING(){}

#define DO(STATEMENTS) [=]()mutable{STATEMENTS}