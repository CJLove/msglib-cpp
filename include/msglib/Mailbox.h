#pragma once
#include "detail/BytePool.h"
#include "Message.h"
#include "detail/Receiver.h"
#include "detail/Queue.h"
#include "detail/MailboxData.h"
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
    Mailbox();

    /**
     * @brief Construct a new Mailbox
     * 
     */
    explicit Mailbox(size_t queueSize);

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

    bool Initialize();

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
    bool Initialize(size_t smallSize, size_t smallCap, size_t largeSize, size_t largeCap);

    /**
     * @brief Register to receive messages with this label
     *
     * @param label - message label to register
     */
    bool RegisterForLabel(Label label);

    /**
     * @brief Cancel registration to receive messages with this label
     *
     * @param label
     */
    bool UnregisterForLabel(Label label);

    /**
     * @brief Release the data block associated with a message
     */
    void ReleaseMessage(Message &msg);

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

        const auto &receivers = s_mailboxData.GetReceivers(label);
        for (const auto &receiver : receivers.m_receivers) {
            if (receiver == nullptr) {
                break;
            }
            if (sizeof(T) > s_mailboxData.largeSize()) {
                return false;
            }
            detail::DataBlock db;
            if (sizeof(T) <= s_mailboxData.smallSize()) {
                db = s_mailboxData.allocateSmall();
            } else {
                db = s_mailboxData.allocateLarge();
            }
            if (db.get() != nullptr) {
                if (db.put(t)) {
                    receiver->m_queue.emplace(label, sizeof(T), db.get());
                    return true;
                } else {
                    s_mailboxData.freeSmall(db.get());
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * @brief Send a signal with a specific label
     *
     * @param label - signal's label
     */
    bool SendSignal(Label label);

    /**
     * @brief Block and wait until a signal/message of a register type is received
     *
     * @param label - label of the signal/message which was received
     * @param msg - associated message data, or nullptr for signal
     *              Note: message is owned by the mailbox
     */
    void Receive(Message &msg);

private:
    /**
     * @brief Shared mailbox state among all Mailbox instances
     */
    static detail::MailboxData s_mailboxData;

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
