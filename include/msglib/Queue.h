#pragma once

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <cstdio>

namespace msglib {

/**
 * @brief Thread-safe queue which supports blocking and non-blocking pop() operations
 * 
 * @tparam T 
 */
template <class T>
class Queue {
public:
    /**
     * @brief Construct a new Queue object with specified capacity
     * 
     * @param cap - queue capacity
     */
    explicit Queue(size_t cap) : m_capacity(cap)
    {}

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
            m_queue.push(value);
            m_condVariable.notify_one();
            return true;
        }
        return false;
    }

    /**
     * @brief Push a new value onto the queue
     * 
     * @param value - value to be pushed onto the queue
     *
     */
    bool push(T value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_queue.size() < m_capacity) {
            m_queue.push(value);
            m_condVariable.notify_one();
            return true;
        }
        return false;
        //throw std::runtime_error("Queue full");
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
            m_queue.emplace(std::forward<Args>(args)...);
            m_condVariable.notify_one();
            return true;
        }
        return false; 
        //throw std::runtime_error("Queue full");
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
            m_queue.pop();
        }

        return true;
    }

    /**
     * @brief 
     * 
     * @tparam Rep 
     * @tparam Period 
     * @param value - value returned from the queue
     * @param duration - how long to wait before returning false
     * @return true - element was dequeued
     * @return false - pop() operation timed out
     */
    template<class Rep, class Period>
    bool popWait(T& value, const std::chrono::duration<Rep, Period>& duration) {
        {
            std::unique_lock<std::mutex> uniqueLock(m_mutex);
            if (m_condVariable.wait_for(uniqueLock, duration, [this]() { return !m_queue.empty(); })) {
                value = m_queue.front();
                m_queue.pop();
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
        m_queue.pop();
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
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condVariable;
    size_t m_capacity;
};

}  // Namespace msglib