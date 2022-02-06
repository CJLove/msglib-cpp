# msglib-cpp

Mailbox messaging and timer support for C++17 and up

See the `pre-cpp17` branch

## TODO
- Add `Mailbox::initialize()` to be called from `main()` with sizing arguments and as trigger for initializing static data.
- static types are problematic if they throw (`s_mailboxData`)
- Switch to pmr allocator for pool
- Refactor to hide more details currently exposed in Mailbox.h and so details like Pool.h and Queue.h aren't public
