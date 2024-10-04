/// \brief Tracks body mass over time binary

#include "Sph.h"
#include "io/Table.h"
#include "post/Analysis.h"
#include <fstream>
#include <iostream>

using namespace Sph;

static Array<ArgDesc> params{
    { "f", "filemask", ArgEnum::STRING, "Mask for the input files (i.e. 'collision_%d.ssf')." },
    { "o", "output", ArgEnum::STRING, "Path for the output file (i.e. 'plot_data.txt')." },
    { "n", "number", ArgEnum::INT, "Number of bodies to plot." },
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
        OutputFile mask = OutputFile(Path(filemask));
        Statistics stats;
        Table table(5);
        table.setCell(0, 0, "# Time [s]");
        for (Size c = 0; c < outputCount; ++c) {
            table.setCell(c + 1, 0, "# Mass " + toString(c + 1) + "[kg]");
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
                Array<Size> indices;
                Size componentCount = Post::findComponents(
                    storage, 2.f, Post::ComponentFlag::OVERLAP | Post::ComponentFlag::SORT_BY_MASS, indices);

                for (Size c = 0; c < outputCount; ++c) {
                    Float totalMass = 0;
                    for (Size i = 0; i < indices.size(); ++i) {
                        if (indices[i] == c) {
                            totalMass += m[i];
                        }
                    }
                    std::cout << "Body " << c << " has mass " << totalMass << std::endl;
                    table.setCell(c + 1, tableRow, toString(totalMass));
                }
            } else {
                Array<Float> m = storage.getValue<Float>(QuantityId::MASS).clone();
                std::sort(m.begin(), m.end(), std::greater<Float>());

                for (Size c = 0; c < outputCount; ++c) {
                    std::cout << "Particle " << c << " has mass " << m[c] << std::endl;
                    table.setCell(c + 1, tableRow, toString(m[c]));
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
