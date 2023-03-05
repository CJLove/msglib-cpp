#include "gtest/gtest.h"
#include "msglib/detail/DiagResource.h"
#include "msglib/detail/Queue.h"
#include <memory_resource>
#include <mutex>
#include <thread>

using msglib::detail::Queue;
using namespace std::chrono_literals;

struct TestStruct {
    TestStruct() = default;

    TestStruct(int a, int b, int c) : m_a(a), m_b(b), m_c(c) {
    }

    int m_a = 0;
    int m_b = 0;
    int m_c = 0;
};



class QueueTest : public ::testing::Test {
protected:
    QueueTest():
        m_defaultResource(std::pmr::get_default_resource()),
        m_rogueResource(),
        m_oomResource(),
        m_storage(std::make_unique<std::byte[]>(16384)),
        m_bufferResource(m_storage.get(),16384, &m_oomResource),
        m_syncResource(&m_bufferResource)
    {}

    virtual ~QueueTest() noexcept
    {}


    void SetUp() override {
        // Install the rogue resource as the default to track any stray
        // allocations
        std::pmr::set_default_resource(&m_rogueResource);

    }

    void TearDown() override {
        // Restore the default resource
        std::pmr::set_default_resource(m_defaultResource);
    }

protected:
    std::pmr::memory_resource *m_defaultResource;

    msglib::detail::TestResource m_rogueResource;
    msglib::detail::TestResource m_oomResource;
    std::unique_ptr<std::byte[]> m_storage;
    std::pmr::monotonic_buffer_resource m_bufferResource;
    std::pmr::synchronized_pool_resource m_syncResource;

};

TEST_F(QueueTest, pushTests) {
    Queue<TestStruct> queue(2,&m_syncResource);

    EXPECT_EQ(0, queue.size());
    EXPECT_TRUE(queue.empty());

    EXPECT_TRUE(queue.push(TestStruct(1, 2, 3)));
    EXPECT_EQ(1, queue.size());
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.emplace(4, 5, 6));
    EXPECT_EQ(2, queue.size());
    EXPECT_FALSE(queue.empty());

    EXPECT_FALSE(queue.emplace(7, 8, 9));
}

TEST_F(QueueTest, tryPopTests) {
    Queue<TestStruct> queue(2, &m_syncResource);

    TestStruct msg;
    EXPECT_FALSE(queue.tryPop(msg));

    queue.emplace(1, 2, 3);

    queue.pop(msg);
    EXPECT_EQ(1, msg.m_a);
    EXPECT_EQ(2, msg.m_b);
    EXPECT_EQ(3, msg.m_c);

    EXPECT_FALSE(queue.tryPop(msg));
}

void producer(Queue<TestStruct> &queue) {
    std::this_thread::sleep_for(500ms);
    queue.emplace(1, 2, 3);

    queue.emplace(4, 5, 6);  // NOLINT
}

TEST_F(QueueTest, popWaitTests) {
    Queue<TestStruct> queue(2,&m_syncResource);
    TestStruct msg;
    EXPECT_EQ(false, queue.popWait(msg, 500ms));

    queue.emplace(1, 2, 3);
    EXPECT_EQ(true, queue.popWait(msg, 500ms));
    EXPECT_EQ(1, msg.m_a);
    EXPECT_EQ(2, msg.m_b);
    EXPECT_EQ(3, msg.m_c);
}

TEST_F(QueueTest, popTests) {
    Queue<TestStruct> queue(2, &m_syncResource);

    // Start producer thread which will push 2 messages after 500ms
    std::thread prodThread(producer, std::ref(queue));

    TestStruct msg;
    try {
        // Test blocking wait on an empty queue
        queue.pop(msg);
        EXPECT_EQ(1, msg.m_a);
        EXPECT_EQ(2, msg.m_b);
        EXPECT_EQ(3, msg.m_c);
    } catch (std::exception &e) {
        FAIL() << "Unexpected exception in pop() " << e.what() << "\n";
    }

    try {
        // Test wait on non-empty queue
        queue.pop(msg);
        EXPECT_EQ(4, msg.m_a);
        EXPECT_EQ(5, msg.m_b);
        EXPECT_EQ(6, msg.m_c);
    } catch (std::exception &e) {
        FAIL() << "Unexpected exception in pop() " << e.what() << "\n";
    }

    prodThread.join();

    EXPECT_FALSE(m_rogueResource.allocatorInvoked());
    EXPECT_FALSE(m_oomResource.allocatorInvoked());
}
