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

        Array<Float> histogram(binCount);
        histogram.fill(0._f);

        const Float annulus = (maxDist - minDist) / binCount;
        for (Size i = 0; i < m.size(); ++i) {
            const Vector p = r[i] - planet.position;
            const Float dist = sqrt(sqr(p[0]) + sqr(p[1]));
            const Size bin =
                clamp(Size(round((dist - minDist) / (maxDist - minDist) * binCount)), 0u, binCount - 1);
            histogram[bin] += m[i] / (2 * PI * dist * annulus);
        }

        std::ofstream ofs(outputFile.native());
        ofs << "# Radial distance [Roche radius] vs. Surface density [Earth mass / Roche radius^2]\n";
        for (Size b = 0; b < binCount; ++b) {
            const Float p = minDist + (maxDist - minDist) * Float(b) / (binCount - 1);
            ofs << p / R_Roche << "    " << histogram[b] * sqr(R_Roche) / Constants::M_earth << '\n';
        }
        std::cout << "Histogram saved to file " << outputFile << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Cannot run program. " << e.what() << std::endl;
    }


    return 0;
}
