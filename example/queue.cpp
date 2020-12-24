#include "Queue.h"
#include <thread>
#include <chrono>

using namespace msglib;
using namespace std::chrono_literals;

// Sample code using the Queue template class

struct Msg {
    int m_a;
    int m_b;

    Msg(int a, int b): m_a(a),m_b(b)
    {}

    Msg(): m_a(0),m_b(0)
    {}
};



void consumer(Queue<Msg> &queue)
{
    Msg msg;
    while (true) {
        queue.pop(msg);
        std::cout << "Received msg(" << msg.m_a << " " << msg.m_b << ")\n";
    }

}

void producer(Queue<Msg> &queue)
{
    int i = 0;
    std::this_thread::sleep_for(1s);

    while (true) {
        queue.emplace(i,i+1);
        i++;
        std::this_thread::sleep_for(100ms);
    }
}

int main(int /* argc */, char ** /* argv */)
{
    Queue<Msg> queue(5);
    std::thread t1(consumer,std::ref(queue));
    //std::thread t2(producer,std::ref(queue));
    producer(queue);


}