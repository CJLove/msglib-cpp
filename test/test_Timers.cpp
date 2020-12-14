#include "gtest/gtest.h"

#include "msglib/TimerManager.h"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using namespace msglib;

static Label OneShotEvent = 999;
static Label PeriodicEvent = 998;

struct EventTester {
    int count = 0;
    bool received = false;
};

void EventTestThread(EventTester &tester) {
    Mailbox mbox;
    mbox.RegisterForLabel(OneShotEvent);

    Message msg;
    mbox.Receive(msg);
    MessageGuard guard(mbox, msg);

    tester.received = (msg.m_label == OneShotEvent);
    mbox.UnregisterForLabel(OneShotEvent);
}

void RecurringEventTestThread(EventTester &tester) {
    Mailbox mbox;
    mbox.RegisterForLabel(PeriodicEvent);
    while (true) {
        Message msg;
        mbox.Receive(msg);
        MessageGuard guard(mbox, msg);
        std::cout << "Received recurring event " << msg.m_label << "\n";

        if (msg.m_label == PeriodicEvent) {
            tester.count++;
        }
        if (tester.count == 3) {
            break;
        }
    }
    mbox.UnregisterForLabel(PeriodicEvent);
}

#if 1
TEST(TimerManager, OneShotPOSIX) {
    timespec ts {0, 500000000};
    EventTester tester;

    std::thread evt(EventTestThread, std::ref(tester));

    TimerManager::StartTimer(OneShotEvent, ts, Timer::ONE_SHOT);

    std::this_thread::sleep_for(1s);

    evt.join();
    EXPECT_TRUE(tester.received);
}
#endif

#if 1
TEST(TimerManager, OneShotChrono) {
    EventTester tester;

    std::thread evt(EventTestThread, std::ref(tester));

    TimerManager::StartTimer(PeriodicEvent, 500ms, Timer::ONE_SHOT);

    std::this_thread::sleep_for(1s);

    evt.join();
    EXPECT_TRUE(tester.received);
}
#endif

TEST(TimerManager, RecurringPOSIX) {
    const time_t PERIOD = 500L;
    const unsigned long MSEC2NSEC = 1000000UL;
    timespec ts {0, PERIOD * MSEC2NSEC};

    EventTester tester;

    std::thread evt(RecurringEventTestThread, std::ref(tester));

    TimerManager::StartTimer(PeriodicEvent, ts, Timer::PERIODIC);

    std::this_thread::sleep_for(2s);
    TimerManager::CancelTimer(PeriodicEvent);

    std::cout << "Joining\n";
    evt.join();
    EXPECT_EQ(3, tester.count);
}
