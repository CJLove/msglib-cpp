#include "msglib/Mailbox.h"
#include <iostream>
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

void displayMsg(const char *thread, msglib::Message &msg)
{
    switch (msg.m_label)
    {
        case 1:
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
        case 2:
            try {
                Message2 *m2 = msg.as<Message2>();
                std::cout << "Thread " << thread << " got Msg2 { " << m2->a << " }\n";
            } catch (std::exception &e)
            {
                std::cout << "Exception " << e.what() << " getting Message2\n";
            }
            break;
        case 3:
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
    mbox.RegisterForLabel(1);
    mbox.RegisterForLabel(2);
    while (true)
    {
        msglib::Message msg;

        mbox.Receive(msg);
        displayMsg("Thread1",msg);
        mbox.ReleaseMessage(msg);
    }
    mbox.UnregisterForLabel(1);
    mbox.UnregisterForLabel(2);
}

void thread2(int inst)
{
    msglib::Mailbox mbox;
    std::cout << "Thread " << inst << " registering for labels 1 & 3\n";
    mbox.RegisterForLabel(1);
    mbox.RegisterForLabel(3);
    while (true)
    {
        msglib::Message msg;
        mbox.Receive(msg);
        displayMsg("Thread2",msg);
        mbox.ReleaseMessage(msg);
    }
    mbox.UnregisterForLabel(1);
    mbox.UnregisterForLabel(3);
}

void thread3(int inst)
{
    msglib::Mailbox mbox;
    std::cout << "Thread " << inst << " registering for labels 4 & 5\n";
    mbox.RegisterForLabel(4);
    mbox.RegisterForLabel(5);
    while (true)
    {
        msglib::Message msg;

        mbox.Receive(msg);
        displayMsg("Thread3",msg);
        mbox.ReleaseMessage(msg);
    }
    mbox.UnregisterForLabel(1);
    mbox.UnregisterForLabel(3);
}

int main(int /* argc */, char** /* argv */)
{
    std::thread t1(thread1,1);
    std::thread t2(thread2,2);
    std::thread t3(thread3,3);

    sleep(1);

    // Main thread
    msglib::Mailbox mbox;

    Message3 msg3 { 1,2,3 };
    mbox.SendMessage(3,msg3);

    Message2 msg2 { true };
    mbox.SendMessage(2,msg2);

    Message1 msg1 { 1,2,3,4,true};
    mbox.SendMessage(1,msg1);

    mbox.SendSignal(4);

    mbox.SendSignal(5);

    mbox.RegisterForLabel(6);
    while (true)
    {
        sleep(1);
    }
    mbox.UnregisterForLabel(6);

    return 0;
}