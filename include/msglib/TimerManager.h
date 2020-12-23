#pragma once
#include "Mailbox.h"
#include "TimeConv.h"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace msglib {

    /**
     * @brief Types of timers
     * 
     */
    enum TimerType_e {
        PERIODIC,
        ONE_SHOT
    };

/**
 * @brief Timer is a representation of a timer which has been scheduled within the TimerManager
 * 
 */
class Timer {
public:

    Timer(Mailbox &mailbox, Label label, timespec time, TimerType_e type);

    void cancel();

    void timerEvent();

private:
    Mailbox &m_mailbox;
    Label m_label = 0;
    timer_t m_timer = nullptr;
    TimerType_e m_type = ONE_SHOT;
    struct sigevent m_sev {};
    struct sigaction m_sa {};
    struct itimerspec m_spec {};

    static void handler(int /* sig */, siginfo_t *si, void * /* uc */);
};    

/**
 * @brief TimerManagerData is the centralized representation of all timers managed by
 *        the TimerManager.
 */
class TimerManagerData {
public:

    TimerManagerData() = default;

    void startTimer(const Label &label, const timespec &time, const TimerType_e type);

    void cancelTimer(const Label &label);

private:
    using TimerMap = std::unordered_map<Label,Timer>;

    Mailbox m_mailbox;
    TimerMap m_timers;
    std::mutex m_mutex;
};

/**
 * @brief TimerManager supports one-shot and recurring timers which result in specific signals being sent 
 *        as signals to the mailbox for processing by other thread(s)
 * 
 */
class TimerManager {
public:

    /**
     * @brief Start a one-shot or recurring timer resulting in the specified label being signalled
     * 
     * @param label - label to use for this timer
     * @param time - time specification of when the timer should fire as POSIX timespec
     * @param type - type of timer to create (default is one-shot)
     */
    static void StartTimer(const Label &label, const timespec &time, const TimerType_e type = ONE_SHOT);

    /**
     * @brief Start a one-shot or recurring timer resulting in the specified label being signalled
     * 
     * @tparam T - std::chrono::duration representation class
     * @tparam P - std::chrono::duration period class
     * @param label - label to use for this timer
     * @param time - time expressed as a std::chrono::duration 
     * @param type - type of timer to create (default is one-shot)
     */
    template< class T, class P>
    static void StartTimer(const Label &label, const std::chrono::duration<T,P> time, const TimerType_e type = ONE_SHOT)
    {
        auto ts = Chrono2Timespec(time);
        s_timerData->startTimer(label, ts, type);
    }

    /**
     * @brief Start a one-shot timer resulting in the specified label being signalled at a specific time
     * 
     * @param label - label to use for this timer
     * @param time - time expressed as a std::chrono::time_point
     */
    static void StartTimer(const Label &label, const std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> &time);

    /**
     * @brief Cancel a one-shot timer for the specified label
     * 
     * @param label - timer to be cancelled
     */
    static void CancelTimer(const Label &label);

private:
    static std::unique_ptr<TimerManagerData> s_timerData;

};

}   // namespace msglib