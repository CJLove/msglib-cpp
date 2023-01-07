
#include "msglib/Mailbox.h"
#include "msglib/TimerManager.h"
#include <iostream>
#include <thread>


const msglib::Label RECURRING_TIMER = 1;
const msglib::Label ONE_SHOT_TIMER = 2;
const msglib::Label DONE = 3;
const msglib::Label EXIT_THREAD = 4;

void thread1(int inst)
{
    const int MAX_EVENTS = 10;
    msglib::Mailbox mbox;
    std::cout << "Thread " << inst << " registering for RECURRING_TIMER\n";
    mbox.RegisterForLabel(RECURRING_TIMER);
    mbox.RegisterForLabel(EXIT_THREAD);
    unsigned count = 0;

    while (true)
    {
        msglib::Message msg;

        mbox.Receive(msg);
        if (msg.m_label == RECURRING_TIMER) {
            ++count;
            std::cout << "Received RECURRING_TIMER event " << count << "\n";

            if (count == MAX_EVENTS) {
                mbox.SendSignal(DONE);
            }
            mbox.ReleaseMessage(msg);
        } else if (msg.m_label == EXIT_THREAD) {
            std::cout << "Thread " << inst << " received EXIT_THREAD\n";
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
    std::cout << "Thread " << inst << " registering for ONE_SHOT\n";
    mbox.RegisterForLabel(ONE_SHOT_TIMER);
    mbox.RegisterForLabel(EXIT_THREAD);
    unsigned count = 0;

    while (true) 
    {
        msglib::Message msg;

        mbox.Receive(msg);
        if (msg.m_label == ONE_SHOT_TIMER) {
            count++;
            std::cout << "Received ONE_SHOT event " << count << "\n";
            mbox.ReleaseMessage(msg);

        } else if (msg.m_label == EXIT_THREAD) {
            std::cout << "Thread " << inst << " received EXIT_THREAD\n";
            mbox.ReleaseMessage(msg);
            break;
        }
    }
    mbox.UnregisterForLabel(ONE_SHOT_TIMER);
    mbox.UnregisterForLabel(EXIT_THREAD);

}

int main(int /* argc */, char ** /* argv */)
{
    using namespace std::chrono_literals;

    const time_t PERIOD = 750;  // msec
    const ulong MSEC_TO_NS = 1000000;
    std::cout << "Calling Initialize()\n";
    msglib::TimerManager::Initialize();

    std::thread t1(thread1,1);
    std::thread t2(thread2,2);

    // Start a recurring timer using a POSIX timespec
    timespec ts { 0, PERIOD * MSEC_TO_NS };
    msglib::TimerManager::StartTimer(RECURRING_TIMER, ts, msglib::PERIODIC);

    // Start a one-shot timer using a std::chrono::duration value in msec (literal form)
    msglib::TimerManager::StartTimer(ONE_SHOT_TIMER, 900ms, msglib::ONE_SHOT);

    msglib::Mailbox mbox;
    mbox.RegisterForLabel(DONE);
    while (true) {
        msglib::Message msg;
        mbox.Receive(msg);
        msglib::MessageGuard guard(mbox,msg);
        if (msg.m_label == DONE) {
            std::cout << "Cancelling RECURRING_TIMER\n";
            msglib::TimerManager::CancelTimer(RECURRING_TIMER);
            break;
        }
    }
    mbox.SendSignal(EXIT_THREAD);

    std::cout << "Joining threads\n";
    t1.join();
    t2.join();
    std::cout << "Done\n";
}