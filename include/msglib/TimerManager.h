#pragma once

namespace msglib {

/**
 * @brief Types of timers
 */
enum TimerType_e { PERIODIC, ONE_SHOT };

}

#include "Mailbox.h"
#include "detail/TimeConv.h"
#include "detail/TimerManagerData.h"
#include <chrono>
#include <cstdint>
#include <memory>

namespace msglib {

/**
 * @brief TimerManager supports one-shot and recurring timers which result in specific signals being sent
 *        as signals to the mailbox for processing by other thread(s)
 */
class TimerManager {
public:
    static bool Initialize() {
        sigset_t sigset;
        if (sigemptyset(&sigset) != 0) {
            return false;
        }
        if (sigaddset(&sigset, SIGRTMIN) != 0) {
            return false;
        }
        if (sigprocmask(SIG_BLOCK, &sigset, nullptr) != 0) {  // NOLINT
            return false;
        }
        if (pthread_sigmask(SIG_BLOCK, &sigset, nullptr) != 0) {
            return false;
        }

        return s_timerData.Initialize();
    }

    /**
     * @brief Start a one-shot or recurring timer resulting in the specified label being signalled
     *
     * @param label - label to use for this timer
     * @param time - time specification of when the timer should fire as POSIX timespec
     * @param type - type of timer to create (default is one-shot)
     * @return true - timer started successfully
     * @return false - timer not started
     */
    static bool StartTimer(const Label &label, const timespec &time, const TimerType_e type = ONE_SHOT) {
        return s_timerData.startTimer(label, time, type);
    }

    /**
     * @brief Start a one-shot or recurring timer resulting in the specified label being signalled
     * 
     * @param label - label to use for this timer
     * @param time - time specification of when the timer should fire as POSIX timeval
     * @param type - type of timer to create (default is one-shot)
     * @return true - timer started successfully
     * @return false - timer not started
     */
    static bool StartTimer(const Label &label, const timeval &time, const TimerType_e type = ONE_SHOT) {
        timespec ts { 0, 0 };
        if (detail::Timeval2Timespec(time,ts)) {
            return s_timerData.startTimer(label, ts, type);
        }
        return false;
    }

    /**
     * @brief Start a one-shot or recurring timer resulting in the specified label being signalled
     *
     * @tparam T - std::chrono::duration representation class
     * @tparam P - std::chrono::duration period class
     * @param label - label to use for this timer
     * @param time - time expressed as a std::chrono::duration
     * @param type - type of timer to create (default is one-shot)
     * @return true - timer started successfully
     * @return false - timer not started
     */
    template <class T, class P>
    static bool StartTimer(const Label &label, const std::chrono::duration<T, P> time, const TimerType_e type = ONE_SHOT) {
        auto ts = detail::Chrono2Timespec(time);
        return s_timerData.startTimer(label, ts, type);
    }

    /**
     * @brief Start a one-shot timer resulting in the specified label being signalled at a specific time
     *
     * @param label - label to use for this timer
     * @param time - time expressed as a std::chrono::time_point
     * @return true - timer started successfully
     * @return false - timer not started
     */
    template <typename C, typename D>
    static bool StartTimer(const Label &label, std::chrono::time_point<C, D> &time) {
        auto secs = std::chrono::time_point_cast<std::chrono::seconds>(time);
        auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(time) -
            std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

        return s_timerData.startTimer(label, timespec {secs.time_since_epoch().count(), ns.count()}, ONE_SHOT);
    }

    /**
     * @brief Cancel a one-shot timer for the specified label
     *
     * @param label - timer to be cancelled
     * @return true - timer cancelled successfully
     * @return false - timer not cancelled
     */
    static bool CancelTimer(const Label &label) {
        return s_timerData.cancelTimer(label);
    }

private:
    inline static detail::TimerManagerData s_timerData;
};

}  // namespace msglib