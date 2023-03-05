#pragma once
#include <atomic>
#include <cstdalign>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <mutex>

namespace msglib::detail {

/**
 * @brief DataBlock represents the result of trying to allocate a block from a BytePool
 */
class DataBlock {
public:
    /**
     * @brief Construct a new DataBlock wrapping a block from a BytePool
     * 
     * @param size - size allocated from BytePool in bytes
     * @param ptr - memory allocated from BytePool
     */
    DataBlock(size_t size, std::byte *ptr):
        m_size(size), m_data(ptr)
    {}

    /**
     * @brief Construct a DataBlock representing a failed allocation from a BytePool
     */
    DataBlock(): m_size(0), m_data(nullptr)
    {}

    /**
     * @brief Return a pointer to the DataBlock's data buffer
     * 
     * @return std::byte* 
     */
    std::byte *get() {
        return m_data;
    }

    /**
     * @brief Put a message type's content into the DataBlock's data buffer
     * 
     * Note: static assert proves that the template parameter is trivially
     *       copyable. Runtime checks ensure that there is an underlying 
     *       data buffer and the template type will fit in the buffer.
     * 
     * @tparam T - message type (e.g. a trivially copyable struct)
     * @param t - instance of the message type
     * @return true - message data was put into the DataBlock's buffer
     * @return false - failure
     */
    template <typename T>
    bool put(const T &t) {
        static_assert(std::is_trivially_copyable_v<T>,
                  "DataBlock requires trivially copyable types");
        if (m_data != nullptr && sizeof(T) <= m_size && std::is_trivially_copyable<T>()) {
            memcpy(m_data, &t, sizeof(T));
            return true;
        }
        return false;
    }

    /**
     * @brief Return the size of the DataBlock (the element size from the BytePool)
     * 
     * @return size_t - buffer size
     */
    [[nodiscard]] size_t size() const {
        return m_size;
    }
    
private:
    /**
     * @brief Element size from the BytePool
     */
    size_t m_size;

    /**
     * @brief Pointer to the element buffer
     */
    std::byte *m_data;
};

/**
 * @brief BytePool is a fixed-block allocator where the block size and pool
 *        capacity is specified at instantiation time.
 *        Internally std::pmr::polymorphic_allocator<std::byte> is used; the
 *        underlying memory_resource for this allocator is assumed to be
 *        std::pmr::synchronized_BytePool_resource for thread safety
 */
class BytePool {
public:
    /**
     * @brief Construct a new Byte Pool object
     *
     * @param eltSize - size in bytes of each element
     * @param capacity - number of elements
     * @param resource - underlying PMR memory_resource
     *
     * TODO: Pad eltSize to ensure that each element is aligned properly
     */
    BytePool(size_t eltSize, size_t capacity, std::pmr::memory_resource *resource)
        : m_alloc(resource), m_eltSize(eltSize), m_size(capacity), m_capacity(capacity) {
    }

    /**
     * @brief Disable copy construction
     */
    BytePool(const BytePool &rhs) = delete;

    /**
     * @brief Disable move construction
     */
    BytePool(BytePool &&rhs) = delete;

    /**
     * @brief Destroy the Byte Pool object
     * 
     */
    ~BytePool() = default;

    /**
     * @brief Disable assignment
     */
    BytePool &operator=(const BytePool &rhs) = delete;

    /**
     * @brief Disable move assignment
     */
    BytePool &operator=(BytePool &&rhs) = delete;

    /**
     * @brief Return the current number of allocated elements
     *
     * @return size_t
     */
    [[nodiscard]] size_t size() const {
        return m_size;
    }

    /**
     * @brief Return the pool's capacity
     *
     * @return size_t
     */
    [[nodiscard]] size_t capacity() const {
        return m_capacity;
    }

    /**
     * @brief Return the element size
     *
     * @return size_t
     */
    [[nodiscard]] size_t eltSize() const {
        return m_eltSize;
    }

    /**
     * @brief Allocate an element from the BytePool
     * 
     * @return DataBlock - encapsulates the allocated element if successful or
     *                     indicates allocation failure
     */
    DataBlock alloc() {
        if (m_size > 0) {
            try {
                auto *result = m_alloc.allocate(m_eltSize);
                m_size--;
                return DataBlock(m_eltSize,result);
            } catch (...) {
                // Contain PMR exception here
                return DataBlock();
            }
        }
        return DataBlock();
    }

    /**
     * @brief Return an allocated element to the BytePool
     * 
     * @param t 
     */
    void free(std::byte *t) {
        if (t != nullptr) {
            try {
                m_alloc.deallocate(t, m_eltSize);
                m_size++;
            } catch (...) {
                // Contain PMR exception here
            }
        }
    }

private:
    /**
     * @brief Polymorphic allocator using an underlying std::pmr::synchronized_pool_resource
     */
    std::pmr::polymorphic_allocator<std::byte> m_alloc;

    /**
     * @brief Fixed size for elements in the BytePool
     */
    size_t m_eltSize;

    /**
     * @brief Current number of elements allocated from the BytePool
     */
    std::atomic<size_t> m_size;

    /**
     * @brief Capacity of elements in the BytePool
     */
    std::atomic<size_t> m_capacity;
};

}  // namespace msglib::detail