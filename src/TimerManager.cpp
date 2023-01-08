#include "msglib/TimerManager.h"
#include "msglib/Mailbox.h"
#include <atomic>
#include <csignal>
#include <mutex>
#include <thread>

namespace msglib {

namespace detail {

/**
 * @brief Timer is a representation of a timer which has been scheduled within the TimerManager
 *
 */
class Timer {
public:
    Timer(Mailbox &mailbox, Label label, timespec time, TimerType_e type);

    ~Timer();

    void cancel() {
        itimerspec itsnew {};
        itsnew.it_value.tv_sec = itsnew.it_value.tv_nsec = 0;
        itsnew.it_interval.tv_sec = itsnew.it_interval.tv_nsec = 0;
        timer_settime(m_timer, 0, &itsnew, &m_spec);
    }

    void timerEvent() {
        m_mailbox.SendSignal(m_label);
        if (m_type == ONE_SHOT) {
            TimerManager::CancelTimer(m_label);
        }
    }

private:
    Mailbox &m_mailbox;
    Label m_label = 0;
    timer_t m_timer = nullptr;
    TimerType_e m_type = ONE_SHOT;
    struct sigevent m_sev { };
    struct itimerspec m_spec { };

};

Timer::Timer(Mailbox &mailbox, Label label, timespec time, TimerType_e type)
    : m_mailbox(mailbox), m_label(label), m_type(type) {
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

Timer::~Timer()
{
    timer_delete(m_timer);
}


struct TimerManagerData::TimerManagerDataImpl {

    TimerManagerDataImpl() = default;

    ~TimerManagerDataImpl();

    using TimerMap = std::unordered_map<Label, Timer>;

    void Initialize();

    void HandleSignals();

    Mailbox m_mailbox;
    TimerMap m_timers;
    std::recursive_mutex m_mutex;
    std::thread m_thread;
    std::atomic<bool> m_shutdown;
};


TimerManagerData::TimerManagerDataImpl::~TimerManagerDataImpl()
{
    m_shutdown = true;
    m_thread.join();
}

void TimerManagerData::TimerManagerDataImpl::Initialize()
{
    m_thread = std::thread(&TimerManagerDataImpl::HandleSignals,this);
}

void TimerManagerData::TimerManagerDataImpl::HandleSignals()
{
    constexpr int64_t NSEC_DELAY = 500000000;
    struct timespec ts { 0, NSEC_DELAY };
    sigset_t sigset;
    if (sigemptyset(&sigset) != 0) {
        throw std::runtime_error("sigemptyset error");
    }
    if (sigaddset(&sigset,SIGRTMIN) != 0) {
        throw std::runtime_error("sigaddset error");
    }
    if (pthread_sigmask(SIG_BLOCK, &sigset, nullptr) != 0) {
        throw std::runtime_error("pthread_sigmask error");
    }

    while (!m_shutdown) {
        siginfo_t info = { };
        int result = sigtimedwait(&sigset, &info, &ts );
        if (result == SIGRTMIN) {
            std::lock_guard<std::recursive_mutex> guard(m_mutex);

            auto *timer = reinterpret_cast<Timer *>(info.si_value.sival_ptr);
            timer->timerEvent();
        }
    }
}

TimerManagerData::TimerManagerData() : m_pImpl(std::make_unique<TimerManagerData::TimerManagerDataImpl>()) {
}

void TimerManagerData::initialize()
{
    m_pImpl->Initialize();
}

bool TimerManagerData::startTimer(const Label &label, const timespec &time, const TimerType_e type) {
    std::lock_guard<std::recursive_mutex> guard(m_pImpl->m_mutex);
    if (m_pImpl->m_timers.find(label) == m_pImpl->m_timers.end()) {
        m_pImpl->m_timers.emplace(std::piecewise_construct, std::forward_as_tuple(label),
            std::forward_as_tuple(m_pImpl->m_mailbox, label, time, type));
        return true;
    }
    return false;
}

bool TimerManagerData::cancelTimer(const Label &label) {
    std::lock_guard<std::recursive_mutex> guard(m_pImpl->m_mutex);
    auto timer = m_pImpl->m_timers.find(label);
    if (timer != m_pImpl->m_timers.end()) {
        timer->second.cancel();
        m_pImpl->m_timers.erase(label);
        return true;
    }
    return false;
}

} // Namespace detail

/**
 * @brief Static member declaration
 *
 */
std::unique_ptr<detail::TimerManagerData> TimerManager::s_timerData;


bool TimerManager::Initialize() {
    s_timerData = std::make_unique<detail::TimerManagerData>();
    s_timerData->initialize();
    sigset_t sigset;
    if (sigemptyset(&sigset) != 0) {
        return false;
        //throw std::runtime_error("sigfillset error");
    }
    if (sigaddset(&sigset,SIGRTMIN) != 0) {
        return false;
        //throw std::runtime_error("sigaddset error");
    }
    if (sigprocmask(SIG_BLOCK,&sigset,nullptr) != 0) {  // NOLINT
        return false;
        //throw std::runtime_error("sigprocmask error");
    }
    if (pthread_sigmask(SIG_BLOCK, &sigset, nullptr) != 0) {
        return false;
        //throw std::runtime_error("pthread_sigmask error");
    }
    return true;
}

bool TimerManager::StartTimer(const Label &label, const timespec &time, const TimerType_e type) {
    if (s_timerData) {
        return s_timerData->startTimer(label, time, type);
    } else {
        return false;
        //throw std::runtime_error("TimerManager not initialized");
    }
}

bool TimerManager::StartTimer(
    const Label &label, const std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> &time) {
    if (s_timerData) {
        auto secs = std::chrono::time_point_cast<std::chrono::seconds>(time);
        auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(time) -
            std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

        return s_timerData->startTimer(label, timespec {secs.time_since_epoch().count(), ns.count()}, ONE_SHOT);
    } else {
        return false;
        //throw std::runtime_error("TimerManager not initialized");
    }
}

bool TimerManager::CancelTimer(const Label &label) {
    if (s_timerData) {
        return s_timerData->cancelTimer(label);
    } else {
        return false;
        //throw std::runtime_error("TimerManager not initialized");
    }
}

}  // Namespace msglib