#include "mpi/Mpi.h"
#include "mpi/MpiScheduler.h"
#include "mpi/Serializable.h"
#include <atomic>
#include <iostream>

using namespace Sph;

class TestSerializable : public ISerializable {
public:
    Size data = 0;

    virtual void serialize(Array<uint8_t>& buffer) const override {
        buffer.resize(sizeof(Size));
        memcpy(&buffer[0], &data, sizeof(data));
    }

    virtual void deserialize(ArrayView<uint8_t> buffer) override {
        ASSERT(buffer.size() >= sizeof(Size));
        memcpy(&data, &buffer[0], sizeof(Size));
    }

    virtual Size handle() const override {
        return 1234;
    }
};

int main() {
    Mpi& mpi = Mpi::getInstance();
    std::cout << mpi.getProcessorName() << ", rank: " << mpi.getProcessRank() << std::endl;

    mpi.record(makeClone<TestSerializable>());
    TestSerializable dummy;
    ASSERT(dummy.handle() == 0);

    if (mpi.isMaster()) {
        for (Size i = 0; i < 100; ++i) {
            dummy.data = i;
            mpi.send(dummy, i % mpi.getCommunicatorSize());
        }
    } else {
        while (true) {
            AutoPtr<ISerializable> serializable = mpi.receive(RecvSource::ANYONE);
            TestSerializable* act = dynamic_cast<TestSerializable*>(&*serializable);
            ASSERT(act);
            std::cout << "Received " << act->data << " by " << mpi.getProcessRank() << std::endl;
        }
    }

    /*  MpiScheduler scheduler;
      std::atomic_int sum{ 0 };

      class Task : public ITask, public ISerializable {
      private:
          std::atomic_int& sum;

      public:
          explicit Task(std::atomic_int& sum)
              : sum(sum) {}

          virtual void operator()() override {
              sum += i;
          }

          virtual void serialize(Array<uint8_t>& buffer) const override {
              int value = sum.load();
              buffer.resize(sizeof(value));
              memcpy(&buffer[0], &value, sizeof(value));
          }

          virtual void deserialize(ArrayView<uint8_t> buffer) {
              int value;
              memcpy(&value, &buffer[0], sizeof(int));
              sum.store(value);
          }

          virtual Size handle() const override {
              return 666;
          }
      };

      parallelFor(scheduler, 0, 1000, 10, makeAuto<Task>(sum));

      std::cout << "sum = " << sum << std::endl;*/

    Mpi::shutdown();
    return 0;
}
