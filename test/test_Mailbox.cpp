#include "msglib/Mailbox.h"
#include "gtest/gtest.h"
#include <array>

using namespace msglib;  // NOLINT

struct MsgStruct {
    int a;
};

struct TestMessage {
    int a;
    int b;
    int c;
};

struct MsgBad {
    explicit MsgBad(int b) : m_b(b) {
    }
    int m_b;

    virtual void virtMethod() {
    }
};

struct MsgBig {
    char data[1024];
};

struct HugeMsg {
    char data[8192];
};

class MailboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        msglib::Mailbox mbox;
        mbox.Initialize();
    }
};

#if 0
TEST(MailboxText, DataBlock) {
    using DB = DataBlock<128>;  // NOLINT

    DB db;

    EXPECT_EQ(128, db.size());
    EXPECT_NE(nullptr, db.get());

    try {
        MsgStruct m {12345};  // NOLINT

        db.put(m);

        EXPECT_EQ(sizeof(m), db.size());
    } catch (std::exception &e) {
        FAIL() << "unexpected exception " << e.what();
    }

    // Following test is OBE since static_assert(is_trivially_copyable added)
    // try {
    //     MsgBad m {666}; // NOLINT

    //     db.put(m);
    //     FAIL() << "Unexpected successful put()";
    // } catch (std::exception &e) { std::cout << "Caught expected exception " << e.what() << "\n"; }
}

TEST_F(MailboxTest, MessageTest) {
    const Label label = 123;
    {
        // Default ctor
        Message msg;
        EXPECT_EQ(0, msg.m_label);
        EXPECT_EQ(0, msg.m_size);
        EXPECT_EQ(nullptr, msg.m_data);
    }

    {
        // Message as signal
        Message msg(label);
        EXPECT_EQ(label, msg.m_label);
        EXPECT_EQ(0, msg.m_size);
        EXPECT_EQ(nullptr, msg.m_data);
    }

    {
        // Message with data
        using DB = DataBlock<128>;  // NOLINT
        DB db;
        MsgStruct m {label};
        db.put(m);
        Message msg(label, sizeof(m), &db);
        EXPECT_EQ(123, msg.m_label);
        EXPECT_EQ(sizeof(m), msg.m_size);
        EXPECT_EQ(&db, msg.m_data);

        auto *mPtr = msg.as<MsgStruct>();
        EXPECT_EQ(label, mPtr->a);

        auto *badPtr = msg.as<MsgBad>();
        EXPECT_EQ(nullptr, badPtr);
    }
}
#endif

TEST_F(MailboxTest, Receivers) {
    detail::Receivers r;
    Mailbox mbox1;
    Mailbox mbox2;
    Mailbox mbox3;
    Mailbox mbox4;

    EXPECT_TRUE(r.add(&mbox1));
    EXPECT_EQ(&mbox1, r.m_receivers[0]);
    EXPECT_EQ(nullptr, r.m_receivers[1]);
    EXPECT_EQ(nullptr, r.m_receivers[2]);

    EXPECT_TRUE(r.add(&mbox2));
    EXPECT_EQ(&mbox1, r.m_receivers[0]);
    EXPECT_EQ(&mbox2, r.m_receivers[1]);
    EXPECT_EQ(nullptr, r.m_receivers[2]);

    EXPECT_TRUE(r.add(&mbox3));
    EXPECT_EQ(&mbox1, r.m_receivers[0]);
    EXPECT_EQ(&mbox2, r.m_receivers[1]);
    EXPECT_EQ(&mbox3, r.m_receivers[2]);

    EXPECT_FALSE(r.add(&mbox4));

    EXPECT_FALSE(r.remove(&mbox1));
    EXPECT_EQ(nullptr, r.m_receivers[0]);
    EXPECT_EQ(&mbox2, r.m_receivers[1]);
    EXPECT_EQ(&mbox3, r.m_receivers[2]);

    EXPECT_FALSE(r.remove(&mbox2));
    EXPECT_EQ(nullptr, r.m_receivers[0]);
    EXPECT_EQ(nullptr, r.m_receivers[1]);
    EXPECT_EQ(&mbox3, r.m_receivers[2]);

    EXPECT_TRUE(r.remove(&mbox3));
    EXPECT_EQ(nullptr, r.m_receivers[0]);
    EXPECT_EQ(nullptr, r.m_receivers[1]);
    EXPECT_EQ(nullptr, r.m_receivers[2]);
}

#if 0
TEST(MailboxTest, MailboxDataAlloc) {
    MailboxData data;

    std::array<MailboxData::SmallBlock *, SMALL_CAP> smallBlocks;
    for (int i = 0; i < SMALL_CAP; i++) {
        smallBlocks[i] = data.getSmall();
        EXPECT_TRUE(smallBlocks[i] != nullptr);
    }

    auto *smallPtr = data.getSmall();
    EXPECT_EQ(nullptr, smallPtr);

    for (int i = 0; i < SMALL_CAP; i++) {
        data.freeSmall(smallBlocks[i]);
    }

    smallPtr = data.getSmall();
    EXPECT_NE(nullptr, smallPtr);
    data.freeSmall(smallPtr);

    std::array<MailboxData::LargeBlock *, LARGE_CAP> largeBlocks;
    for (int i = 0; i < LARGE_CAP; i++) {
        largeBlocks[i] = data.getLarge();
        EXPECT_TRUE(largeBlocks[i] != nullptr);
    }

    auto *largePtr = data.getLarge();
    EXPECT_EQ(nullptr, largePtr);

    for (int i = 0; i < LARGE_CAP; i++) {
        data.freeLarge(largeBlocks[i]);
    }

    largePtr = data.getLarge();
    EXPECT_NE(nullptr, largePtr);
    data.freeLarge(largePtr);
}

