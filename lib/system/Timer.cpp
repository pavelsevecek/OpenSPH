#include "system/Timer.h"

NAMESPACE_SPH_BEGIN

std::unique_ptr<TimerThread> TimerThread::instance = nullptr;

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

std::shared_ptr<Timer> makeTimer(const int64_t interval,
    const std::function<void(void)>& callback,
    const Flags<TimerFlags> flags) {
    std::shared_ptr<Timer> timer = std::make_shared<Timer>(interval, flags);
    TimerThread* instance = TimerThread::getInstance();
    instance->registerTimer(timer, callback);
    return timer;
}

TimerThread::TimerThread() {
    closingDown = false;
    thread = std::thread([this]() { this->runLoop(); });
}

TimerThread::~TimerThread() {
    closingDown = true;
    ASSERT(thread.joinable());
    thread.join();
}

TimerThread* TimerThread::getInstance() {
    if (!instance) {
        instance = std::make_unique<TimerThread>();
    }
    return instance.get();
}

void TimerThread::registerTimer(const std::shared_ptr<Timer>& timer,
    const std::function<void(void)>& callback) {
    std::unique_lock<std::mutex> lock(mutex);
    entries.push(TimerEntry{ timer, callback });
}


void TimerThread::runLoop() {
    Array<TimerEntry> copies;
    while (!closingDown) {
        {
            std::unique_lock<std::mutex> lock(mutex);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void TimerThread::removeEntry(TimerEntry& entry) {
    /// \todo proper removing
    entry.timer.reset();
    /*std::unique_lock<std::mutex> lock(mutex);
    auto iter = std::find_if(entries.begin(), entries.end(), [&entry](TimerEntry& t) { //
        return t.timer == entry.timer;
    });
    entries.remove(iter - entries.begin());*/
}

NAMESPACE_SPH_END
