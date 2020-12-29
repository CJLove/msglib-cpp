
#include "msglib/Thread.h"
#include <iostream>

using thrd::thread;
using namespace std::chrono_literals;

void thread1Func() {
    std::cout << "Starting thread1\n";

    while (true) {
        std::this_thread::sleep_for(1s);
    }
}

void thread2Func() {
    std::cout << "Starting thread2\n";

    while (true) {
        std::this_thread::sleep_for(1s);
    }
}

void thread3Func() {
    std::cout << "Starting thread3\n";

    while (true) {
        std::this_thread::sleep_for(1s);
    }    
}

std::ostream &dumpPolicy(int policy, std::ostream &os) {
    switch (policy) {
    case SCHED_BATCH:
        os << "SCHED_BATCH";
        break;
    case SCHED_RR:
        os << "SCHED_RR";
        break;
    case SCHED_FIFO:
        os << "SCHED_FIFO";
        break;
    case SCHED_OTHER:
        os << "SCHED_OTHER";
        break;
    default:
        break;
    }
    return os;
}

int main(int /* argc */, char** /* argv */) {
    std::thread thread1(thread1Func);
    std::thread thread2(thread2Func);
    thread thread3(thread3Func,"MyThread3");

    thread::setName(thread1, "MyThread1");
    thread::setName(thread2, "MySecondThread");

    thread::setScheduling(thread1, SCHED_RR, 2);
    thread::setScheduling(thread2, SCHED_FIFO, 1);

    std::cout << "Thread 1's name is " << thread::getName(thread1) << "\n";
    std::cout << "Thread 2's name is " << thread::getName(thread2) << "\n";
    std::cout << "Thread 3's name is " << thread3.getName() << "\n";

    int policy;
    sched_param parms = thread::getScheduling(thread1,policy);
    std::cout << "Thread 1 policy "; 
    dumpPolicy(policy,std::cout);
    std::cout << " priority " << parms.sched_priority << "\n";


    while (true) {
        std::this_thread::sleep_for(1s);
    }
}