#include <csignal>
#include <ctime>
#include <iostream>
#include <mutex>
#include <unordered_map>

#include "TimerManager.h"
#include "Mailbox.h"

class Timer {
public:
    Timer(Mailbox &mailbox, Label label, timespec time, TimerManager::TimerType_e type):
        m_mailbox(mailbox), m_label(label)
    {   
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

        switch(type) {
        case TimerManager::PERIODIC:
            m_spec.it_value = time;
            m_spec.it_interval = time;
            break;
        case TimerManager::ONE_SHOT:
            m_spec.it_value = time;
            m_spec.it_interval.tv_sec = m_spec.it_interval.tv_nsec = 0;
            break;
        }
        if (timer_settime(m_timer,0, &m_spec, nullptr) == -1) {
            throw std::runtime_error("Couldn't start timer");
        }

        std::cout << "Timer instance @ " << this << "\n";
    }

    void cancel() {
        itimerspec itsnew { };
        itsnew.it_value.tv_sec = itsnew.it_value.tv_nsec = 0;
        itsnew.it_interval.tv_sec = itsnew.it_interval.tv_nsec = 0;
        timer_settime(m_timer,0,&itsnew, &m_spec);

        std::cout << "Timer instance @ " << this << " canceled\n";
    }

    void timerEvent() {
        m_mailbox.SendSignal(m_label);
    }

private:
    Mailbox &m_mailbox;
    Label m_label = 0;
    timer_t m_timer = nullptr;
    struct sigevent m_sev {};
    struct sigaction m_sa {};
    struct itimerspec m_spec {};

    static void handler(int, siginfo_t *si, void *uc) {
        (reinterpret_cast<Timer*>(si->si_value.sival_ptr))->timerEvent();
    }
};


struct TimerManagerImpl {
    using TimerMap = std::unordered_map<Label,Timer>;

    TimerManagerImpl() = default;

    void startTimer(const Label &label, const timespec &time, const TimerManager::TimerType_e type)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_timers.find(label) == m_timers.end()) {
            m_timers.emplace(std::piecewise_construct,
                             std::forward_as_tuple(label),
                             std::forward_as_tuple(m_mailbox, label, time, type));
        }

    }

    void cancelTimer(const Label &label)
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        auto timer = m_timers.find(label);
        if (timer != m_timers.end()) {
            timer->second.cancel();
            m_timers.erase(label);
        }
    }

    Mailbox m_mailbox;
    TimerMap m_timers;
    std::mutex m_mutex;
};

/**
 * @brief Static member declaration
 * 
 */
std::unique_ptr<TimerManagerImpl> TimerManager::s_timerData = std::make_unique<TimerManagerImpl>();

void TimerManager::startTimer(const Label &label, const timespec &time, const TimerType_e type)
{
    s_timerData->startTimer(label,time,type);

}

void TimerManager::cancelTimer(const Label &label)
{
    s_timerData->cancelTimer(label);
}