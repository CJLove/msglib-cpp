#include "gtest/gtest.h"

#include "msglib/Pool.h"
#include <thread>

struct TestStruct {
    TestStruct() = default;

    TestStruct(int a, int b, int c): m_a(a),m_b(b),m_c(c)
    {}

    int m_a = 0;
    int m_b = 0;
    int m_c = 0;
};

TEST(PoolTest, allocFree)
{
    msglib::Pool<TestStruct> pool(3);
    EXPECT_EQ(3,pool.capacity());
    EXPECT_EQ(3,pool.size());

    auto t1 = pool.alloc(1,2,3);
    EXPECT_TRUE(t1 != nullptr);
    EXPECT_EQ(2,pool.size());

    auto t2 = pool.alloc(4,5,6);
    EXPECT_TRUE(t2 != nullptr);
    EXPECT_EQ(1,pool.size());

    auto t3 = pool.alloc(7,8,9);
    EXPECT_TRUE(t3 != nullptr);
    EXPECT_EQ(0,pool.size());

    try {
        auto t4 = pool.alloc();
        FAIL() << "expected std::bad_alloc";
        pool.free(t4);  // NOT REACHED
    } 
    catch (std::bad_alloc &e)
    {
        // Expect std::bad_alloc to be thrown
    }
    catch (std::exception &e) {
        FAIL() << "unexpected exception " << e.what();
    }

    pool.free(t1);
    EXPECT_EQ(1,pool.size());

    // Free a nullptr
    pool.free(nullptr);
    EXPECT_EQ(1,pool.size());

    // Try freeing an item not part of the pool
    auto t5 = new TestStruct(10,11,12);
    pool.free(t5);

    delete t5;
    EXPECT_EQ(1,pool.size());
}

void testPoolThread(msglib::Pool<TestStruct> *pool)
{
    TestStruct *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = pool->alloc(i,i+1,i+2);
    }

    for (int i = 99; i >= 0; i--) {
        pool->free(ptrs[i]);
    }
}

TEST(PoolTest,threads)
{
    msglib::Pool<TestStruct> pool(300);

    try {
        std::cout << "Starting threads\n";
        std::thread t1(testPoolThread,&pool);
        std::thread t2(testPoolThread,&pool);
        std::thread t3(testPoolThread,&pool);

        std::cout << "Joining threads\n";
        t1.join();
        t2.join();
        t3.join();
    }
    catch (std::exception &e) {
        FAIL() << "Unexpected exception " << e.what();
    }
}

