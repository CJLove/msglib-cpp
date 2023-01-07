#pragma once
#include <cstdalign>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <atomic>

namespace msglib {
template <typename T>
class Pool {
public:
    using AllocatorType = std::pmr::polymorphic_allocator<T>;

    explicit Pool(size_t capacity):
        m_alloc(&m_resource),
        m_size(capacity),
        m_capacity(capacity) {
    }

    ~Pool() = default;

    size_t size() const { return m_size; }

    size_t capacity() const { return m_capacity; }

    template <typename... Args>
    T* alloc(Args &&... args) {
        if (m_size) {
            T* result = m_alloc.allocate(1);
            new (result) T(std::forward<Args>(args)...);
            m_size--;
            return result;
        }
        throw std::bad_alloc();
    }

    void free(T* t) {
        if (t) {
            m_size++;
            m_alloc.deallocate(t, 1);
        }
    }

private:
    // Implementation using std::pmr::synchronized_pool_resource:
    std::pmr::synchronized_pool_resource m_resource;
    AllocatorType m_alloc;
    std::atomic<size_t> m_size;
    std::atomic<size_t> m_capacity;
};

}  // Namespace msglib