
#include "msglib/Mailbox.h"
#include "msglib/TimerManager.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>


const msglib::Label RECURRING_TIMER = 1;
const msglib::Label ONE_SHOT_TIMER = 2;
const msglib::Label DONE = 3;
const msglib::Label EXIT_THREAD = 4;

void thread1(int inst)
{
    msglib::Mailbox mbox;
    spdlog::info("Thread {} registering for RECURRING_TIMER", inst);
    mbox.RegisterForLabel(RECURRING_TIMER);
    mbox.RegisterForLabel(EXIT_THREAD);
    unsigned count = 0;

    while (true)
    {
        msglib::Message msg;

        mbox.Receive(msg);
        if (msg.m_label == RECURRING_TIMER) {
            ++count;
            spdlog::info("Thread {} received RECURRING_TIMER event {}", inst, count);


            mbox.ReleaseMessage(msg);
        } else if (msg.m_label == EXIT_THREAD) {
            spdlog::info("Thread {} received EXIT_THREAD", inst);
            mbox.ReleaseMessage(msg);
            break;
        }
    }
    mbox.UnregisterForLabel(RECURRING_TIMER);
    mbox.UnregisterForLabel(EXIT_THREAD);
}

void thread2(int inst)
{
    msglib::Mailbox mbox;
    spdlog::info("Thread {} registering for ONE_SHOT", inst);
    mbox.RegisterForLabel(ONE_SHOT_TIMER);
    for (msglib::Label i = 5; i <= 45; i++) {
        mbox.RegisterForLabel(i);
    }
    mbox.RegisterForLabel(EXIT_THREAD);
    unsigned count = 0;

    while (true) 
    {
        msglib::Message msg;

        mbox.Receive(msg);
        if (msg.m_label == ONE_SHOT_TIMER) {
            count++;
            spdlog::info("Thread {} received ONE_SHOT event", inst, count);
            mbox.ReleaseMessage(msg);
        }
        else if (msg.m_label >= 5 && msg.m_label <= 45) {
            spdlog::info("Thread {} received label {}", inst, msg.m_label);
            mbox.ReleaseMessage(msg);

            if (msg.m_label == 45) {
                mbox.SendSignal(DONE);
            }
        }

        else if (msg.m_label == EXIT_THREAD) {
            spdlog::info("Thread {} received EXIT_THREAD", inst);
            mbox.ReleaseMessage(msg);
            break;
        }
    }
    mbox.UnregisterForLabel(ONE_SHOT_TIMER);
    for (msglib::Label i = 5; i < 45; i++) {
        mbox.RegisterForLabel(i);
    }
    mbox.UnregisterForLabel(EXIT_THREAD);

}

int main(int /* argc */, char ** /* argv */)
{
    using namespace std::chrono_literals;

    spdlog::info("Initializing Mailbox subsystem");
    msglib::Mailbox mbox;
    mbox.Initialize();

    const time_t PERIOD = 750;  // msec
    const ulong MSEC_TO_NS = 1000000;
    spdlog::info("Initializing Timer subsystem");
    msglib::TimerManager::Initialize();

    std::thread t1(thread1,1);
    std::thread t2(thread2,2);

    // Start a recurring timer using a POSIX timespec
    timespec ts { 0, PERIOD * MSEC_TO_NS };
    msglib::TimerManager::StartTimer(RECURRING_TIMER, ts, msglib::PERIODIC);

    for (uint16_t x = 5; x < 46; x++) {
        spdlog::info("TimerStart({}) returns {}", x, msglib::TimerManager::StartTimer(x, 100ms, msglib::ONE_SHOT));
    }

    // Start a one-shot timer using a std::chrono::duration value in msec (literal form)
    msglib::TimerManager::StartTimer(ONE_SHOT_TIMER, 900ms, msglib::ONE_SHOT);

    mbox.RegisterForLabel(DONE);
    while (true) {
        msglib::Message msg;
        mbox.Receive(msg);
        msglib::MessageGuard guard(mbox,msg);
        if (msg.m_label == DONE) {
            spdlog::info("Cancelling RECURRING_TIMER");
            msglib::TimerManager::CancelTimer(RECURRING_TIMER);
            break;
        }
    }
    mbox.SendSignal(EXIT_THREAD);

    spdlog::info("Joining threads");
    t1.join();
    t2.join();
    spdlog::info("Done joining threads, exiting main()");
}