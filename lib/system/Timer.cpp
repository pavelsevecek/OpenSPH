#include "system/Timer.h"
#include "objects/wrappers/LockingPtr.h"
#include <atomic>
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

    LockingPtr<Array<TimerEntry>> entries;

public:
    TimerThread();

    ~TimerThread();

    static TimerThread* getInstance();

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
    TimerThread* instance = TimerThread::getInstance();
    instance->registerTimer(timer, callback);
    return timer;
}

TimerThread::TimerThread() {
    closingDown = false;
    entries = makeLocking<Array<TimerEntry>>();
    thread = std::thread([this]() { this->runLoop(); });
}

TimerThread::~TimerThread() {
    closingDown = true;
    ASSERT(thread.joinable());
    thread.join();
}

TimerThread* TimerThread::getInstance() {
    if (!instance) {
        instance = makeAuto<TimerThread>();
    }
    return instance.get();
}

void TimerThread::registerTimer(const SharedPtr<Timer>& timer, const std::function<void(void)>& callback) {
    entries->push(TimerEntry{ timer, callback });
}


void TimerThread::runLoop() {
    Array<TimerEntry> copies;
    while (!closingDown) {
        {
            auto proxy = entries.lock();
            copies = copyable(*proxy);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void TimerThread::removeEntry(TimerEntry& entry) {
    /// \todo proper removing
    entry.timer.reset();
}

NAMESPACE_SPH_END
