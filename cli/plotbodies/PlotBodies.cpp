/// \brief Tracks body mass over time binary

#include "Sph.h"
#include "io/Table.h"
#include "post/Analysis.h"
#include "post/TwoBody.h"
#include "quantities/Attractor.h"
#include <fstream>
#include <iostream>
#include <numeric>

using namespace Sph;

static Array<ArgDesc> params{
    { "f", "filemask", ArgEnum::STRING, "Mask for the input files (i.e. 'collision_%d.ssf')." },
    { "o", "output", ArgEnum::STRING, "Path for the output file (i.e. 'plot_data.txt')." },
    { "n", "number", ArgEnum::INT, "Number of bodies to plot." },
    { "e",
        "elements",
        ArgEnum::BOOL,
        "Compute orbital elements of bodies (semi-major axis and eccentricity)." },
    { "c",
        "components",
        ArgEnum::BOOL,
        "Whether the bodies are components (groups of overlapping particles, for SPH solver), or isolated "
        "particles (for hard-sphere solver with merging). Defaults to true." },
};

int main(int argc, char* argv[]) {
    try {
        ArgParser parser(params);
        parser.parse(argc, argv);

        Size outputCount = parser.getArg<int>("n");
        String outputFile = parser.getArg<String>("o");
        String filemask = parser.getArg<String>("f");
        bool doComponents = parser.tryGetArg<bool>("c").valueOr(true);
        bool doElements = parser.tryGetArg<bool>("e").valueOr(false);
        OutputFile mask = OutputFile(Path(filemask));
        Statistics stats;
        Table table(5);
        table.setCell(0, 0, "# Time [s]");
        for (Size c = 0; c < outputCount; ++c) {
            table.setCell(c + 1, 0, "# Mass " + toString(c + 1) + "[kg]");

            if (doElements) {
                table.setCell(outputCount + c + 1, 0, "# SMA " + toString(c + 1) + "[m]");
                table.setCell(2 * outputCount + c + 1, 0, "# Eccentricity " + toString(c + 1) + "[]");
            }
        }

        Size tableRow = 1;
        while (true) {
            Path path = mask.getNextPath(stats);
            if (!FileSystem::pathExists(path)) {
                break;
            }
            AutoPtr<IInput> input = Factory::getInput(path);
            std::cout << "Reading file '" << path.string() << "'" << std::endl;
            Storage storage;
            Outcome result = input->load(path, storage, stats);
            if (!result) {
                std::cout << "Failed to read the file. " << result.error() << std::endl;
            }
            Float time = stats.get<Float>(StatisticsId::RUN_TIME);
            std::cout << "Analyzing simulation at time t=" << time << " ..." << std::endl;
            table.setCell(0, tableRow, toString(time));

            if (doComponents) {
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
                ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
                Array<Size> indices;
                Size componentCount = Post::findComponents(
                    storage, 2.f, Post::ComponentFlag::OVERLAP | Post::ComponentFlag::SORT_BY_MASS, indices);

                for (Size c = 0; c < outputCount; ++c) {
                    Float totalMass = 0;
                    Vector position = Vector(0);
                    Vector velocity = Vector(0);
                    for (Size i = 0; i < indices.size(); ++i) {
                        if (indices[i] == c) {
                            totalMass += m[i];
                            position += m[i] * r[i];
                            velocity += m[i] * v[i];
                        }
                    }
                    if (totalMass > 0) {
                        position /= totalMass;
                        velocity /= totalMass;
                    }

                    std::cout << "Body " << c << " has mass " << totalMass << std::endl;
                    table.setCell(c + 1, tableRow, toString(totalMass));

                    if (storage.getAttractorCnt() > 0 && totalMass > 0) {
                        const Attractor& a = storage.getAttractors()[0];
                        position -= a.position;
                        velocity -= a.velocity;

                        const Float M = totalMass + a.mass;
                        const Float mu = (totalMass * a.mass) / M;
                        Optional<Kepler::Elements> elements =
                            Kepler::computeOrbitalElements(M, mu, position, velocity);
                        if (elements) {
                            table.setCell(outputCount + c + 1, tableRow, toString(elements->a));
                            table.setCell(2 * outputCount + c + 1, tableRow, toString(elements->e));
                        }
                    }
                }
            } else {
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
                ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
                Array<Size> idxs(m.size());
                std::iota(idxs.begin(), idxs.end(), 0);
                std::sort(idxs.begin(), idxs.end(), [m](Size i1, Size i2) { return m[i1] > m[i2]; });

                for (Size c = 0; c < outputCount; ++c) {
                    Size i = idxs[c];
                    std::cout << "Particle " << c << " has mass " << m[i] << std::endl;
                    table.setCell(c + 1, tableRow, toString(m[i]));

                    if (doElements) {
                        Vector position = r[i];
                        Vector velocity = v[i];

                        if (storage.getAttractorCnt() > 0 && m[i] > 0) {
                            const Attractor& a = storage.getAttractors()[0];
                            position -= a.position;
                            velocity -= a.velocity;

                            const Float M = m[i] + a.mass;
                            const Float mu = (m[i] * a.mass) / M;
                            Optional<Kepler::Elements> elements =
                                Kepler::computeOrbitalElements(M, mu, position, velocity);
                            if (elements) {
                                table.setCell(outputCount + c + 1, tableRow, toString(elements->a));
                                table.setCell(2 * outputCount + c + 1, tableRow, toString(elements->e));
                            }
                        }
                    }
                }
            }
            tableRow++;
        }

        std::cout << "Writing to " << outputFile << std::endl;
        std::ofstream ofs(outputFile.toWstring());

        ofs << table.toString();

    } catch (const std::exception& e) {
        std::cout << "Cannot run program. " << e.what() << std::endl;
    }


    return 0;
}
