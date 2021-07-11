#include "msglib/TimerManager.h"
#include "msglib/Mailbox.h"
#include <thread>
#include <mutex>
#include <atomic>

namespace msglib {

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

    TimerManagerDataImpl();

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

TimerManagerData::TimerManagerDataImpl::TimerManagerDataImpl()
{
}

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

//    std::cout << "Starting signal handling thread\n";
    while (!m_shutdown) {
        siginfo_t info = { };
        int result = sigtimedwait(&sigset, &info, &ts );
        if (result == SIGRTMIN) {
            std::lock_guard<std::recursive_mutex> guard(m_mutex);

            Timer *timer = reinterpret_cast<Timer *>(info.si_value.sival_ptr);
            timer->timerEvent();
        }
    }
//    std::cout << "Exiting signal handling thread\n";
}

TimerManagerData::TimerManagerData() : m_pImpl(std::make_unique<TimerManagerData::TimerManagerDataImpl>()) {
}

void TimerManagerData::initialize()
{
    m_pImpl->Initialize();
}

void TimerManagerData::startTimer(const Label &label, const timespec &time, const TimerType_e type) {
    std::lock_guard<std::recursive_mutex> guard(m_pImpl->m_mutex);
    if (m_pImpl->m_timers.find(label) == m_pImpl->m_timers.end()) {
        m_pImpl->m_timers.emplace(std::piecewise_construct, std::forward_as_tuple(label),
            std::forward_as_tuple(m_pImpl->m_mailbox, label, time, type));
    }
}

void TimerManagerData::cancelTimer(const Label &label) {
    std::lock_guard<std::recursive_mutex> guard(m_pImpl->m_mutex);
    auto timer = m_pImpl->m_timers.find(label);
    if (timer != m_pImpl->m_timers.end()) {
        timer->second.cancel();
        m_pImpl->m_timers.erase(label);
    }
}

/**
 * @brief Static member declaration
 *
 */
std::unique_ptr<TimerManagerData> TimerManager::s_timerData = std::make_unique<TimerManagerData>();

void TimerManager::Initialize() {
    sigset_t sigset;
    // if (sigfillset(&sigset) != 0) {
    //     throw std::runtime_error("sigfillset error");
    // }
    if (sigemptyset(&sigset) != 0) {
        throw std::runtime_error("sigfillset error");
    }
    if (sigaddset(&sigset,SIGRTMIN) != 0) {
        throw std::runtime_error("sigaddset error");
    }
    if (sigprocmask(SIG_BLOCK,&sigset,nullptr) != 0) {
        throw std::runtime_error("sigprocmask error");
    }
    if (pthread_sigmask(SIG_BLOCK, &sigset, nullptr) != 0) {
        throw std::runtime_error("pthread_sigmask error");
    }
    s_timerData->initialize();
}

void TimerManager::StartTimer(const Label &label, const timespec &time, const TimerType_e type) {
    s_timerData->startTimer(label, time, type);
}

void TimerManager::StartTimer(
    const Label &label, const std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> &time) {
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(time);
    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(time) -
        std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

    s_timerData->startTimer(label, timespec {secs.time_since_epoch().count(), ns.count()}, ONE_SHOT);
}

void TimerManager::CancelTimer(const Label &label) {
    s_timerData->cancelTimer(label);
}

}  // Namespace msglib