#include "msglib/detail/BytePool.h"
#include "msglib/detail/DiagResource.h"
#include "gtest/gtest.h"
#include <array>
#include <memory_resource>
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

struct TestStruct2 {
    TestStruct2() = default;

    TestStruct2(int a ) : m_a(a) {
    }

    explicit TestStruct2(bool except) {
        if (except) {
            throw std::runtime_error("expected exception");
        }
    }

    int m_a = 0;
};

class BytePoolTest : public ::testing::Test {
protected:
    BytePoolTest()
        : m_defaultResource(std::pmr::get_default_resource())
        , m_rogueResource()
        , m_oomResource()
        , m_storage(std::make_unique<std::byte[]>(16384))
        , m_bufferResource(m_storage.get(), 16384, &m_oomResource)
        , m_syncResource(&m_bufferResource) {
    }

    virtual ~BytePoolTest() noexcept {
    }

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

TEST_F(BytePoolTest, allocFail) {
    msglib::detail::BytePool pool(sizeof(TestStruct),0, &m_syncResource);
    EXPECT_EQ(0, pool.capacity());
    EXPECT_EQ(0, pool.size());
    EXPECT_EQ(sizeof(TestStruct),pool.eltSize());

    auto db = pool.alloc();
    EXPECT_EQ(db.get(),nullptr);
    EXPECT_EQ(db.size(),0);
}

TEST_F(BytePoolTest, allocSuccess) {
    msglib::detail::BytePool pool(sizeof(TestStruct),1, &m_syncResource);

    EXPECT_EQ(1, pool.capacity());
    EXPECT_EQ(1, pool.size());
    EXPECT_EQ(sizeof(TestStruct),pool.eltSize());

    auto db = pool.alloc();
    EXPECT_TRUE(db.get() != nullptr);
    EXPECT_EQ(db.size(),sizeof(TestStruct));
    EXPECT_EQ(0, pool.size());

    pool.free(db.get());
    EXPECT_EQ(1, pool.size());

}

TEST_F(BytePoolTest, DataBlockPut)
{
    msglib::detail::BytePool pool(sizeof(TestStruct2),1, &m_syncResource);
    EXPECT_EQ(1, pool.capacity());
    EXPECT_EQ(1, pool.size());
    EXPECT_EQ(sizeof(TestStruct2),pool.eltSize());

    auto db = pool.alloc();
    EXPECT_TRUE(db.get() != nullptr);
    EXPECT_EQ(db.size(),sizeof(TestStruct2));

    TestStruct2 t2(5);
    EXPECT_TRUE(db.put<TestStruct2>(t2));

    TestStruct t(1,2,3);
    EXPECT_FALSE(db.put<TestStruct>(t));

    pool.free(db.get());
    EXPECT_EQ(1, pool.size());

}


#if 0
TEST(BytePoolTest, allocFree) {
    msglib::BytePool<TestStruct> BytePool(3);
    EXPECT_EQ(3, BytePool.capacity());
    EXPECT_EQ(3, BytePool.size());

    auto *t1 = BytePool.alloc(1, 2, 3);
    EXPECT_TRUE(t1 != nullptr);
    EXPECT_EQ(2, BytePool.size());

    auto *t2 = BytePool.alloc(4, 5, 6);  // NOLINT
    EXPECT_TRUE(t2 != nullptr);
    EXPECT_EQ(1, BytePool.size());

    auto *t3 = BytePool.alloc(7, 8, 9);  // NOLINT
    EXPECT_TRUE(t3 != nullptr);
    EXPECT_EQ(0, BytePool.size());

    try {
        auto *t4 = BytePool.alloc();  // NOLINT
        FAIL() << "expected std::bad_alloc";
        if (t4 != nullptr) {
            BytePool.free(t4);  // NOT REACHED
        }
    } catch (std::bad_alloc &e) {
        // Expect std::bad_alloc to be thrown
    } catch (std::exception &e) {
        FAIL() << "unexpected exception " << e.what();
    }

    std::cout << "BytePool.size() = " << BytePool.size() << "\n";

    BytePool.free(t1);
    EXPECT_EQ(1, BytePool.size());

    // Free a nullptr
    BytePool.free(nullptr);
    EXPECT_EQ(1, BytePool.size());
}

void testBytePoolThread(msglib::BytePool<TestStruct> *BytePool) {
    constexpr int SIZE = 100;
    std::array<TestStruct *, SIZE> ptrs;
    for (int i = 0; i < SIZE; i++) {
        ptrs[i] = BytePool->alloc(i, i + 1, i + 2);
    }

    for (int i = SIZE - 1; i >= 0; i--) {
        BytePool->free(ptrs[i]);
    }
}

TEST(BytePoolTest, threads) {
    constexpr int SIZE = 300;
    msglib::BytePool<TestStruct> BytePool(SIZE);

    try {
        std::cout << "Starting threads\n";
        std::thread t1(testBytePoolThread, &BytePool);
        std::thread t2(testBytePoolThread, &BytePool);
        std::thread t3(testBytePoolThread, &BytePool);

        std::cout << "Joining threads\n";
        t1.join();
        t2.join();
        t3.join();
    } catch (std::exception &e) {
        FAIL() << "Unexpected exception " << e.what();
    }
}

#endif