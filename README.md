[![ci](https://github.com/CJLove/msglib-cpp/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/CJLove/msglib-cpp/actions/workflows/ci.yml)

# msglib-cpp

The msglib-cpp header-only library supports asynchronous messaging and signalling between threads within a process.

## Features
- No post-initialization dynamic memory allocation. C++17 std::pmr allocators are used to perform all memory allocation from
  pools allocated at initialization time.

## Requirements
- C++17 and later, as std::pmr is used
- spdlog and fmt (for example and sandbox code)
- googletest for unit tests
- Support for POSIX per-process timers

## Msglib Concepts
### Labels
A **label** is an arbitrary 16-bit value identifying a **signal** or **message** that will be sent between threads.

### Signals
A **signal** is a trigger with no accompanying data used as either an immediate or scheduled trigger for other thread(s) to take action.

### Messages
A **message** is a simple data structure (think C/C++ struct) defining data to be exchanged with other thread(s). Message types should satisfy the `std::is_trivially_copyable<>` trait.

Messages can fall into either a "small" or "large" category based on their size. By default the "small" category covers messages up to 256 bytes and the "large" category covers messages up to 2048 bytes, but this can be changed at initialization time. Messages are passed by value, with copies made for each recipient out of the "small" or "large" pools.

### Timers
A **timer** is used to specify that a particular signal should be sent at a specific scheduled poin in the future. Timers can either be one-shot or recurring. Timers are started and cancelled using a label.

# Using msglib-cpp
See the `mbox.cpp` and `timer.cpp` example code for complete examples of using msglib-cpp.

## Specifying "small" and "large" pool sizes and capacities
```c++
#include "msglib/Msglib.h"

// Specifying explicit "small" and "large" pool configurations:
// "small" pool element size 128 and capacity 1024
// "large" pool category wih size 8192 and capacity 32
msglib::Initialize(128,1024,8192,32);
```

## Mailbox
Instances of the `Mailbox` class can be declared per-thread or anywhere that messages or signals need to be sent or received.  Each instance has its own fixed-size queue for incoming signals and messages; this queue size can be specified at declaration time as a constructor argument.

The `Mailbox::RegisterForLabel()` method can be used to register/subscribe to receive a particular Label for this `Mailbox` instance.

The `Mailbox::UnregisterForLabel()` method can be used to unregister/unsubscribe from receiving a particular Label for this `Mailbox` instance.

```c++
// Mailbox with default queue size subscribing to labels 1 & 2
msglib::Mailbox mbox;
mbox.RegisterForLabel(1);
mbox.RegisterForLabel(2);
...
mbox.UnregisterForLabel(1);
mbox.UnregisterForLabel(2);

// Mailbox with explicit queue size subscribing to label 3
msglib::Mailbox mboxSmall(16);
mboxSmall.RegisterForLabel(3);
...
mboxSmall.UnregisterForLabel(3);
```

## Message and MessageGuard
The `Message` struct is used to represent a signal or message which has been received via the `Mailbox::Receive()`. It is comprised of a `Label` and pointer to any accompanying message data. The `Message::as<T>()` method can be used to return the message data as a particular message type T, providing that the `sizeof(T)` matches the message data size.

The message data held within a `Message` instance (if present) is a pointer to a pool of data internal to the `Mailbox` implementation, and should be released by calling `Mailbox::ReleaseMessage()` when the client is finished. 

The `MessageGuard` class is an RAII wrapper that can be used to automatically call `Mailbox::ReleaseMessage()` when the `Message` instance goes out of scope.

```c++
msglib::Mailbox mbox;
msglib::Message msg;
// Type data for label 1
struct MsgType {
  int a;
  int b;
};

mbox.Receive(msg);
msglib::MessageGuard guard(mbox, msg);
if (msg.m_label == 1) {
  MsgType *m = msg.as<MsgType>();
  if (m) {
    // Label 1 received and data interpreted as `MsgType`
    ...
  }
}
...
```

## TimerManager
The `TimerManager` class has static `StartTimer()` methods for starting timers using `timeval`, `timespec`, or `std::chrono::duration<>` arguments, specifying a label to be signalled when the timer fires.

Once started, a timer can be cancelled using the static `CancelTimer()` method.

```c++
// Signal label 4 every 500ms
msglib::TimerManager::StartTimer(4, 900ms, msglib::PERIODIC);

// Signal label 5 in 5000 nsec
timespec ts { 0, 5000 };
msglib::TimerManager::StartTimer(5, ts, msglib::ONE_SHOT);

// Signal label 6 in 30 usec
timeval tv { 0, 30 };
msglib::TimerManager::StartTimer(6, tv, msglib::ONE_SHOT);

// Cancel timer 4
msglib::TimerManager::CancelTimer(4);
```

