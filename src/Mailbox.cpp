#include "msglib/Mailbox.h"
#include "msglib/detail/MailboxData.h"

namespace msglib {

/**
 * @brief Static member declaration
 *
 */
detail::MailboxData Mailbox::s_mailboxData;

Mailbox::Mailbox() : 
    m_storage(std::make_unique<std::byte[]>(QUEUE_SIZE * sizeof(Message))),
    m_bytes(&m_storage[0],QUEUE_SIZE * sizeof(Message)),
    m_resource(&m_bytes),
    m_queue(QUEUE_SIZE, &m_resource) 
{ }

Mailbox::Mailbox(size_t size):
    m_storage(std::make_unique<std::byte[]>(size * sizeof(Message))),
    m_bytes(&m_storage[0],size * sizeof(Message)),
    m_resource(&m_bytes),
    m_queue(size, &m_resource) 
{ }   

bool Mailbox::Initialize()
{
    return s_mailboxData.Initialize();
}

bool Mailbox::Initialize(size_t smallSize, size_t smallCap, size_t largeSize, size_t largeCap)
{
    return s_mailboxData.Initialize(smallSize, smallCap, largeSize, largeCap);
}

bool Mailbox::RegisterForLabel(Label label) { return s_mailboxData.RegisterForLabel(label, this); }

bool Mailbox::UnregisterForLabel(Label label) { return s_mailboxData.UnregisterForLabel(label, this); }

void Mailbox::Receive(Message &msg) {
    m_queue.pop(msg);
}

void Mailbox::ReleaseMessage(Message &msg) {
    if (msg.m_data != nullptr) {
        if (msg.m_size <= s_mailboxData.smallSize()) {
            s_mailboxData.freeSmall(msg.m_data);
        } else {
            s_mailboxData.freeLarge(msg.m_data);
        }
    }
}

bool Mailbox::SendSignal(Label label) {
    std::lock_guard<std::mutex> guard(s_mailboxData.GetMutex());
    
    const auto &receivers = s_mailboxData.GetReceivers(label);
    for (const auto &receiver : receivers.m_receivers) {
        if (receiver == nullptr) {
            break;
        }
        Message m(label);
        receiver->m_queue.emplace(label);
    }
    return true;
}

}  // Namespace msglib