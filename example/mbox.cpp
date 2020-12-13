#include "msglib/Mailbox.h"
#include <iostream>
#include <thread>


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

void displayMsg(const char *thread, msglib::Message &msg)
{
    switch (msg.m_label)
    {
        case Msg1:
        {
            try {
                Message1 *m1 = msg.as<Message1>();
                std::cout << "Thread " << thread << " got Msg1 { " << m1->a << " " << m1->b << " " << m1->c << " " << m1->d << " " << m1->e << " }\n";
            } catch (std::exception &e)
            {
                std::cout << "Exception " << e.what() << " getting Message1\n";
            }
        }
        break;
        case Msg2:
            try {
                Message2 *m2 = msg.as<Message2>();
                std::cout << "Thread " << thread << " got Msg2 { " << m2->a << " }\n";
            } catch (std::exception &e)
            {
                std::cout << "Exception " << e.what() << " getting Message2\n";
            }
            break;
        case Msg3:
            try {
                Message3 *t = msg.as<Message3>();
                std::cout << "Thread " << thread << " got Msg3 { " << t->a << " " << t->b << " " << t->c << " }\n";
            } catch (std::exception &e)
            {
                std::cout << "Exception " << e.what() << " getting Message3\n";
            }
            break;
        default:
            std::cout << "Thread " << thread << " got Signal " << msg.m_label << "\n";
            break;
    }
}

void thread1(int inst)
{
    msglib::Mailbox mbox;
    std::cout << "Thread " << inst << " registering for labels 1 & 2\n";
    mbox.RegisterForLabel(Msg1);
    mbox.RegisterForLabel(Msg2);
    mbox.RegisterForLabel(Exit);
    while (true)
    {
        msglib::Message msg;

        mbox.Receive(msg);
        displayMsg("Thread1",msg);
        if (msg.m_label == Exit) {
            mbox.ReleaseMessage(msg);
            break;
        }
        mbox.ReleaseMessage(msg);
    }
    mbox.UnregisterForLabel(Msg1);
    mbox.UnregisterForLabel(Msg2);
    mbox.UnregisterForLabel(Exit);
}

void thread2(int inst)
{
    msglib::Mailbox mbox;
    std::cout << "Thread " << inst << " registering for labels 1 & 3\n";
    mbox.RegisterForLabel(Msg1);
    mbox.RegisterForLabel(Msg3);
    mbox.RegisterForLabel(Exit);
    while (true)
    {
        msglib::Message msg;
        mbox.Receive(msg);
        displayMsg("Thread2",msg);
        if (msg.m_label == Exit) {
            mbox.ReleaseMessage(msg);
            break;
        }        
        mbox.ReleaseMessage(msg);
    }
    mbox.UnregisterForLabel(Msg1);
    mbox.UnregisterForLabel(Msg3);
    mbox.UnregisterForLabel(Exit);
}

void thread3(int inst)
{
    msglib::Mailbox mbox;
    std::cout << "Thread " << inst << " registering for labels 4 & 5\n";
    mbox.RegisterForLabel(Msg4);
    mbox.RegisterForLabel(Msg5);
    mbox.RegisterForLabel(Exit);
    while (true)
    {
        msglib::Message msg;

        mbox.Receive(msg);
        displayMsg("Thread3",msg);
        if (msg.m_label == Exit) {
            mbox.ReleaseMessage(msg);
            break;
        }        
        mbox.ReleaseMessage(msg);
    }
    mbox.UnregisterForLabel(Msg1);
    mbox.UnregisterForLabel(Msg3);
    mbox.UnregisterForLabel(Exit);
}

int main(int /* argc */, char** /* argv */)
{
    std::thread t1(thread1,1);
    std::thread t2(thread2,2);
    std::thread t3(thread3,3);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Main thread
    msglib::Mailbox mbox;

    Message3 msg3 { 1,2,3 };
    mbox.SendMessage(Msg3,msg3);

    Message2 msg2 { true };
    mbox.SendMessage(Msg2,msg2);

    Message1 msg1 { 1,2,3,4,true};
    mbox.SendMessage(Msg1,msg1);

    mbox.SendSignal(Msg4);

    mbox.SendSignal(Msg5);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Send signal for all threads to exit
    mbox.SendSignal(Exit);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}