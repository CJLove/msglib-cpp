#include "msglib/Pool.h"
#include <array>
#include <iostream>

#define TRACE_METHOD() std::cout << this << " " << __PRETTY_FUNCTION__ << "\n";

constexpr int X_DEFAULT_VAL = 42;
constexpr int X_INIT_VAL = 44;

struct Foo {
    int x = X_DEFAULT_VAL;
    Foo() { TRACE_METHOD(); }
    explicit Foo(int x) : x(x) { TRACE_METHOD(); }
    ~Foo() { TRACE_METHOD(); };
};

int main(int /* argc */, char ** /* argv */ ) {
    constexpr int SIZE = 256;
    msglib::Pool<Foo> mp(SIZE);

    Foo *p1 = mp.alloc();
    Foo *p2 = mp.alloc(X_INIT_VAL);

    std::cout << "p1->x=" << p1->x << "\n";
    std::cout << "p2->x=" << p2->x << "\n";

    mp.free(p1);
    mp.free(p2);

    std::array<Foo*,SIZE> p;
    for (int i = 0; i < SIZE; i++) {
        p[i] = mp.alloc(i);
    }
    try {
        Foo *n = mp.alloc(SIZE);

        mp.free(n);
    } catch (std::exception &e) {
        std::cout << "Caught " << e.what() << "\n";
    }

    for (int i = 0; i < SIZE; i++) {
        mp.free(p[i]);
    }

    return 0;
}