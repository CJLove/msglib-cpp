#pragma once
#include "Mailbox.h"
#include <cstdint>
#include <memory>
#include <time.h>

namespace msglib {
struct TimerManagerImpl;

class TimerManager {
public:
    enum TimerType_e {
        PERIODIC,
        ONE_SHOT
    };

    static void startTimer(const Label &label, const timespec &time, const TimerType_e type = PERIODIC);

    static void cancelTimer(const Label &label);

private:
    static std::unique_ptr<TimerManagerImpl> s_timerData;

};

}   // namespace msglib