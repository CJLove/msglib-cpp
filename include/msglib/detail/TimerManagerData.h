#pragma once

#include "msglib/Mailbox.h"
#include "msglib/TimerManager.h"
#include <atomic>
#include <csignal>
#include <ctime>
#include <map>
#include <mutex>
#include <thread>

namespace msglib {

namespace detail {

/**
 * @brief Forward declaration
 */
class TimerManagerData;

/**
 * @brief Timer is a representation of a timer which has been scheduled within the TimerManager
 *
 */
class Timer {
public:
    Timer(Mailbox &mailbox, TimerManagerData &timerMgrData, Label label, timespec time, TimerType_e type)
        : m_mailbox(mailbox), m_timerManagerData(timerMgrData), m_label(label), m_type(type) {
        m_sev.sigev_notify = SIGEV_SIGNAL;
        m_sev.sigev_signo = SIGRTMIN;
        m_sev.sigev_value.sival_ptr = this;
        if (timer_create(CLOCK_MONOTONIC, &m_sev, &m_timer) == -1) {
            throw std::runtime_error("Couldn't create timer");
        }

        switch (type) {
        case PERIODIC:
            m_spec.it_value = time;
            m_spec.it_interval = time;
            break;
        case ONE_SHOT:
            m_spec.it_value = time;
            m_spec.it_interval.tv_sec = m_spec.it_interval.tv_nsec = 0;
            break;
        }
        if (timer_settime(m_timer, 0, &m_spec, nullptr) == -1) {
            throw std::runtime_error("Couldn't start timer");
        }
    }

    ~Timer() {
        timer_delete(m_timer);
    }

    void cancel() {
        itimerspec itsnew {};
        itsnew.it_value.tv_sec = itsnew.it_value.tv_nsec = 0;
        itsnew.it_interval.tv_sec = itsnew.it_interval.tv_nsec = 0;
        timer_settime(m_timer, 0, &itsnew, &m_spec);
    }

    void timerEvent();

private:
    Mailbox &m_mailbox;
    TimerManagerData &m_timerManagerData;
    Label m_label = 0;
    timer_t m_timer = nullptr;
    TimerType_e m_type = ONE_SHOT;
    struct sigevent m_sev { };
    struct itimerspec m_spec { };
};

/**
 * @brief TimerResources encapsulates shared resources held by the TimerManagerData class
 *
 */
struct TimerResources {
    using TimerMap = std::map<Label, Timer>;

    /**
     * @brief HandleSignals is a thread which handles SIGRTMIN which indicates that a timer
     *        has fired
     */
    void HandleSignals() {
        constexpr int64_t NSEC_DELAY = 500000000;
        struct timespec ts {
            0, NSEC_DELAY
        };
        sigset_t sigset;
        if (sigemptyset(&sigset) != 0) {
            throw std::runtime_error("sigemptyset error");
        }
        if (sigaddset(&sigset, SIGRTMIN) != 0) {
            throw std::runtime_error("sigaddset error");
        }
        if (pthread_sigmask(SIG_BLOCK, &sigset, nullptr) != 0) {
            throw std::runtime_error("pthread_sigmask error");
        }

        while (!m_shutdown) {
            siginfo_t info = {};
            int result = sigtimedwait(&sigset, &info, &ts);
            if (result == SIGRTMIN) {
                std::lock_guard<std::recursive_mutex> guard(m_mutex);

                auto *timer = reinterpret_cast<Timer *>(info.si_value.sival_ptr);
                timer->timerEvent();
            }
        }
    }

    TimerResources(std::recursive_mutex &mutex) : m_mutex(mutex), m_thread(std::thread(&TimerResources::HandleSignals, this)) {
    }

    ~TimerResources() {
        m_shutdown = true;
        m_thread.join();
    }

    Mailbox m_mailbox;
    TimerMap m_timers;
    std::atomic<bool> m_shutdown = false;
    std::recursive_mutex &m_mutex;
    std::thread m_thread;
};

/**
 * @brief TimerManagerData is the centralized representation of all timers managed by
 *        the TimerManager.
 */
class TimerManagerData {
public:
    TimerManagerData() noexcept = default;

    ~TimerManagerData() = default;

    bool Initialize() {
        try {
            std::lock_guard<std::recursive_mutex> guard(m_mutex);
            if (!m_initialized) {
                m_resources = std::make_unique<TimerResources>(m_mutex);
                m_initialized = true;
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool startTimer(const Label &label, const timespec &time, const TimerType_e type) {
        std::lock_guard<std::recursive_mutex> guard(m_mutex);
        if (m_resources->m_timers.find(label) == m_resources->m_timers.end()) {
            m_resources->m_timers.emplace(std::piecewise_construct, std::forward_as_tuple(label),
                std::forward_as_tuple(m_resources->m_mailbox, *this, label, time, type));
            return true;
        }
        return false;
    }

    bool cancelTimer(const Label &label) {
        std::lock_guard<std::recursive_mutex> guard(m_mutex);
        auto timer = m_resources->m_timers.find(label);
        if (timer != m_resources->m_timers.end()) {
            timer->second.cancel();
            m_resources->m_timers.erase(label);
            return true;
        }
        return false;
    }

private:
    /**
     * @brief Mutex protecting Timer resources
     */
    std::recursive_mutex m_mutex;

    /**
     * @brief Flag indicating that TimerManagerData has initialized and allocated
     *        shared resources
     */
    bool m_initialized = false;

    /**
     * @brief Timer resources shared
     */
    std::unique_ptr<TimerResources> m_resources;
};

/**
 * @brief Handle a timer firing by sending the appropriate mailbox signal and
 *        cancelling the timer from recurring if it is a ONE_SHOT
 */
void Timer::timerEvent() {
    m_mailbox.SendSignal(m_label);
    if (m_type == ONE_SHOT) {
        m_timerManagerData.cancelTimer(m_label);
    }
}

}  // namespace detail
}  // namespace msglib