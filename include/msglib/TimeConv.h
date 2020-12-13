#pragma once
#include <chrono>
#include <ctime>

namespace msglib {

/**
 * @brief Convert from a std::chrono::duration type to a POSIX timespec
 * 
 * @tparam T - duration representation type
 * @tparam P - duration period type
 * @param dur - duration to be converted
 * @return timespec - POSIX timespec representation
 */
template< class T, class P>
timespec Chrono2Timespec( std::chrono::duration<T,P> dur)
{
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
    dur -= secs;

    return timespec{secs.count(), std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count()};
}

}