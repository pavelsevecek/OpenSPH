#include "Sph.h"
#include "quantities/Attractor.h"

#include <fstream>
#include <iostream>

using namespace Sph;

static Array<ArgDesc> params{
    { "i", "input", ArgEnum::STRING, "Input file (i.e. 'collision_0000.ssf')." },
    { "o", "output", ArgEnum::STRING, "Output file (i.e. 'plot_data.txt')." },
    { "r1", "minRadius", ArgEnum::FLOAT, "Minimum radial distance (in km)." },
    { "r2", "maxRadius", ArgEnum::FLOAT, "Maximum radial distance (in km)." },
    { "b", "binCount", ArgEnum::INT, "Number of histogram bins." },
    { "roche", "RocheRadius", ArgEnum::FLOAT, "Roche radius used for the final plot (in km)." },
};

int main(int argc, char* argv[]) {
    try {
        ArgParser parser(params);
        parser.parse(argc, argv);

        Path outputFile = Path(parser.getArg<String>("o"));
        Path inputFile = Path(parser.getArg<String>("i"));
        Float minDist = parser.tryGetArg<Float>("r1").valueOr(0._f) * 1000;
        Float maxDist = parser.tryGetArg<Float>("r2").valueOr(35000._f) * 1000;
        Size binCount = parser.tryGetArg<int>("b").valueOr(100);

        if (!FileSystem::pathExists(inputFile)) {
            throw Exception("Input file not found");
        }

        AutoPtr<IInput> input = Factory::getInput(inputFile);
        std::cout << "Reading file '" << inputFile << "'" << std::endl;
        Storage storage;
        Statistics stats;
        Outcome result = input->load(inputFile, storage, stats);
        if (!result) {
            throw Exception("Failed to read the file. " + result.error());
        }

        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        Attractor planet = storage.getAttractors()[0];

        Float R_Roche = 0;
        if (auto inputRoche = parser.tryGetArg<Float>("roche")) {
            R_Roche = inputRoche.value() * 1000;
        } else {
            const Float planetDensity = planet.mass / sphereVolume(planet.radius);
            const Float particleDensity = m[0] / sphereVolume(2 * r[0][H]);
            R_Roche = planet.radius * std::cbrt(2 * planetDensity / particleDensity);
            std::cout << "Using Roche radius of " << R_Roche / 1000 << "km" << std::endl;
        }

        Array<Float> surfaceDensity(binCount);
        Array<Float> velocityDispersion(binCount);
        Array<Size> counts(binCount);
        surfaceDensity.fill(0._f);
        velocityDispersion.fill(0._f);
        counts.fill(0);

        const Float annulus = (maxDist - minDist) / binCount;
        for (Size i = 0; i < m.size(); ++i) {
            const Vector position = r[i] - planet.position;
            const Vector velocity = v[i] - planet.velocity;
            const Float dist = sqrt(sqr(position[0]) + sqr(position[1]));
            const Size bin =
                clamp(Size(round((dist - minDist) / (maxDist - minDist) * binCount)), 0u, binCount - 1);
            surfaceDensity[bin] += m[i] / (2 * PI * dist * annulus);

            const Float v_rad = dot(velocity, getNormalized(position));
            velocityDispersion[bin] += sqr(v_rad);
            counts[bin]++;
        }

        std::ofstream ofs(outputFile.native());
        ofs << "# Radial distance [Roche radius]   Surface density [Earth mass / Roche radius^2]   Toomre "
               "Q\n";
        for (Size b = 0; b < binCount; ++b) {
            const Float dist = minDist + (maxDist - minDist) * Float(b) / (binCount - 1);
            const Float sigma = surfaceDensity[b];
            Float Q = 0;
            if (counts[b] > 0) {
                const Float Omega_kepl = sqrt(Constants::gravity * planet.mass / pow<3>(dist));
                const Float v_r = sqrt(velocityDispersion[b] / counts[b]);
                Q = v_r * Omega_kepl / (PI * Constants::gravity * sigma);
            }

            ofs << dist / R_Roche << "    " << sigma * sqr(R_Roche) / Constants::M_earth << "    " << Q
                << '\n';
        }
        std::cout << "Histograms saved to file " << outputFile << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Cannot run program. " << e.what() << std::endl;
    }


    return 0;
}
