#include "system/Timer.h"

NAMESPACE_SPH_BEGIN

TimerThread* TimerThread::instance = nullptr;

Timer::Timer(const int64_t interval, const Flags<TimerFlags> flags)
    : interval(interval)
    , flags(flags) {
    if (flags.has(TimerFlags::START_EXPIRED)) {
        std::chrono::milliseconds ms(interval);
        started = TClock::now() - ms;
    } else {
        started = TClock::now();
    }
}

Timer::Timer(const int64_t interval, const std::function<void(void)>& callback, const Flags<TimerFlags> flags)
    : Timer(interval, flags) {
    TimerThread* instance = TimerThread::getInstance();
    instance->registerTimer(this, callback);
    instance->run();
}

void TimerThread::runLoop() {
    while (true) {
        for (TimerEntry& entry : entries) {
            // if the timer is expired (and still exists)
            if (entry.timer) {
                if (entry.timer->isExpired()) {
                    // run callback
                    entry.callback();
                    if (entry.timer->isPeriodic()) {
                        entry.timer->restart();
                    } else {
                        /// \todo actually remove the timer
                        entry.timer = nullptr;
                    }
                }
            } else {
                /// \todo remove expired
            }
        }
    }
}

NAMESPACE_SPH_END
