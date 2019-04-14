#include "Sph.h"

using namespace Sph;

// van der Waals equation state, implementing the IEos interface.
class VanDerWaalsEos : public IEos {
private:
    // van der Waals contants
    Float a, b;

    // adiabatic index
    Float gamma;

public:
    VanDerWaalsEos(const Float a, const Float b, const Float gamma)
        : a(a)
        , b(b)
        , gamma(gamma) {}

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override {
        // returns a pair of values - pressure and sound speed at given density and internal energy
        const Float R = Constants::gasConstant;

        // first, compute the temperature
        const Float v = 1._f / rho;
        const Float T = 2._f * (u + a / v) / (3._f * R);

        // now compute the pressure
        const Float p = max(R * T / (v - b) - a / sqr(v), EPS);
        ASSERT(isReal(p));

        /// \todo correct sound speed
        const Float cs = sqrt(gamma * p / rho);
        ASSERT(isReal(cs));

        return { p, cs };
    }


    virtual Float getInternalEnergy(const Float, const Float) const override {
        // not needed for this example
        NOT_IMPLEMENTED
    }

    virtual Float getDensity(const Float, const Float) const override {
        // not needed for this example
        NOT_IMPLEMENTED
    }
};

class VanDerWallsSimulation : public IRun {
public:
    virtual void setUp() override {
        settings.set(RunSettingsId::RUN_NAME, std::string("Van der Waals"));

        // We simulate a gass, so the only force in the system is due to pressure gradient
        settings.set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE);

        storage = makeShared<Storage>();
        InitialConditions ic(*scheduler, settings);
        SphericalDomain domain(Vector(0._f), 1.e3_f);
        BodySettings body;
        body.set(BodySettingsId::PARTICLE_COUNT, 10000);
        body.set(BodySettingsId::DENSITY, 2._f); // 2 kg/m^3

        // Prepare our custom EoS
        AutoPtr<VanDerWaalsEos> eos = makeAuto<VanDerWaalsEos>(1.38_f, 0.032_f, 1.4_f);

        // Create new material; this allows us to specify a custom EoS.
        AutoPtr<EosMaterial> material = makeAuto<EosMaterial>(body, std::move(eos));

        // Add the sphere with van der Waals gass into the particle storage; note that here we pass the
        // material instead of 'body' parameter
        ic.addMonolithicBody(*storage, domain, std::move(material));
    }

    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

int main() {
    VanDerWallsSimulation simulation;
    simulation.setUp();
    simulation.run();
    return 0;
}
