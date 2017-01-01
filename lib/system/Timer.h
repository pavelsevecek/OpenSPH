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

enum class TimerUnit { MILLISECOND, MICROSECOND };

/// Basic time-measuring tool. Starts automatically when constructed.
class Timer : public Observable {
protected:
    using TTimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using TClock     = std::chrono::system_clock;

    TTimePoint started;
    int64_t interval;
    Flags<TimerFlags> flags;

public:
    /// Create timer with given expiration duration. Flag PERIODIC does not do anything in this case, user
    /// must check for expiration and possibly restart timer, this isn't provided by timer constructed this
    /// way.
    /// \param interval Timer interval in milliseconds. It isn't currently possible to create interval in
    /// different units, but you can explicitly specify units in elapsed() method.
    Timer(const int64_t interval = 0, const Flags<TimerFlags> flags = EMPTY_FLAGS);

    /// Create timer with given exporation duration and on-timer-expired callback. The callback is executed
    /// only once by default, or periodically if PERIODIC flag is passed.
    Timer(const int64_t interval,
          const std::function<void(void)>& callback,
          const Flags<TimerFlags> flags = EMPTY_FLAGS);

    /// Reset elapsed duration to zero.
    void restart() { started = TClock::now(); }

    /// Returns elapsed time in timer units. Does not reset the timer.
    template <TimerUnit TUnit>
    int64_t elapsed() const {
        switch (TUnit) {
        case TimerUnit::MILLISECOND:
            return std::chrono::duration_cast<std::chrono::milliseconds>(TClock::now() - started).count();
        case TimerUnit::MICROSECOND:
            return std::chrono::duration_cast<std::chrono::microseconds>(TClock::now() - started).count();
        }
    }

    /// Checks if the interval has already passed.
    bool isExpired() const { return elapsed<TimerUnit::MILLISECOND>() >= interval; }

    bool isPeriodic() const { return flags.has(TimerFlags::PERIODIC); }
};

/// Simple extension of Timer allowing to pause and continue timer.
class StoppableTimer : public Timer {
protected:
    TTimePoint stopped;
    bool isStopped = false;

    auto elapsedImpl() const {
        if (!isStopped) {
            return TClock::now() - started;
        } else {
            return stopped - started;
        }
    }

public:
    /// Stops the timer. Function getElapsed() will report the same value from now on.
    void stop() {
        if (!isStopped) {
            isStopped = true;
            stopped = TClock::now();
        }
    }

    /// Resumes stopped timer.
    void resume() {
        if (isStopped) {
            isStopped = false;
            // advance started by stopped duration to report correct elapsed time of the timer
            started += TClock::now() - stopped;
        }
    }

    /// Returns elapsed time in timer units. Does not reset the timer.
    template <TimerUnit TUnit>
    int64_t elapsed() const {
        switch (TUnit) {
        case TimerUnit::MILLISECOND:
            return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedImpl()).count();
        case TimerUnit::MICROSECOND:
            return std::chrono::duration_cast<std::chrono::microseconds>(elapsedImpl()).count();
        }
    }
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
