#include "gtest/gtest.h"

#include "msglib/TimeConv.h"
#include <chrono>

TEST(TimeConvTest, conversions) {
    auto ts1 = msglib::Chrono2Timespec(std::chrono::seconds(5)); // NOLINT
    EXPECT_EQ(5, ts1.tv_sec);
    EXPECT_EQ(0, ts1.tv_nsec);

    auto ts2 = msglib::Chrono2Timespec(std::chrono::seconds(65)); // NOLINT
    EXPECT_EQ(65, ts2.tv_sec);
    EXPECT_EQ(0, ts2.tv_nsec);

    auto ts3 = msglib::Chrono2Timespec(std::chrono::milliseconds(500)); // NOLINT
    EXPECT_EQ(0, ts3.tv_sec);
    EXPECT_EQ(500000000, ts3.tv_nsec);

    auto ts4 = msglib::Chrono2Timespec(std::chrono::milliseconds(1500)); // NOLINT
    EXPECT_EQ(1, ts4.tv_sec);
    EXPECT_EQ(500000000, ts4.tv_nsec);

    auto ts5 = msglib::Chrono2Timespec(std::chrono::microseconds(500)); // NOLINT
    EXPECT_EQ(0, ts5.tv_sec);
    EXPECT_EQ(500000, ts5.tv_nsec);

    auto ts6 = msglib::Chrono2Timespec(std::chrono::microseconds(90000000)); // NOLINT
    EXPECT_EQ(90, ts6.tv_sec);
    EXPECT_EQ(0, ts6.tv_nsec);

    auto ts7 = msglib::Chrono2Timespec(std::chrono::nanoseconds(500)); // NOLINT
    EXPECT_EQ(0, ts7.tv_sec);
    EXPECT_EQ(500, ts7.tv_nsec);
}