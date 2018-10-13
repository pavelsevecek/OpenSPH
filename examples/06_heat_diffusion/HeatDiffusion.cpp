#include "Sph.h"

using namespace Sph;

// In this example, we need to set up some material parameters that are not present in BodySettingsId enum,
// so they cannot be stored directly in BodySettings object. To do this, we can:
// 1) Simply assign unique indices to your parameters and cast them to BodySettingsId.
// 2) Create a custom material by deriving from IMaterial and store the parameters manually.
// Here we show how to implement the second solution.

enum class CustomMaterialId {
    // Initial temperature of the material
    INITIAL_TEMPERATURE,

    // Thermal conductivity
    CONDUCTIVITY,

    // Density (yes, there is BodySettingsId::DENSITY, but we duplicate it to have all parameters
    // at one place).
    DENSITY,

    // Specific heat capacity
    CAPACITY,

    // Albedo of the surcae
    ALBEDO,
};

struct CustomMaterial : public NullMaterial {

    // Map storing our custom material parameters.
    FlatMap<CustomMaterialId, Float> parameters;

    CustomMaterial()
        : NullMaterial(BodySettings::getDefaults()) {}
};

// Our custom solver, computing temperature derivatives using 1D finite differences.
class HdeSolver : public ISolver {
private:
    // Parameters related to the boundary conditions

    // Solar constant
    Float Phi = 1360._f;

    // Rotational frequency of the asteroid
    Float omega = 2._f * PI / (60._f * 60._f * 2._f);


    // Numerical parameters of the solver

    // Number of discretization elements in space
    Size n = 1000;

    // Step in position
    Float dz = 0.001_f;

public:
    // Function create should initialize all quantities in the provided storage, using the given material
    // parameters. It is called for every created body (only once in this example).
    virtual void create(Storage& storage, IMaterial& material) const override {
        // Cast to access our material parameters
        CustomMaterial& customMaterial = static_cast<CustomMaterial&>(material);

        // Initialize the temperature in the storage
        Array<Float> T(n);
        T.fill(customMaterial.parameters[CustomMaterialId::INITIAL_TEMPERATURE]);
        storage.insert<Float>(QuantityId::TEMPERATURE, OrderEnum::FIRST, std::move(T));
    }

    // Main entry point of the solver. The function should compute the temporal derivatives of all time
    // dependent quantities. In our case, we have just one quantity - temperature. We compute the derivative
    // using the heat diffusion equation and save them to corresponding array in the storage.
    virtual void integrate(Storage& storage, Statistics& stats) override {
        // Get the quantities from the storage
        Array<Float>& T = storage.getValue<Float>(QuantityId::TEMPERATURE);
        Array<Float>& dT = storage.getDt<Float>(QuantityId::TEMPERATURE);

        // Storage can contain multiple materials. While this example only uses one material, here we show a
        // more general approach: we obtain material parameters from the material of each particle.
        for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {

            // This object holds a reference to the material and a list of particles with that material.
            MaterialView mat = storage.getMaterial(matId);

            // Cast to access our parameter map
            FlatMap<CustomMaterialId, Float>& parameters =
                static_cast<CustomMaterial&>(mat.material()).parameters;

            // Iterate over all the particles with this material
            for (Size i : mat.sequence()) {

                if (i == 0) {
                    // First element: we need to apply the boundary condition on the surface.
                    // Set the temperature gradient based on the illumination and the surface temperature.
                    const Float A = parameters[CustomMaterialId::ALBEDO];
                    const Float K = parameters[CustomMaterialId::CONDUCTIVITY];
                    const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
                    const Float incidentFlux = (1._f - A) * Phi * max(cos(omega * t), 0._f);
                    const Float emissionFlux = Constants::stefanBoltzmann * pow<4>(T[0]);
                    T[0] = T[1] + dz * (incidentFlux - emissionFlux) / K;
                } else if (i == n - 1) {
                    // Last element: we need to apply the boundary condition at "infinity".
                    // Set the temperature gradient to zero
                    T[n - 1] = T[n - 2];
                } else {
                    // Compute the temporal derivative using the laplacian
                    const Float K = parameters[CustomMaterialId::CONDUCTIVITY];
                    const Float rho = parameters[CustomMaterialId::DENSITY];
                    const Float C = parameters[CustomMaterialId::CAPACITY];
                    dT[i] = K / (rho * C) * (T[i + 1] + T[i - 1] - 2._f * T[i]) / sqr(dz);

                    // Note that it is enough to set the temporal derivative. We do not have to integrate the
                    // quantity manually, the code does this automatically.
                }
            }
        }
    }
};

// Custom logger, writing the current progress to standard output every 2 minutes.
// It also writes the surface temperature to file 'temperature.txt'.
class ProgressLogger : public PeriodicTrigger {
private:
    StdOutLogger stdout;
    FileLogger file;

public:
    ProgressLogger()
        : PeriodicTrigger(120._f, 0._f)
        , file(Path("temperature.txt")) {}

    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) override {
        const Float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);
        stdout.write("Progress: ", int(progress * 100._f), "%");

        const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
        Array<Float>& T = storage.getValue<Float>(QuantityId::TEMPERATURE);
        file.write(time, "   ", T[0]);
        return nullptr;
    }
};

class Hde : public IRun {
public:
    virtual void setUp() override {

        // Create the material and set up the parameters
        AutoPtr<CustomMaterial> material = makeAuto<CustomMaterial>();
        material->parameters[CustomMaterialId::INITIAL_TEMPERATURE] = 250._f;
        material->parameters[CustomMaterialId::CONDUCTIVITY] = 1._f;
        material->parameters[CustomMaterialId::DENSITY] = 2500._f;
        material->parameters[CustomMaterialId::CAPACITY] = 680._f;
        material->parameters[CustomMaterialId::ALBEDO] = 0.1_f;

        // Create the storage, passing our custom material
        storage = makeShared<Storage>(std::move(material));

        // Set the solver and create the quantities
        solver = makeAuto<HdeSolver>();
        solver->create(*storage, storage->getMaterial(0));

        // Set up the integrator
        settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT);
        settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
        settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.5_f);
        settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 60._f * 60._f * 12._f));

        // Create our custom progress logger
        triggers.pushBack(makeAuto<ProgressLogger>());
    }

    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

int main() {
    Hde simulation;
    simulation.setUp();
    simulation.run();
    return 0;
}
