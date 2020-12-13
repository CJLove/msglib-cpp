# msglib-cpp

Mailbox messaging and timer support for C++14 and up

## TODO
- Remove dependency on BlockingCollection (complicates licensing)
- Refactor to hide more details currently exposed in Mailbox.h
- RAII approach to ensuring that `Mailbox::ReleaseMessage()` gets called