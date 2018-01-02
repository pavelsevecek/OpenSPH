#pragma once

/// \file Timer.h
/// \brief Measuring time intervals and executing periodic events.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/SharedPtr.h"
#include <chrono>
#include <functional>

NAMESPACE_SPH_BEGIN

enum class TimerFlags {
    /// Timer will execute callback periodically
    PERIODIC = 1 << 0,

    /// Creates expired timer, calling \ref elapsed immediately after creating will return the timer interval.
    START_EXPIRED = 1 << 1
};

enum class TimerUnit { SECOND, MILLISECOND, MICROSECOND, NANOSECOND };

/// Basic time-measuring tool. Starts automatically when constructed.
class Timer {
protected:
    using Clock = std::chrono::system_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint started;
    int64_t interval;
    Flags<TimerFlags> flags;

public:
    /// \brief Creates timer with given expiration duration.
    ///
    /// Flag \ref TimerFlags::PERIODIC does not do anything in this case, user must check for expiration and
    /// possibly restart timer, this isn't provided by timer constructed this way.
    /// \param interval Timer interval in milliseconds. It isn't currently possible to create interval in
    ///                 different units.
    /// \param flags Optional parameters of the timer, see \ref TimerFlags.
    Timer(const int64_t interval = 0, const Flags<TimerFlags> flags = EMPTY_FLAGS);

    /// Reset elapsed duration to zero.
    void restart();

    /// Returns elapsed time in timer units. Does not reset the timer.
    int64_t elapsed(const TimerUnit unit) const;

    /// Checks if the interval has already passed.
    bool isExpired() const {
        return elapsed(TimerUnit::MILLISECOND) >= interval;
    }

    bool isPeriodic() const {
        return flags.has(TimerFlags::PERIODIC);
    }
};

/// \brief Creates timer with given interval and callback when time interval is finished.
///
/// The callback is executed only once by default, or periodically if \ref TimerFlags::PERIODIC flag is
/// passed. If the timer is destroyed before the interval passes, no callback is called.
SharedPtr<Timer> makeTimer(const int64_t interval,
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
    void stop();

    /// Resumes stopped timer.
    void resume();

    /// Returns elapsed time in timer units. Does not reset the timer.
    int64_t elapsed(const TimerUnit unit) const;
};

/// \brief Returns the human-readable formatted time in suitable units
///
/// \param time Time in milliseconds
std::string getFormattedTime(const int64_t time);

NAMESPACE_SPH_END
