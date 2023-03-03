# msglib-cpp

The msglib-cpp header-only library supports asynchronous messaging and signalling between threads within a process.

## Features
- No post-initialization dynamic memory allocation. C++17 std::pmr allocators are used to perform all memory allocation from
  pools allocated at initialization time.

## Requirements
- C++17 and later, as std::pmr is used

## Concepts
- Label - a label is an arbitrary 16-bit value identifying a signal or message type
- Message - a message is a simple data structure (think C/C++ struct) defining data to be exchanged between threads.
- Signal - a signal is either an immediate or scheduled trigger for another thread to take action.

## TODO
- sanitizer testing - fix tsan issue after TimerManager refactoring
- TimerManager refactoring
- finish changes to be header-only
- Refactor to hide more details currently exposed in Mailbox.h and so details like Pool.h and Queue.h aren't public
