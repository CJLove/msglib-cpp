#include "msglib/Mailbox.h"

namespace msglib {

/**
 * @brief Static member declaration
 *
 */
MailboxData Mailbox::s_mailboxData;

void MailboxData::RegisterForLabel(Label label, Mailbox *mbox) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto f = m_mailboxes.find(label);
    if (f == m_mailboxes.end()) {
        Receivers r(mbox);
        m_mailboxes[label] = r;
    } else {
        if (!f->second.add(mbox)) {
            throw std::runtime_error("Unable to add mailbox as listener");
        }
    }
}

void MailboxData::UnregisterForLabel(Label label, Mailbox *mbox) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto f = m_mailboxes.find(label);
    if (f != m_mailboxes.end()) {
        if (f->second.remove(mbox)) {
            m_mailboxes.erase(f);
        }
    }
}

Receivers &MailboxData::GetReceivers(Label label) {
    auto f = m_mailboxes.find(label);
    if (f != m_mailboxes.end()) {
        return f->second;
    }
    return m_receivers;
}

MailboxData::SmallBlock *MailboxData::getSmall() { 
    try {
        return m_smallPool.alloc(); 
    }
    catch (std::exception &) {
        return nullptr;
    }
}

void MailboxData::freeSmall(MailboxData::SmallBlock *msg) { m_smallPool.free(msg); }

MailboxData::LargeBlock *MailboxData::getLarge() { 
    try {
        return m_largePool.alloc(); 
    }
    catch (std::exception &) {
        return nullptr;
    }
}

void MailboxData::freeLarge(MailboxData::LargeBlock *msg) { m_largePool.free(msg); }

Mailbox::Mailbox() : m_queue(QUEUE_SIZE) { }

Mailbox::~Mailbox() = default;

void Mailbox::RegisterForLabel(Label label) { s_mailboxData.RegisterForLabel(label, this); }

void Mailbox::UnregisterForLabel(Label label) { s_mailboxData.UnregisterForLabel(label, this); }

void Mailbox::Receive(Message &msg) {
    m_queue.pop(msg);
}

void Mailbox::ReleaseMessage(Message &msg) {
    if (msg.m_data != nullptr) {
        if (msg.m_size < SMALL_SIZE) {
            s_mailboxData.freeSmall(reinterpret_cast<MailboxData::SmallBlock *>(msg.m_data));
        } else {
            s_mailboxData.freeLarge(reinterpret_cast<MailboxData::LargeBlock *>(msg.m_data));
        }
    }
}

void Mailbox::SendSignal(Label label) {
    std::lock_guard<std::mutex> guard(s_mailboxData.GetMutex());
    
    const auto &receivers = s_mailboxData.GetReceivers(label);
    for (const auto &receiver : receivers.m_receivers) {
        if (receiver == nullptr) {
            break;
        }
        Message m(label);
        receiver->m_queue.emplace(label);
    }
}

}  // Namespace msglib