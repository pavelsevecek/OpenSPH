#pragma once

/// Measuring time intervals and executing periodic events.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/NonOwningPtr.h"
#include <chrono>
#include <thread>

NAMESPACE_SPH_BEGIN

enum class TimerFlags { PERIODIC = 1 << 0, START_EXPIRED = 1 << 1 };

class TimerThread;

class Timer : public Observable {
private:
    using TTimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using TClock     = std::chrono::system_clock;

    TTimePoint started;
    int64_t interval;
    Flags<TimerFlags> flags;

public:
    /// Create timer with given expiration duration. Flag PERIODIC does not do anything in this case, user
    /// must check for expiration and possibly restart timer, this isn't provided by timer constructed this
    /// way.
    Timer(const int64_t interval, const Flags<TimerFlags> flags = EMPTY_FLAGS);

    /// Create timer with given exporation duration and on-timer-expired callback. The callback is executed
    /// only once by default, or periodically if PERIODIC flag is passed.
    Timer(const int64_t interval,
          const std::function<void(void)>& callback,
          const Flags<TimerFlags> flags = EMPTY_FLAGS);

    /// Reset elapsed duration to zero.
    void restart() { started = TClock::now(); }

    /// Returns elapsed time in milliseconds. Does not reset the timer.
    int64_t elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(TClock::now() - started).count();
    }

    bool isExpired() const { return elapsed() >= interval; }

    bool isPeriodic() const { return flags.has(TimerFlags::PERIODIC); }
};


class TimerThread : public Noncopyable {
private:
    static TimerThread* instance;
    std::thread thread;

    struct TimerEntry {
        NonOwningPtr<Timer> timer;
        std::function<void(void)> callback;
    };

    Array<TimerEntry> entries;

    void runLoop();

public:
    TimerThread() = default;

    ~TimerThread() {
        if (thread.joinable()) {
            thread.join();
        }
    }

    static TimerThread* getInstance() {
        if (!instance) {
            instance = new TimerThread();
        }
        return instance;
    }

    /// Starts the timer loop (if it isn't already running)
    void run() {
        if (thread.joinable()) {
            // already running
            return;
        }
        thread = std::thread([this]() { this->runLoop(); });
    }

    void registerTimer(Timer* timer, const std::function<void(void)>& callback) {
        entries.push(TimerEntry{ timer, callback });
    }
};

NAMESPACE_SPH_END
