#include "sph/timestepping/TimeStepping.h"
#include "catch.hpp"
#include "models/AbstractModel.h"
#include "storage/Storage.h"
#include <iostream>

using namespace Sph;

struct TestModel : public Abstract::Model {
    Float period = 1._f;

    TestModel(const std::shared_ptr<Storage>& storage)
        : Abstract::Model(storage) {
        this->storage->template insert<QuantityKey::R>(); // add positions
    }

    virtual void compute(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        /// \todo get value and 1st and 2nd derivatives at once
        /// \todo get from storage using ID and explicitly specifying type and temporal enum ("manual")
        r           = storage.get<QuantityKey::R>();
        v           = storage.dt<QuantityKey::R>();
        dv          = storage.d2t<QuantityKey::R>();
        Float omega = 2._f * Math::PI / period;
        for (int i = 0; i < r.size(); ++i) {
            dv[i] = -Math::sqr(omega) * r[i];
        }
    }

    virtual Storage createParticles(const Abstract::Domain& UNUSED(domain),
                                    const Settings<BodySettingsIds>&) const override {
        NOT_IMPLEMENTED;
    }
};

const Float timeStep = 0.01_f;

template <typename TTimestepping, typename... TArgs>
void testTimestepping(TArgs&&... args) {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    TestModel model(storage);

    Array<Vector>& rs = storage->get<QuantityKey::R>();
    rs.push(Vector(1.f, 0.f, 0.f));
    Array<Vector>& vs = storage->dt<QuantityKey::R>();
    vs.push(Vector(0.f, 0.f, 0.f));
    Array<Vector>& dvs = storage->d2t<QuantityKey::R>();
    dvs.resize(1);

    TTimestepping timestepping(storage, std::forward<TArgs>(args)...);
    int n;
    for (float t = 0.f; t < 3.f; t += timestepping.getTimeStep()) {
        // std::cout << rs[0][X] << "  " << vs[0][X] << "  " << Math::cos(2.f * Math::PI * t) << std::endl;
        if ((n++ % 15) == 0) {
            REQUIRE(Math::almostEqual(rs[0],
                                      Vector(Math::cos(2.f * Math::PI * t), 0.f, 0.f),
                                      timeStep * 2._f * Math::PI));
            REQUIRE(Math::almostEqual(vs[0],
                                      Vector(-Math::sin(2.f * Math::PI * t) * 2.f * Math::PI, 0.f, 0.f),
                                      timeStep * Math::sqr(2._f * Math::PI)));
        }
        timestepping.step(&model);
    }
}

TEST_CASE("harmonic oscilator", "[timestepping]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, timeStep);

    testTimestepping<EulerExplicit>(settings);
    testTimestepping<PredictorCorrector>(settings);
}