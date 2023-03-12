#pragma once
#include "Message.h"
#include "detail/BytePool.h"
#include "detail/MailboxData.h"
#include "detail/Queue.h"
#include "detail/Receiver.h"
#include <array>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace msglib {

using Label = uint16_t;

/**
 * @brief Mailbox provides interfaces for sending and receiving messages to one or more subscribers
 */
class Mailbox {
public:
    const size_t QUEUE_SIZE = 256;

    /**
     * @brief Construct a new Mailbox object
     */
    Mailbox()
        : m_storage(std::make_unique<std::byte[]>(QUEUE_SIZE * sizeof(Message)))
        , m_bytes(&m_storage[0], QUEUE_SIZE * sizeof(Message))
        , m_resource(&m_bytes)
        , m_queue(QUEUE_SIZE, &m_resource) {
    }

    /**
     * @brief Construct a new Mailbox
     *
     */
    explicit Mailbox(size_t queueSize)
        : m_storage(std::make_unique<std::byte[]>(queueSize * sizeof(Message)))
        , m_bytes(&m_storage[0], queueSize * sizeof(Message))
        , m_resource(&m_bytes)
        , m_queue(queueSize, &m_resource) {
    }

    /**
     * @brief Disable copy construction
     */
    Mailbox(const Mailbox &) = delete;

    /**
     * @brief Disable move construction
     */
    Mailbox(Mailbox &&) = delete;

    /**
     * @brief Destroy the Mailbox object
     */
    ~Mailbox() = default;

    static bool Initialize() {
        return s_mailboxData.Initialize();
    }

    /**
     * @brief Initialize mailbox internals with the specified capacities
     *
     * @param smallSize - max size of elements in the "small" pool
     * @param smallCap - capacity of the "small" pool
     * @param largeSize - max size of elements in the "large" pool
     * @param largeCap - capacity of the "large" pool
     * @return true
     * @return false
     */
    static bool Initialize(size_t smallSize, size_t smallCap, size_t largeSize, size_t largeCap) {
        return s_mailboxData.Initialize(smallSize, smallCap, largeSize, largeCap);
    }

    /**
     * @brief Register to receive messages with this label
     *
     * @param label - message label to register
     */
    bool RegisterForLabel(Label label) {
        return s_mailboxData.RegisterForLabel(label, this);
    }

    /**
     * @brief Cancel registration to receive messages with this label
     *
     * @param label
     */
    bool UnregisterForLabel(Label label) {
        return s_mailboxData.UnregisterForLabel(label, this);
    }

    /**
     * @brief Release the data block associated with a message
     */
    void ReleaseMessage(Message &msg) {
        if (msg.m_data != nullptr) {
            if (msg.m_size <= s_mailboxData.smallSize()) {
                s_mailboxData.freeSmall(msg.m_data);
            } else {
                s_mailboxData.freeLarge(msg.m_data);
            }
        }
    }

    /**
     * @brief Send a message with a specific label and associated data of type T
     *
     * @tparam T - a POD type
     * @param label - the message label
     * @param t - an instance
     */
    template <typename T>
    bool SendMessage(Label label, const T &t) {
        std::lock_guard<std::mutex> guard(s_mailboxData.GetMutex());
        bool result = true;
        if (sizeof(T) > s_mailboxData.largeSize()) {
            return false;
        }
        bool isLarge = (sizeof(T) > s_mailboxData.smallSize());
        const auto &receivers = s_mailboxData.GetReceivers(label);
        for (const auto &receiver : receivers.m_receivers) {
            if (receiver == nullptr) {
                continue;
            }
            detail::DataBlock db;
            if (isLarge) {
                db = s_mailboxData.allocateLarge();
            } else {
                db = s_mailboxData.allocateSmall();
            }
            if (db.get() != nullptr) {
                if (db.put(t)) {
                    result &= receiver->m_queue.emplace(label, sizeof(T), db.get());
                } else {
                    s_mailboxData.freeSmall(db.get());
                    result = false;
                }
            } else {
                result = false;
            }
        }
        return result;
    }

    /**
     * @brief Send a signal with a specific label
     *
     * @param label - signal's label
     */
    bool SendSignal(Label label) {
        std::lock_guard<std::mutex> guard(s_mailboxData.GetMutex());

        const auto &receivers = s_mailboxData.GetReceivers(label);
        for (const auto &receiver : receivers.m_receivers) {
            if (receiver == nullptr) {
                continue;
            }
            Message m(label);
            receiver->m_queue.emplace(label);
        }
        return true;
    }

    /**
     * @brief Block and wait until a signal/message of a register type is received
     *
     * @param label - label of the signal/message which was received
     * @param msg - associated message data, or nullptr for signal
     *              Note: message is owned by the mailbox
     */
    void Receive(Message &msg) {
        m_queue.pop(msg);
    }

private:
    /**
     * @brief Shared mailbox state among all Mailbox instances
     */
    inline static detail::MailboxData s_mailboxData;

    /**
     * @brief Underlying data for the queue's monotonic buffer resource
     */
    std::unique_ptr<std::byte[]> m_storage;

    /**
     * @brief Monotonic buffer resource supporting this instance's queue
     */
    std::pmr::monotonic_buffer_resource m_bytes;

    /**
     * @brief Synchronized pool resource supporting this instance's queue
     */
    std::pmr::synchronized_pool_resource m_resource;

    /**
     * @brief Queue for this instance of the Mailbox class
     */
    detail::Queue<Message> m_queue;
};

/**
 * @brief MessageGuard is an RAII-style wrapper class to reclaim resources associated with a Message
 *        which has been received from a mailbox.
 */
class MessageGuard {
public:
    /**
     * Disable default construction
     */
    MessageGuard() = delete;

    /**
     * @brief Construct a new MessageGuard object for a specific mailbox and message
     *
     * @param mailbox
     * @param msg
     */
    MessageGuard(Mailbox &mailbox, Message &msg) : m_mailbox(mailbox), m_msg(msg) {
    }

    /**
     * @brief Free any datablock associated with this message
     *
     */
    ~MessageGuard() {
        if (m_msg.m_data != nullptr) {
            m_mailbox.ReleaseMessage(m_msg);
        }
    }

private:
    /**
     * @brief References to the specific mailbox and message
     */
    Mailbox &m_mailbox;
    Message &m_msg;
};

}  // Namespace msglib
