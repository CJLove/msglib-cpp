#include "gtest/gtest.h"

#include "msglib/Pool.h"
#include <array>
#include <thread>

struct TestStruct {
    TestStruct() = default;

    TestStruct(int a, int b, int c) : m_a(a), m_b(b), m_c(c) {
    }

    explicit TestStruct(bool except) {
        if (except) {
            throw std::runtime_error("expected exception");
        }
    }

    int m_a = 0;
    int m_b = 0;
    int m_c = 0;
};

TEST(PoolTest, allocFail) {
    msglib::Pool<TestStruct> pool(3);
    EXPECT_EQ(3,pool.size());
    EXPECT_EQ(3,pool.capacity());

    try {
        // Throw as part of allocating
        auto *t1 = pool.alloc(true); // NOLINT
        FAIL() << "expected std::runtime_error()";
        if (t1 != nullptr) {
            pool.free(t1);
        }
    }
    catch (std::exception &e) {

        EXPECT_EQ(3,pool.size());
    }
}

TEST(PoolTest, allocFree) {
    msglib::Pool<TestStruct> pool(3);
    EXPECT_EQ(3, pool.capacity());
    EXPECT_EQ(3, pool.size());

    auto *t1 = pool.alloc(1, 2, 3);
    EXPECT_TRUE(t1 != nullptr);
    EXPECT_EQ(2, pool.size());

    auto *t2 = pool.alloc(4, 5, 6); // NOLINT
    EXPECT_TRUE(t2 != nullptr);
    EXPECT_EQ(1, pool.size());

    auto *t3 = pool.alloc(7, 8, 9); // NOLINT
    EXPECT_TRUE(t3 != nullptr);
    EXPECT_EQ(0, pool.size());

    try {
        auto *t4 = pool.alloc(); // NOLINT
        FAIL() << "expected std::bad_alloc";
        if (t4 != nullptr) {
            pool.free(t4);  // NOT REACHED
        }
    } catch (std::bad_alloc &e) {
        // Expect std::bad_alloc to be thrown
    } catch (std::exception &e) { FAIL() << "unexpected exception " << e.what(); }

    std::cout << "pool.size() = " << pool.size() << "\n";

    pool.free(t1);
    EXPECT_EQ(1, pool.size());

    // Free a nullptr
    pool.free(nullptr);
    EXPECT_EQ(1, pool.size());

}

void testPoolThread(msglib::Pool<TestStruct> *pool) {
    constexpr int SIZE=100;
    std::array<TestStruct *,SIZE> ptrs;  
    for (int i = 0; i < SIZE; i++) {
        ptrs[i] = pool->alloc(i, i + 1, i + 2);
    }

    for (int i = SIZE-1; i >= 0; i--) {
        pool->free(ptrs[i]);
    }
}

TEST(PoolTest, threads) {
    constexpr int SIZE=300;
    msglib::Pool<TestStruct> pool(SIZE);

    try {
        std::cout << "Starting threads\n";
        std::thread t1(testPoolThread, &pool);
        std::thread t2(testPoolThread, &pool);
        std::thread t3(testPoolThread, &pool);

        std::cout << "Joining threads\n";
        t1.join();
        t2.join();
        t3.join();
    } catch (std::exception &e) { FAIL() << "Unexpected exception " << e.what(); }
}