TEST(MailboxTest, MailboxDataRegister) {
    const Label label {123};
    const Label badLabel {456};

    Mailbox mbox;
    Mailbox mbox2;
    MailboxData data;
    data.RegisterForLabel(label, &mbox);

    auto &emptyReceivers = data.GetReceivers(badLabel);
    EXPECT_EQ(nullptr, emptyReceivers.m_receivers[0]);
    EXPECT_EQ(nullptr, emptyReceivers.m_receivers[1]);
    EXPECT_EQ(nullptr, emptyReceivers.m_receivers[2]);

    {
        auto &receivers = data.GetReceivers(label);

        EXPECT_EQ(&mbox, receivers.m_receivers[0]);
        EXPECT_EQ(nullptr, receivers.m_receivers[1]);
        EXPECT_EQ(nullptr, receivers.m_receivers[2]);
    }

    data.RegisterForLabel(label, &mbox2);
    {
        auto &receivers = data.GetReceivers(label);
        EXPECT_EQ(&mbox, receivers.m_receivers[0]);
        EXPECT_EQ(&mbox2, receivers.m_receivers[1]);
        EXPECT_EQ(nullptr, receivers.m_receivers[2]);
    }

    data.UnregisterForLabel(label, &mbox);

    {
        auto &receivers = data.GetReceivers(label);
        EXPECT_EQ(nullptr, receivers.m_receivers[0]);
        EXPECT_EQ(&mbox2, receivers.m_receivers[1]);
        EXPECT_EQ(nullptr, receivers.m_receivers[2]);
    }

    data.UnregisterForLabel(label, &mbox2);

    {
        auto &receivers = data.GetReceivers(label);
        EXPECT_EQ(nullptr, receivers.m_receivers[0]);
        EXPECT_EQ(nullptr, receivers.m_receivers[1]);
        EXPECT_EQ(nullptr, receivers.m_receivers[2]);
    }
}
#endif

TEST_F(MailboxTest, SendFail) {
    Mailbox mbx;
    Label Msg1 = 555;  // NOLINT

    HugeMsg msg;

    EXPECT_FALSE(mbx.SendMessage(Msg1,msg));
}

TEST_F(MailboxTest, MailboxTest) {
    Label Msg1 = 555;  // NOLINT
    Label Msg2 = 666;  // NOLINT
    Label Msg3 = 777;  // NOLINT

    Mailbox mbox1;
    Mailbox mbox2;
    Mailbox mbox3(10);

    mbox1.RegisterForLabel(Msg1);
    mbox1.RegisterForLabel(Msg2);
    mbox2.RegisterForLabel(Msg2);
    mbox2.RegisterForLabel(Msg3);

    TestMessage m1 {3, 2, 1};
    mbox3.SendMessage(Msg1, m1);

    {
        Message msg;
        mbox1.Receive(msg);
        MessageGuard guard(mbox1, msg);

        EXPECT_EQ(Msg1, msg.m_label);
        auto *m1Ptr = msg.as<TestMessage>();
        EXPECT_EQ(3, m1Ptr->a);
        EXPECT_EQ(2, m1Ptr->b);
        EXPECT_EQ(1, m1Ptr->c);
    }

    MsgBig bigMsg;

    EXPECT_TRUE(mbox3.SendMessage(Msg2, bigMsg));
    {
        Message msg;
        mbox1.Receive(msg);
        MessageGuard guard(mbox1, msg);

        EXPECT_EQ(Msg2, msg.m_label);
        auto *bigPtr = msg.as<MsgBig>();
        EXPECT_TRUE(bigPtr != nullptr);
    }
    {
        Message msg;
        mbox2.Receive(msg);
        MessageGuard guard(mbox2, msg);

        EXPECT_EQ(Msg2, msg.m_label);
        auto *bigPtr = msg.as<MsgBig>();
        EXPECT_TRUE(bigPtr != nullptr);

        auto *m1Ptr = msg.as<TestMessage>();
        EXPECT_TRUE(m1Ptr == nullptr);
    }

    mbox3.SendSignal(Msg3);
    {
        Message msg;
        mbox2.Receive(msg);
        MessageGuard guard(mbox2, msg);

        EXPECT_EQ(Msg3, msg.m_label);
    }

    mbox1.UnregisterForLabel(Msg1);
    mbox1.UnregisterForLabel(Msg2);
    mbox2.UnregisterForLabel(Msg2);
    mbox2.UnregisterForLabel(Msg3);
}
