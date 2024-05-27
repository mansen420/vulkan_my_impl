#pragma once

#ifdef NDEBUG   //make sure to use the correct CMAKE_BUILD_TYPE!
    constexpr const bool DEBUG_MODE = false;
#else
    constexpr bool DEBUG_MODE = true;
#endif
