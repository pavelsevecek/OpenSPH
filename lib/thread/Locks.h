/// \file Locks.h
/// \brief Some additional locks, not (yet) available in standard library
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Assert.h"
#include <shared_mutex>

NAMESPACE_SPH_BEGIN

/// \brief Implements a reader-writer lock, which can be locked for reading and later upgraded to an exclusive
/// ownership of the mutex.
class UpgradeableMutex : public Immovable {
private:
    std::shared_timed_mutex shared;
    std::mutex exclusive;

public:
    /// \brief Locks the mutex for writing, blocking if currently owned by other thread.
    ///
    /// After the function returns, the calling thread has exclusive ownership of the mutex.
    void lockWriter() {
        lockUpgradeable();
        lockUpgrade();
    }

    /// \brief Unlocks the mutex locked for writing.
    void unlockWriter() {
        unlockUpgrade();
        unlockUpgradeable();
    }

    /// \brief Locks the mutex for reading, blocking if another thread locked the mutex for writing.
    ///
    /// After the function returns, the calling thread has shared ownership of the mutex with other threads
    /// that had locked it for reading.
    void lockReader() {
        shared.lock_shared();
    }

    /// \brief Unlocks the mutex locked for reading.
    void unlockReader() {
        shared.unlock_shared();
    }

    /// \brief Upgrades the reader lock to writer lock, blocking if currently owned by other thread.
    ///
    /// After the function returns, the calling thread has exclusive ownership of the mutex.
    void lockUpgrade() {
        shared.lock();
    }

    /// \brief Degrades the lock from writer to reader.
    void unlockUpgrade() {
        shared.unlock();
    }

private:
    void lockUpgradeable() {
        exclusive.lock();
    }
    void unlockUpgradeable() {
        exclusive.unlock();
    }
};

/// \brief Guard upgrading an \ref UpgradeableMutex to writer lock when it was previously locked for reading.
class UpgradedLock : public Noncopyable {
private:
    UpgradeableMutex* mutex = nullptr;

public:
    ~UpgradedLock() {
        if (mutex) {
            mutex->unlockUpgrade();
        }
    }

    UpgradedLock(UpgradedLock&& other) {
        mutex = other.mutex;
        other.mutex = nullptr;
    }

private:
    explicit UpgradedLock(UpgradeableMutex& mutex)
        : mutex(std::addressof(mutex)) {
        mutex.lockUpgrade();
    }

    friend class UpgradeableLock;
};

/// \brief Guard of \ref UpgradeableMutex for exception safety.
///
/// When created, the mutex passed in the constructor is locked for reading. It can be later upgraded to a
/// writer lock. The lock is automatically released in destructor.
class UpgradeableLock : public Immovable {
private:
    UpgradeableMutex& mutex;

public:
    explicit UpgradeableLock(UpgradeableMutex& mutex)
        : mutex(mutex) {
        mutex.lockReader();
    }

    ~UpgradeableLock() {
        mutex.unlockReader();
    }

    UpgradedLock upgrade() {
        return UpgradedLock(mutex);
    }
};

NAMESPACE_SPH_END
