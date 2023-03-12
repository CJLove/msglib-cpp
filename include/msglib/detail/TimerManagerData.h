#pragma once

#include "msglib/Mailbox.h"
#include "msglib/TimerManager.h"
#include <array>
#include <atomic>
#include <csignal>
#include <ctime>
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

    /**
     * @brief Cancel a timer
     */
    void cancel() {
        itimerspec itsnew {};
        itsnew.it_value.tv_sec = itsnew.it_value.tv_nsec = 0;
        itsnew.it_interval.tv_sec = itsnew.it_interval.tv_nsec = 0;
        timer_settime(m_timer, 0, &itsnew, &m_spec);
    }

    /**
     * @brief Handle a timer event firing.
     *
     * NOTE: Implementation separate to break circular dependency with TimerManagerData
     */
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

    TimerResources(std::recursive_mutex &mutex)
        : m_bytes(std::make_unique<std::byte[]>(65536 * sizeof(Timer)))
        , m_byteResource(m_bytes.get(), 65536 * sizeof(Timer), std::pmr::null_memory_resource())
        , m_timerResource(&m_byteResource)
        , m_mutex(mutex)
        , m_thread(std::thread(&TimerResources::HandleSignals, this)) {
    }

    ~TimerResources() {
        m_shutdown = true;
        m_thread.join();
    }

    /**
     * @brief Byte array supporting the byteResource
     */
    std::unique_ptr<std::byte[]> m_bytes;

    /**
     * @brief Monotonic buffer resource using byte array
     */
    std::pmr::monotonic_buffer_resource m_byteResource;

    std::pmr::polymorphic_allocator<Timer> m_timerResource;

    /**
     * @brief Mailbox to use for timer signals
     */
    Mailbox m_mailbox;

    /**
     * @brief Collection of current outstanding timers
     */
    std::array<Timer *, 65536> m_timers;

    /**
     * @brief Flag indicating that shutdown has been triggered
     */
    std::atomic<bool> m_shutdown = false;

    /**
     * @brief Mutex protecting timer resources
     */
    std::recursive_mutex &m_mutex;

    /**
     * @brief Thread handling SIGRTMIN signals
     */
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
        if (m_resources->m_timers[label] == nullptr) {
            m_resources->m_timers[label] = m_resources->m_timerResource.allocate(1);
            m_resources->m_timerResource.construct<Timer>(
                m_resources->m_timers[label], m_resources->m_mailbox, *this, label, time, type);
            return true;
        }
        return false;
    }

    bool cancelTimer(const Label &label) {
        std::lock_guard<std::recursive_mutex> guard(m_mutex);
        if (m_resources->m_timers[label] != nullptr) {
            m_resources->m_timers[label]->cancel();
            m_resources->m_timerResource.destroy<Timer>(m_resources->m_timers[label]);
            m_resources->m_timerResource.deallocate(m_resources->m_timers[label], 1);
            m_resources->m_timers[label] = nullptr;
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