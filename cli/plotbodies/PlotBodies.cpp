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
    { "am", "angularMomentum", ArgEnum::BOOL, "Compute the angular momentum of bodies." },
    { "p", "position", ArgEnum::BOOL, "Compute the positions (x,y,z) of bodies." },
    { "v", "velocity", ArgEnum::BOOL, "Compute the velocities (x,y,z) of bodies." },
    { "r", "radius", ArgEnum::BOOL, "Compute the radii of bodies." },
    { "rp", "rotationPeriod", ArgEnum::BOOL, "Compute the rotation period of bodies." },
    { "pm", "planetMass", ArgEnum::BOOL, "Compute the mass of the central planet." },
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
        bool doAngularMomentum = parser.tryGetArg<bool>("am").valueOr(false);
        bool doPlanetMass = parser.tryGetArg<bool>("pm").valueOr(false);
        bool doPosition = parser.tryGetArg<bool>("p").valueOr(false);
        bool doVelocity = parser.tryGetArg<bool>("v").valueOr(false);
        bool doRadius = parser.tryGetArg<bool>("r").valueOr(false);
        bool doRotationPeriod = parser.tryGetArg<bool>("rp").valueOr(false);

        OutputFile mask = OutputFile(Path(filemask));
        Statistics stats;
        Table table(5);
        table.setCell(0, 0, "# Time [s]");
        Size nextColumn = 1;
        Size massColumn = 1;
        for (Size c = 0; c < outputCount; ++c) {
            table.setCell(c + 1, 0, "# Mass " + toString(c + 1) + " [kg]");
        }
        nextColumn += outputCount;

        Size smaColumn = 1;
        Size eccentricityColumn = 1;
        if (doElements) {
            smaColumn = nextColumn;
            eccentricityColumn = nextColumn + outputCount;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(smaColumn + c, 0, "# SMA " + toString(c + 1) + " [m]");
                table.setCell(eccentricityColumn + c, 0, "# Eccentricity " + toString(c + 1) + " []");
            }
            nextColumn += 2 * outputCount;
        }

        Size pmColumn = 1;
        if (doPlanetMass) {
            pmColumn = nextColumn;
            table.setCell(pmColumn, 0, "# Planet mass [kg]");
            nextColumn++;
        }

        Size amColumn = 1;
        if (doAngularMomentum) {
            amColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(amColumn + c, 0, "# AM " + toString(c + 1) + "[kg m^2 s^-1]");
            }
            table.setCell(amColumn + outputCount, 0, "# AM [kg m^2 s^-1]");
            nextColumn += outputCount + 1;
        }

        Size positionColumn = 1;
        if (doPosition) {
            positionColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(positionColumn + 3 * c + 0, 0, "# X [m]");
                table.setCell(positionColumn + 3 * c + 1, 0, "# Y [m]");
                table.setCell(positionColumn + 3 * c + 2, 0, "# Z [m]");
            }
            nextColumn += 3 * outputCount;
        }

        Size velocityColumn = 1;
        if (doPosition) {
            velocityColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(velocityColumn + 3 * c + 0, 0, "# VX [m]");
                table.setCell(velocityColumn + 3 * c + 1, 0, "# VY [m]");
                table.setCell(velocityColumn + 3 * c + 2, 0, "# VZ [m]");
            }
            nextColumn += 3 * outputCount;
        }

        Size radiusColumn = 1;
        if (doRadius) {
            radiusColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(radiusColumn + c, 0, "# Radius [m]");
            }
            radiusColumn += outputCount;
        }

        Size rotationPeriodColumn = 1;
        if (doRotationPeriod) {
            rotationPeriodColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(rotationPeriodColumn + c, 0, "# Period [s]");
            }
            nextColumn += outputCount;
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
                Array<Size> bodyIdxs;
                for (Size c = 0; c < outputCount; ++c) {
                    Float totalMass = 0;
                    Float totalVolume = 0;
                    Vector position = Vector(0);
                    Vector velocity = Vector(0);
                    bodyIdxs.clear();
                    for (Size i = 0; i < indices.size(); ++i) {
                        if (indices[i] == c) {
                            totalMass += m[i];
                            totalVolume += sphereVolume(r[i][H]);
                            position += m[i] * r[i];
                            velocity += m[i] * v[i];

                            if (doRotationPeriod) {
                                bodyIdxs.push(i);
                            }
                        }
                    }
                    if (totalMass > 0) {
                        position /= totalMass;
                        velocity /= totalMass;
                    }

                    if (doRotationPeriod) {
                        Vector omega = Post::getAngularFrequency(m, r, v, position, velocity, bodyIdxs);
                        Float period = getLength(omega) > EPS ? 2 * PI / getLength(omega) : 0;
                        table.setCell(rotationPeriodColumn + c, tableRow, toString(period));
                    }

                    std::cout << "Body " << c << " has mass " << totalMass << std::endl;
                    table.setCell(massColumn + c, tableRow, toString(totalMass));

                    if (doPosition) {
                        table.setCell(positionColumn + 3 * c + 0, tableRow, toString(position[0]));
                        table.setCell(positionColumn + 3 * c + 1, tableRow, toString(position[1]));
                        table.setCell(positionColumn + 3 * c + 2, tableRow, toString(position[2]));
                    }

                    if (doVelocity) {
                        table.setCell(velocityColumn + 3 * c + 0, tableRow, toString(velocity[0]));
                        table.setCell(velocityColumn + 3 * c + 1, tableRow, toString(velocity[1]));
                        table.setCell(velocityColumn + 3 * c + 2, tableRow, toString(velocity[2]));
                    }

                    if (doRadius) {
                        table.setCell(
                            radiusColumn + c, tableRow, toString(volumeEquivalentRadius(totalVolume)));
                    }

                    if (storage.getAttractorCnt() > 0 && totalMass > 0) {
                        const Attractor& a = storage.getAttractors()[0];
                        position -= a.position;
                        velocity -= a.velocity;

                        if (doElements) {
                            const Float M = totalMass + a.mass;
                            const Float mu = (totalMass * a.mass) / M;
                            Optional<Kepler::Elements> elements =
                                Kepler::computeOrbitalElements(M, mu, position, velocity);
                            if (elements) {
                                table.setCell(smaColumn + c, tableRow, toString(elements->a));
                                table.setCell(eccentricityColumn + c, tableRow, toString(elements->e));
                            }
                        }

                        if (doAngularMomentum) {
                            const Float am = totalMass * norm(cross(position, velocity));
                            table.setCell(amColumn + c, tableRow, toString(am));
                        }
                    }
                }
            } else {
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
                ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
                ArrayView<const Vector> omega;
                if (storage.has(QuantityId::ANGULAR_FREQUENCY)) {
                    omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
                }

                Array<Size> idxs(m.size());
                std::iota(idxs.begin(), idxs.end(), 0);
                std::sort(idxs.begin(), idxs.end(), [m](Size i1, Size i2) { return m[i1] > m[i2]; });

                for (Size c = 0; c < outputCount; ++c) {
                    Size i = idxs[c];
                    std::cout << "Particle " << c << " has mass " << m[i] << std::endl;
                    table.setCell(massColumn + c, tableRow, toString(m[i]));

                    Vector position = r[i];
                    Vector velocity = v[i];

                    if (doPosition) {
                        table.setCell(positionColumn + 3 * c + 0, tableRow, toString(position[0]));
                        table.setCell(positionColumn + 3 * c + 1, tableRow, toString(position[1]));
                        table.setCell(positionColumn + 3 * c + 2, tableRow, toString(position[2]));
                    }

                    if (doVelocity) {
                        table.setCell(velocityColumn + 3 * c + 0, tableRow, toString(velocity[0]));
                        table.setCell(velocityColumn + 3 * c + 1, tableRow, toString(velocity[1]));
                        table.setCell(velocityColumn + 3 * c + 2, tableRow, toString(velocity[2]));
                    }

                    if (doRadius) {
                        table.setCell(radiusColumn + c, tableRow, toString(r[i][H]));
                    }

                    if (doRotationPeriod) {
                        Float period =
                            !omega.empty() && getLength(omega[i]) > EPS ? 2 * PI / getLength(omega[i]) : 0;
                        table.setCell(rotationPeriodColumn + c, tableRow, toString(period));
                    }

                    if (storage.getAttractorCnt() > 0 && m[i] > 0) {
                        const Attractor& a = storage.getAttractors()[0];
                        position -= a.position;
                        velocity -= a.velocity;

                        if (doElements) {
                            const Float M = m[i] + a.mass;
                            const Float mu = (m[i] * a.mass) / M;
                            Optional<Kepler::Elements> elements =
                                Kepler::computeOrbitalElements(M, mu, position, velocity);
                            if (elements) {
                                table.setCell(smaColumn + c, tableRow, toString(elements->a));
                                table.setCell(eccentricityColumn + c, tableRow, toString(elements->e));
                            }
                        }

                        if (doAngularMomentum) {
                            const Float am = m[i] * getLength(cross(position, velocity));
                            table.setCell(amColumn + c, tableRow, toString(am));
                        }
                    }
                }
            }

            if (doAngularMomentum) {
                TotalAngularMomentum am;
                const double value = getLength(am.evaluate(storage));
                table.setCell(amColumn + outputCount, tableRow, toString(value));
            }

            if (doPlanetMass && storage.getAttractorCnt() > 0) {
                const Attractor& a = storage.getAttractors()[0];
                table.setCell(pmColumn, tableRow, toString(a.mass));
            }

            tableRow++;
        }

        std::cout << "Writing to " << outputFile << std::endl;
        std::ofstream ofs(Path(outputFile).native());

        ofs << table.toString();

    } catch (const std::exception& e) {
        std::cout << "Cannot run program. " << e.what() << std::endl;
    }


    return 0;
}
