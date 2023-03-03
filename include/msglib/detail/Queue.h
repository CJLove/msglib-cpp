#pragma once

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <mutex>

namespace msglib::detail {

/**
 * @brief Thread-safe queue which supports blocking and non-blocking pop() operations
 *
 * @tparam T - data type for elements in the queue
 */
template <class T>
class Queue {
public:
    /**
     * @brief Construct a new Queue object with specified capacity
     *
     * @param cap - queue capacity
     */
    explicit Queue(size_t cap, std::pmr::memory_resource* resource) : m_alloc(resource), m_queue(m_alloc), m_capacity(cap) {
    }

    // Disallow copy and move constructors
    Queue(const Queue& other) = delete;
    Queue(const Queue&& other) = delete;

    // Disallow asignment and move assignment
    Queue& operator=(const Queue&) = delete;
    Queue& operator=(const Queue&&) = delete;

    /**
     * @brief Push a new value onto the queue if there is available space
     *
     * @param value - value to be pushed on the queue
     * @return true - value added successfully
     * @return false - queue full
     */
    bool tryPush(T value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_queue.size() < m_capacity) {
            m_queue.push_back(value);
            m_condVariable.notify_one();
            return true;
        }
        return false;
    }

    /**
     * @brief Push a new value onto the queue
     *
     * @param value - value to be pushed onto the queue
     */
    bool push(T value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_queue.size() < m_capacity) {
            m_queue.push_back(value);
            m_condVariable.notify_one();
            return true;
        }
        return false;
        // throw std::runtime_error("Queue full");
    }

    /**
     * @brief Push a new value onto the queue which is constructed in place
     *
     * @tparam Args
     * @param args - arguments to construct value in place
     */
    template <typename... Args>
    bool emplace(Args&&... args) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_queue.size() < m_capacity) {
            m_queue.emplace_back(std::forward<Args>(args)...);
            m_condVariable.notify_one();
            return true;
        }
        return false;
        // throw std::runtime_error("Queue full");
    }

    /**
     * @brief Try to pop a value off of the queue, returning false if the queue is empty
     *
     * @param value - value returned from the queue
     * @return true - value was returned from the queue
     * @return false - queue is empty
     */
    bool tryPop(T& value) {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (m_queue.empty()) {
                return false;
            }
            value = m_queue.front();
            m_queue.pop_front();
        }

        return true;
    }

    /**
     * @brief Wait for up to a specified duration to pop an element from the queue
     *
     * @tparam Rep
     * @tparam Period
     * @param value - value returned from the queue
     * @param duration - how long to wait before returning false
     * @return true - element was dequeued
     * @return false - pop() operation timed out
     */
    template <class Rep, class Period>
    bool popWait(T& value, const std::chrono::duration<Rep, Period>& duration) {
        {
            std::unique_lock<std::mutex> uniqueLock(m_mutex);
            if (m_condVariable.wait_for(uniqueLock, duration, [this]() { return !m_queue.empty(); })) {
                value = m_queue.front();
                m_queue.pop_front();
                return true;
            } else {
                return false;
            }
        }
    }

    /**
     * @brief Pop a value off of the queue, blocking until
     *
     * @param value - value returned from the queue
     */
    void pop(T& value) {
        std::unique_lock<std::mutex> uniqueLock(m_mutex);
        m_condVariable.wait(uniqueLock, [this]() { return !m_queue.empty(); });
        value = m_queue.front();
        m_queue.pop_front();
    }

    /**
     * @brief Return true if the queue is empty
     *
     * @return true - queue is empty
     * @return false - queue is not empty
     */
    bool empty() {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_queue.empty();
    }

    /**
     * @brief Return the queue size
     *
     * @return size_t
     */
    size_t size() {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_queue.size();
    }

private:
    /**
     * @brief Polymorphic allocator to use for queue elements
     */
    std::pmr::polymorphic_allocator<T> m_alloc;

    /**
     * @brief Queue representation
     */
    std::pmr::deque<T> m_queue;

    /**
     * @brief Mutex synchronizing queue access
     */
    std::mutex m_mutex;

    /**
     * @brief Condition variable for blocking pop() operations
     */
    std::condition_variable m_condVariable;

    /**
     * @brief Capacity of the queue
     */
    size_t m_capacity;
};

}  // namespace msglib::detail