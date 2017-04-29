#pragma once

/// Measuring time intervals and executing periodic events.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

NAMESPACE_SPH_BEGIN

enum class TimerFlags { PERIODIC = 1 << 0, START_EXPIRED = 1 << 1 };

class TimerThread;

enum class TimerUnit { SECOND, MILLISECOND, MICROSECOND };

/// Basic time-measuring tool. Starts automatically when constructed.
class Timer {
protected:
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    using Clock = std::chrono::system_clock;

    TimePoint started;
    int64_t interval;
    Flags<TimerFlags> flags;

public:
    /// Create timer with given expiration duration. Flag PERIODIC does not do anything in this case, user
    /// must check for expiration and possibly restart timer, this isn't provided by timer constructed this
    /// way.
    /// \param interval Timer interval in milliseconds. It isn't currently possible to create interval in
    /// different units, but you can explicitly specify units in elapsed() method.
    Timer(const int64_t interval = 0, const Flags<TimerFlags> flags = EMPTY_FLAGS);

    /// Reset elapsed duration to zero.
    void restart() {
        started = Clock::now();
    }

    /// Returns elapsed time in timer units. Does not reset the timer.
    template <TimerUnit TUnit>
    int64_t elapsed() const {
        switch (TUnit) {
        case TimerUnit::SECOND:
            return std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - started).count();
        case TimerUnit::MILLISECOND:
            return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
        case TimerUnit::MICROSECOND:
            return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count();
        }
    }

    /// Checks if the interval has already passed.
    bool isExpired() const {
        return elapsed<TimerUnit::MILLISECOND>() >= interval;
    }

    bool isPeriodic() const {
        return flags.has(TimerFlags::PERIODIC);
    }
};

/// Create timer with given interval and callback when time interval is finished. The callback is executed
/// only once by default, or periodically if PERIODIC flag is passed.
std::shared_ptr<Timer> makeTimer(const int64_t interval,
    const std::function<void(void)>& callback,
    const Flags<TimerFlags> flags = EMPTY_FLAGS);


/// Simple extension of Timer allowing to pause and continue timer.
class StoppableTimer : public Timer {
protected:
    TimePoint stopped;
    bool isStopped = false;

    auto elapsedImpl() const {
        if (!isStopped) {
            return Clock::now() - started;
        } else {
            return stopped - started;
        }
    }

public:
    /// Stops the timer. Function getElapsed() will report the same value from now on.
    void stop() {
        if (!isStopped) {
            isStopped = true;
            stopped = Clock::now();
        }
    }

    /// Resumes stopped timer.
    void resume() {
        if (isStopped) {
            isStopped = false;
            // advance started by stopped duration to report correct elapsed time of the timer
            started += Clock::now() - stopped;
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
    static std::unique_ptr<TimerThread> instance;
    std::thread thread;
    std::atomic<bool> closingDown;

    struct TimerEntry {
        std::weak_ptr<Timer> timer;
        std::function<void()> callback;
    };

    Array<TimerEntry> entries;
    std::mutex mutex;

public:
    TimerThread();

    ~TimerThread();

    static TimerThread* getInstance();

    void registerTimer(const std::shared_ptr<Timer>& timer, const std::function<void(void)>& callback);

private:
    void runLoop();

    void removeEntry(TimerEntry& entry);
};

NAMESPACE_SPH_END
