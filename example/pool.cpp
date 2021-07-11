#include "msglib/Pool.h"
#include <iostream>

#define TRACE_METHOD() std::cout << this << " " << __PRETTY_FUNCTION__ << "\n";

struct Foo {
    int x = 42;
    Foo() { TRACE_METHOD(); }
    Foo(int x) : x(x) { TRACE_METHOD(); }
    ~Foo() { TRACE_METHOD(); };
};

using namespace msglib;

int main(int /* argc */, char ** /* argv */ ) {
    constexpr int SIZE = 256;
    msglib::Pool<Foo> mp(SIZE);

    Foo *p1 = mp.alloc();
    Foo *p2 = mp.alloc(44);

    std::cout << "p1->x=" << p1->x << "\n";
    std::cout << "p2->x=" << p2->x << "\n";

    mp.free(p1);
    mp.free(p2);

    Foo *p[SIZE];
    for (int i = 0; i < SIZE; i++) {
        p[i] = mp.alloc(i);
    }
    try {
        Foo *n = mp.alloc(256);

        mp.free(n);
    } catch (std::exception &e) {
        std::cout << "Caught " << e.what() << "\n";
    }

    for (int i = 0; i < SIZE; i++) {
        mp.free(p[i]);
    }

    return 0;
}