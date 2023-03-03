#include "msglib/Mailbox.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>
#include <unistd.h>

struct Message1 {
    int a;
    int b;
    int c;
    int d;
    bool e;
};

struct Message2 {
    bool a;
};

struct Message3 {
    int a;
    int b;
    int c;
};

// Labels used
const msglib::Label Msg1 = 1;
const msglib::Label Msg2 = 2;
const msglib::Label Msg3 = 3;
const msglib::Label Msg4 = 4;
const msglib::Label Msg5 = 5;
const msglib::Label Exit = 999;

void displayMsg(const char *thread, msglib::Message &msg) {
    switch (msg.m_label) {
    case Msg1: {
        try {
            auto *m1 = msg.as<Message1>();
            spdlog::info("Thread {} got Msg1[ {} {} {} {} {} ]", thread, m1->a, m1->b, m1->c, m1->d, m1->e);
        } catch (std::exception &e) {
            spdlog::error("Thread {} exception {} getting Message1", thread, e.what());
        }
    } break;
    case Msg2:
        try {
            auto *m2 = msg.as<Message2>();
            spdlog::info("Thread {} got Msg2[ {} ]", thread, m2->a);
        } catch (std::exception &e) {
            spdlog::error("Thread {} exception {} getting Message2", thread, e.what());
        }
        break;
    case Msg3:
        try {
            auto *t = msg.as<Message3>();
            spdlog::info("Thread {} got Msg3[ {} {} {} ]", thread, t->a, t->b, t->c);
        } catch (std::exception &e) {
            spdlog::error("Thread {} exception {} getting Message3", thread, e.what());
        }
        break;
    default:
        spdlog::info("Thread {} got Signal {}", thread, msg.m_label);
        break;
    }
}

void thread1(int inst) {
    msglib::Mailbox mbox;
    spdlog::info("Thread {} registering for labels 1 & 2", inst);
    mbox.RegisterForLabel(Msg1);
    mbox.RegisterForLabel(Msg2);
    mbox.RegisterForLabel(Exit);
    while (true) {
        msglib::Message msg;

        mbox.Receive(msg);
        msglib::MessageGuard guard(mbox, msg);
        displayMsg("Thread1", msg);
        if (msg.m_label == Exit) {
            break;
        }
    }
    spdlog::info("Thread {} got Exit message", inst);
    mbox.UnregisterForLabel(Msg1);
    mbox.UnregisterForLabel(Msg2);
    mbox.UnregisterForLabel(Exit);
}

void thread2(int inst) {
    msglib::Mailbox mbox;
    spdlog::info("Thread {} registering for labels 1 & 3", inst);
    mbox.RegisterForLabel(Msg1);
    mbox.RegisterForLabel(Msg3);
    mbox.RegisterForLabel(Exit);
    while (true) {
        msglib::Message msg;
        mbox.Receive(msg);
        msglib::MessageGuard guard(mbox, msg);
        displayMsg("Thread2", msg);
        if (msg.m_label == Exit) {
            break;
        }
    }
    spdlog::info("Thread {} got Exit message", inst);
    mbox.UnregisterForLabel(Msg1);
    mbox.UnregisterForLabel(Msg3);
    mbox.UnregisterForLabel(Exit);
}

void thread3(int inst) {
    msglib::Mailbox mbox;
    spdlog::info("Thread {} registering for labels 4 & 5", inst);
    mbox.RegisterForLabel(Msg4);
    mbox.RegisterForLabel(Msg5);
    mbox.RegisterForLabel(Exit);
    while (true) {
        msglib::Message msg;

        mbox.Receive(msg);
        msglib::MessageGuard guard(mbox, msg);
        displayMsg("Thread3", msg);
        if (msg.m_label == Exit) {
            break;
        }
    }
    spdlog::info("Thread {} got Exit message", inst);
    mbox.UnregisterForLabel(Msg1);
    mbox.UnregisterForLabel(Msg3);
    mbox.UnregisterForLabel(Exit);
}

int main(int argc, char **argv) {
    size_t smallSize = 128;
    size_t smallCap = 128;
    size_t largeSize = 2048;
    size_t largeCap = 32;
    int c;
    while ((c = getopt(argc,argv,"s:S:l:L:?")) != EOF) {
        switch (c) {
        case 's':
            smallSize = std::stoul(optarg,nullptr,10);
            break;
        case 'S':
            smallCap = std::stoul(optarg,nullptr,10);
            break;
        case 'l':
            largeSize = std::stoul(optarg,nullptr,10);
            break;
        case 'L':
            largeCap = std::stoul(optarg,nullptr,10);
            break;
        case '?':
            spdlog::error("Usage:\nmbox -s <smallSize> -S <smallCap> -l <largeSize> -L <largeCap>");
            exit(1);
        }
    }

    // Main thread
    msglib::Mailbox mbox;
    mbox.Initialize(smallSize, smallCap, largeSize, largeCap);

    std::thread t1(thread1, 1);
    std::thread t2(thread2, 2);
    std::thread t3(thread3, 3);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    Message3 msg3 {1, 2, 3};
    mbox.SendMessage(Msg3, msg3);

    Message2 msg2 {true};
    mbox.SendMessage(Msg2, msg2);

    Message1 msg1 {1, 2, 3, 4, true};
    mbox.SendMessage(Msg1, msg1);

    mbox.SendSignal(Msg4);

    mbox.SendSignal(Msg5);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Send signal for all threads to exit
    mbox.SendSignal(Exit);

    try {
        t1.join();
        t2.join();
        t3.join();
    } catch (std::exception &e) {
        spdlog::error("Caught exception {} joining threads", e.what());
    }

    spdlog::info("Exiting main()");
    return 0;
}