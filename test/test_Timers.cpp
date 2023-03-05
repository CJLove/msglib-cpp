#include "gtest/gtest.h"

#include "msglib/TimerManager.h"
#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using msglib::Message;
using msglib::Mailbox;
using msglib::Label;
using msglib::MessageGuard;
using msglib::TimerManager;

constexpr Label OneShotEvent = 999;
constexpr Label PeriodicEvent = 998;

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
    std::cout << "Received " << msg.m_label << "\n";

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

class TimeManagerTest: public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize mailbox subsystem
        Mailbox mbox;
        mbox.Initialize();

        // Initialize timer subsystem
        TimerManager::Initialize();

    }

};

TEST_F(TimeManagerTest, OneShotPOSIX) {

    timespec ts {0, 500000000}; // NOLINT
    EventTester tester;

    std::thread evt(EventTestThread, std::ref(tester));

    TimerManager::StartTimer(OneShotEvent, ts, msglib::ONE_SHOT);

    std::this_thread::sleep_for(1s);

    evt.join();
    EXPECT_TRUE(tester.received);
}

TEST_F(TimeManagerTest, OneShotChrono) {
    EventTester tester;

    std::thread evt(EventTestThread, std::ref(tester));

    TimerManager::StartTimer(OneShotEvent, 500ms, msglib::ONE_SHOT);

    std::this_thread::sleep_for(1s);

    evt.join();
    EXPECT_TRUE(tester.received);
}

#if 0
TEST_F(TimeManagerTest, OneShotTimePoint) {
    EventTester tester;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::time_point_cast<std::chrono::nanoseconds> (now + 1s);

    std::thread evt(EventTestThread, std::ref(tester));

    TimerManager::StartTimer(OneShotEvent, time, Timer::ONE_SHOT);

    std::this_thread::sleep_for(2s);

    evt.join();
    EXPECT_TRUE(tester.received);
}
#endif

TEST_F(TimeManagerTest, RecurringPOSIX) {
    const time_t PERIOD = 500L;
    const uint64_t MSEC2NSEC = 1000000UL;
    timespec ts {0, PERIOD * MSEC2NSEC};

    EventTester tester;

    std::thread evt(RecurringEventTestThread, std::ref(tester));

    TimerManager::StartTimer(PeriodicEvent, ts, msglib::PERIODIC);

    std::this_thread::sleep_for(2s);
    TimerManager::CancelTimer(PeriodicEvent);

    std::cout << "Joining\n";
    evt.join();
    EXPECT_EQ(3, tester.count);
}
