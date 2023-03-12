#pragma once
#include <chrono>
#include <ctime>

namespace msglib::detail {

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

/**
 * @brief Convert from a POSIX timeval to a POSIX timespec
 * 
 * @param tv - POSIX timeval to convert
 * @param ts - converted POSIX timespec
 * @return true - conversion successful
 * @return false - conversion failed
 */
inline bool Timeval2Timespec( const timeval tv, timespec &ts)
{
    constexpr long SECONDS_TO_USEC = 1000000;
    constexpr long USEC_TO_NSEC = 1000;

    if (tv.tv_usec < SECONDS_TO_USEC) {
        ts = { tv.tv_sec, tv.tv_usec * USEC_TO_NSEC };
        return true;
    }
    return false;
}

}   // namespace msglib::detail