#include "msglib/TimerManager.h"
#include "msglib/Mailbox.h"

namespace msglib {

Timer::Timer(Mailbox &mailbox, Label label, timespec time, TimerType_e type) : m_mailbox(mailbox), m_label(label) {
    m_sa.sa_flags = SA_SIGINFO;
    m_sa.sa_sigaction = handler;
    sigemptyset(&m_sa.sa_mask);
    if (sigaction(SIGRTMIN, &m_sa, nullptr) == -1) {
        throw std::runtime_error("Couldn't create signal handler");
    }
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

void Timer::cancel() {
    itimerspec itsnew {};
    itsnew.it_value.tv_sec = itsnew.it_value.tv_nsec = 0;
    itsnew.it_interval.tv_sec = itsnew.it_interval.tv_nsec = 0;
    timer_settime(m_timer, 0, &itsnew, &m_spec);
}

void Timer::timerEvent() {
    m_mailbox.SendSignal(m_label);
}

void Timer::handler(int, siginfo_t *si, void * /* uc */) {
    (reinterpret_cast<Timer *>(si->si_value.sival_ptr))->timerEvent();
}

void TimerManagerData::startTimer(const Label &label, const timespec &time, const Timer::TimerType_e type) {
    std::lock_guard<std::mutex> guard(m_mutex);
    if (m_timers.find(label) == m_timers.end()) {
        m_timers.emplace(
            std::piecewise_construct, std::forward_as_tuple(label), std::forward_as_tuple(m_mailbox, label, time, type));
    }
}

void TimerManagerData::cancelTimer(const Label &label) {
    std::lock_guard<std::mutex> guard(m_mutex);
    auto timer = m_timers.find(label);
    if (timer != m_timers.end()) {
        timer->second.cancel();
        m_timers.erase(label);
    }
}

/**
 * @brief Static member declaration
 *
 */
std::unique_ptr<TimerManagerData> TimerManager::s_timerData = std::make_unique<TimerManagerData>();

void TimerManager::StartTimer(const Label &label, const timespec &time, const Timer::TimerType_e type) {
    s_timerData->startTimer(label, time, type);
}

void TimerManager::StartTimer(
    const Label &label, const std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> &time) {
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(time);
    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(time) -
        std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

    s_timerData->startTimer(label, timespec {secs.time_since_epoch().count(), ns.count()}, Timer::ONE_SHOT);
}

void TimerManager::CancelTimer(const Label &label) {
    s_timerData->cancelTimer(label);
}

}  // Namespace msglib