#include "gtest/gtest.h"

#include "msglib/Queue.h"
#include <thread>
#include <mutex>

using msglib::Queue;
using namespace std::chrono_literals;

struct TestStruct {
    TestStruct() = default;

    TestStruct(int a, int b, int c) : m_a(a), m_b(b), m_c(c) {
    }

    int m_a = 0;
    int m_b = 0;
    int m_c = 0;
};

TEST(QueueTest, pushTests) {
    Queue<TestStruct> queue(2);

    EXPECT_EQ(0,queue.size());
    EXPECT_TRUE(queue.empty());

    EXPECT_TRUE(queue.push(TestStruct(1,2,3)));
    EXPECT_EQ(1,queue.size());
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.emplace(4,5,6));
    EXPECT_EQ(2,queue.size());
    EXPECT_FALSE(queue.empty());

    EXPECT_FALSE(queue.emplace(7,8,9));
}

TEST(QueueTest, tryPopTests) {
    Queue<TestStruct> queue(2);

    TestStruct msg;
    EXPECT_FALSE(queue.tryPop(msg));

    queue.emplace(1,2,3);

    queue.pop(msg);
    EXPECT_EQ(1,msg.m_a);
    EXPECT_EQ(2,msg.m_b);
    EXPECT_EQ(3,msg.m_c);

    EXPECT_FALSE(queue.tryPop(msg));

}

void producer(Queue<TestStruct> &queue)
{
    std::this_thread::sleep_for(500ms);
    queue.emplace(1,2,3);

    queue.emplace(4,5,6); // NOLINT

}

TEST(QueueTest, popWaitTests) {
    Queue<TestStruct> queue(2);
    TestStruct msg;
    EXPECT_EQ(false,queue.popWait(msg,500ms));

    queue.emplace(1,2,3);
    EXPECT_EQ(true,queue.popWait(msg,500ms));
    EXPECT_EQ(1,msg.m_a);
    EXPECT_EQ(2,msg.m_b);
    EXPECT_EQ(3,msg.m_c);

}

TEST(QueueTest, popTests) {
    Queue<TestStruct> queue(2);

    // Start producer thread which will push 2 messages after 500ms
    std::thread prodThread(producer,std::ref(queue));

    TestStruct msg;
    try {
        // Test blocking wait on an empty queue
        queue.pop(msg);
        EXPECT_EQ(1,msg.m_a);
        EXPECT_EQ(2,msg.m_b);
        EXPECT_EQ(3,msg.m_c);
    }
    catch (std::exception &e)
    {
        FAIL() << "Unexpected exception in pop() " << e.what() << "\n";
    }

    try {
        // Test wait on non-empty queue
        queue.pop(msg);
        EXPECT_EQ(4,msg.m_a);
        EXPECT_EQ(5,msg.m_b);
        EXPECT_EQ(6,msg.m_c);
    }
    catch (std::exception &e)
    {
        FAIL() << "Unexpected exception in pop() " << e.what() << "\n";
    }

    prodThread.join();
}
