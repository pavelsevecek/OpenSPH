#include "system/Timer.h"
#include "objects/containers/String.h"
#include <atomic>
#include <iomanip>
#include <mutex>
#include <thread>

NAMESPACE_SPH_BEGIN

class TimerThread : public Noncopyable {
private:
    static AutoPtr<TimerThread> instance;
    std::thread thread;
    std::atomic<bool> closingDown;

    struct TimerEntry {
        WeakPtr<Timer> timer;
        std::function<void()> callback;
    };

    std::mutex entryMutex;
    Array<TimerEntry> entries;

public:
    TimerThread();

    ~TimerThread();

    static TimerThread& getInstance();

    void registerTimer(const SharedPtr<Timer>& timer, const std::function<void(void)>& callback);

private:
    void runLoop();

    void removeEntry(TimerEntry& entry);
};

AutoPtr<TimerThread> TimerThread::instance = nullptr;

Timer::Timer(const int64_t interval, const Flags<TimerFlags> flags)
    : interval(interval)
    , flags(flags) {
    if (flags.has(TimerFlags::START_EXPIRED)) {
        std::chrono::milliseconds ms(interval);
        started = Clock::now() - ms;
    } else {
        started = Clock::now();
    }
}

void Timer::restart() {
    started = Clock::now();
}

int64_t Timer::elapsed(const TimerUnit unit) const {
    switch (unit) {
    case TimerUnit::SECOND:
        return std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - started).count();
    case TimerUnit::MILLISECOND:
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
    case TimerUnit::MICROSECOND:
        return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - started).count();
    case TimerUnit::NANOSECOND:
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count();
    default:
        NOT_IMPLEMENTED;
    }
}

void StoppableTimer::stop() {
    if (!isStopped) {
        isStopped = true;
        stopped = Clock::now();
    }
}

void StoppableTimer::resume() {
    if (isStopped) {
        isStopped = false;
        // advance started by stopped duration to report correct elapsed time of the timer
        started += Clock::now() - stopped;
    }
}

int64_t StoppableTimer::elapsed(const TimerUnit unit) const {
    switch (unit) {
    case TimerUnit::MILLISECOND:
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedImpl()).count();
    case TimerUnit::MICROSECOND:
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsedImpl()).count();
    default:
        NOT_IMPLEMENTED;
    }
}


SharedPtr<Timer> makeTimer(const int64_t interval,
    const std::function<void(void)>& callback,
    const Flags<TimerFlags> flags) {
    SharedPtr<Timer> timer = makeShared<Timer>(interval, flags);
    TimerThread& instance = TimerThread::getInstance();
    instance.registerTimer(timer, callback);
    return timer;
}

TimerThread::TimerThread() {
    closingDown = false;
    thread = std::thread([this]() { this->runLoop(); });
}

TimerThread::~TimerThread() {
    closingDown = true;
    SPH_ASSERT(thread.joinable());
    thread.join();
}

TimerThread& TimerThread::getInstance() {
    if (!instance) {
        instance = makeAuto<TimerThread>();
    }
    return *instance;
}

void TimerThread::registerTimer(const SharedPtr<Timer>& timer, const std::function<void(void)>& callback) {
    std::unique_lock<std::mutex> lock(entryMutex);
    entries.push(TimerEntry{ timer, callback });
}

void TimerThread::runLoop() {
    Array<TimerEntry> copies;
    while (!closingDown) {
        {
            std::unique_lock<std::mutex> lock(entryMutex);
            copies = copyable(entries);
        }
        for (TimerEntry& entry : copies) {
            // if the timer is expired (and still exists)
            if (auto timerPtr = entry.timer.lock()) {
                if (timerPtr->isExpired()) {
                    // run callback
                    entry.callback();
                    if (timerPtr->isPeriodic()) {
                        timerPtr->restart();
                    } else {
                        // one time callback, remove timer from the list
                        this->removeEntry(entry);
                    }
                }
            } else {
                this->removeEntry(entry);
            }
        }
        ///  \todo sleep till the next timer expires (using CV)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void TimerThread::removeEntry(TimerEntry& entry) {
    /// \todo proper removing
    entry.timer.reset();
}

/// \todo can be improved after units are introduced into the code

const int64_t SECOND = 1000;
const int64_t MINUTE = 60 * SECOND;
const int64_t HOUR = 60 * MINUTE;
const int64_t DAY = 24 * HOUR;
const int64_t YEAR = 365 * DAY;

String getFormattedTime(int64_t time) {
    std::stringstream ss;
    bool doMinutes = time < YEAR;
    bool doSeconds = time < DAY;
    bool fixedDay = false;
    if (time >= YEAR) {
        ss << time / YEAR << "yr ";
        time = time % YEAR;
        fixedDay = true;
    }
    if (time >= DAY) {
        if (fixedDay) {
            ss << std::setw(3) << std::setfill('0') << time / DAY << "d ";
        } else {
            ss << time / DAY << "d ";
        }
        time = time % DAY;
    }
    if (time >= HOUR) {
        ss << std::setw(2) << std::setfill('0') << time / HOUR << "h ";
        time = time % HOUR;
    }
    if (doMinutes && time >= MINUTE) {
        ss << std::setw(2) << std::setfill('0') << time / MINUTE << "min ";
        time = time % MINUTE;
    }
    if (doSeconds) {
        ss << std::setw(2) << std::setfill('0') << time / SECOND;
        ss << "." << std::setw(3) << std::setfill('0') << time % SECOND << "s";
    }
    return String::fromAscii(ss.str().c_str());
}

NAMESPACE_SPH_END
