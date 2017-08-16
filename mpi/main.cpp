#include "mpi/Mpi.h"
#include "mpi/Serializable.h"
#include <iostream>

using namespace Sph;

class TestSerializable : public Serializable {
private:
    static Size hndl;

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
        ASSERT(hndl != Size(-1));
        return hndl;
    }

    virtual void registerHandle(const Size handle) {
        hndl = handle;
    }
};

Size TestSerializable::hndl = Size(-1);


int main() {
    Mpi& mpi = Mpi::getInstance();
    std::cout << mpi.getProcessorName() << ", rank: " << mpi.getProcessRank() << std::endl;

    mpi.registerData(makeClone<TestSerializable>());
    TestSerializable dummy;
    ASSERT(dummy.handle() == 0);

    if (mpi.isMaster()) {
        for (Size i = 0; i < 100; ++i) {
            dummy.data = i;
            mpi.send(dummy, i % mpi.getCommunicatorSize());
        }
    } else {
        while (true) {
            AutoPtr<Serializable> serializable = mpi.receive(RecvSource::ANYONE);
            TestSerializable* act = dynamic_cast<TestSerializable*>(&*serializable);
            ASSERT(act);
            std::cout << "Received " << act->data << " by " << mpi.getProcessRank() << std::endl;
        }
    }
    Mpi::shutdown();
    return 0;
}
