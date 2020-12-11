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

int main(int argc, char *argv[]) {
    msglib::Pool<Foo> mp(256);

    Foo *p1 = mp.alloc();
    Foo *p2 = mp.alloc(44);

    std::cout << "p1->x=" << p1->x << "\n";
    std::cout << "p2->x=" << p2->x << "\n";

    mp.free(p1);
    mp.free(p2);

    Foo *p[256];
    for (int i = 0; i < 256; i++) {
        p[i] = mp.alloc(i);
    }
    try {
        Foo *n = mp.alloc(256);
    } catch (std::exception &e) {
        std::cout << "Caught " << e.what() << "\n";
    }

    for (int i = 0; i < 256; i++) {
        mp.free(p[i]);
    }

    return 0;
}