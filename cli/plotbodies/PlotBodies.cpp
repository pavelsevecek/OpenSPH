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
    { "ar", "axesRatii", ArgEnum::BOOL, "Compute the ratios c/b and b/a." },
    { "ram",
        "rotationalAngularMomentum",
        ArgEnum::BOOL,
        "Compute the rotational angular momentum of bodies." },
    { "moif", "momentOfInertiaFactor", ArgEnum::BOOL, "Compute the moment of inertia factor." },
    { "pm", "planetMass", ArgEnum::BOOL, "Compute the mass of the central planet." },
    { "rd", "reconstructDensity", ArgEnum::BOOL, "Recompute particle densities using kernel sum." },
    { "tp", "totalParticles", ArgEnum::BOOL, "Compute the total number of particles in the simulation." },
    { "ppb", "particlesPerBody", ArgEnum::BOOL, "Compute the number of particles of bodies." },
    { "mir",
        "massInsideRadius",
        ArgEnum::FLOAT,
        "Compute the total mass of particles inside of given radius [km] from the central planet." },
    { "mor",
        "massOutsideRadius",
        ArgEnum::FLOAT,
        "Compute the total mass of particles outside of given radius [km] from the central planet." },
    { "c",
        "components",
        ArgEnum::BOOL,
        "Whether the bodies are components (groups of overlapping particles, for SPH solver), or isolated "
        "particles (for hard-sphere solver with merging). Defaults to true." },
};

static Pair<Float> getSemiaxisRatios(const Storage& storage, ArrayView<const Size> idxs) {
    if (idxs.empty()) {
        return { 0, 0 };
    }
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const SymmetricTensor I = Post::getInertiaTensor(m, r, idxs);
    const Eigen e = eigenDecomposition(I);
    const Float A = e.values[0];
    const Float B = e.values[1];
    const Float C = e.values[2];
    const Float a = sqrt(B + C - A);
    const Float b = sqrt(A + C - B);
    const Float c = sqrt(A + B - C);
    SPH_ASSERT(a > 0._f && b > 0._f && c > 0._f, a, b, c);
    return { c / b, b / a };
}


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
        bool doAxesRatii = parser.tryGetArg<bool>("ar").valueOr(false);
        bool doRotationalAngularMomentum = parser.tryGetArg<bool>("ram").valueOr(false);
        bool doMomentOfInertia = parser.tryGetArg<bool>("moif").valueOr(false);
        bool reconstructDensity = parser.tryGetArg<bool>("rd").valueOr(true);
        bool doTotalParticles = parser.tryGetArg<bool>("tp").valueOr(false);
        bool doParticlesPerBody = parser.tryGetArg<bool>("ppb").valueOr(false);
        Optional<Float> radiusInsideKm = parser.tryGetArg<Float>("mir");
        Optional<Float> radiusOutsideKm = parser.tryGetArg<Float>("mor");

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

        Size massInsideRadiusColumn = 1;
        if (radiusInsideKm) {
            massInsideRadiusColumn = nextColumn;
            table.setCell(
                massInsideRadiusColumn, 0, "# Mass inside " + toString(radiusInsideKm.value()) + "km [kg]");
            nextColumn++;
        }

        Size massOutsideRadiusColumn = 1;
        if (radiusOutsideKm) {
            massOutsideRadiusColumn = nextColumn;
            table.setCell(massOutsideRadiusColumn,
                0,
                "# Mass outside " + toString(radiusOutsideKm.value()) + "km [kg]");
            nextColumn++;
        }

        Size particlesPerBodyColumn = 1;
        if (doParticlesPerBody) {
            particlesPerBodyColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(particlesPerBodyColumn + c, 0, "# Particles " + toString(c + 1));
            }
            nextColumn += outputCount;
        }

        Size totalParticlesColumn = 1;
        if (doTotalParticles) {
            totalParticlesColumn = nextColumn;
            table.setCell(totalParticlesColumn, 0, "# Total Particles");
            nextColumn++;
        }

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
                table.setCell(positionColumn + 3 * c + 0, 0, "# X " + toString(c + 1) + " [m]");
                table.setCell(positionColumn + 3 * c + 1, 0, "# Y " + toString(c + 1) + " [m]");
                table.setCell(positionColumn + 3 * c + 2, 0, "# Z " + toString(c + 1) + " [m]");
            }
            nextColumn += 3 * outputCount;
        }

        Size velocityColumn = 1;
        if (doPosition) {
            velocityColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(velocityColumn + 3 * c + 0, 0, "# VX " + toString(c + 1) + " [m]");
                table.setCell(velocityColumn + 3 * c + 1, 0, "# VY " + toString(c + 1) + " [m]");
                table.setCell(velocityColumn + 3 * c + 2, 0, "# VZ " + toString(c + 1) + " [m]");
            }
            nextColumn += 3 * outputCount;
        }

        Size radiusColumn = 1;
        if (doRadius) {
            radiusColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(radiusColumn + c, 0, "# Radius " + toString(c + 1) + " [m]");
            }
            radiusColumn += outputCount;
        }

        Size rotationPeriodColumn = 1;
        if (doRotationPeriod) {
            rotationPeriodColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(rotationPeriodColumn + c, 0, "# Period " + toString(c + 1) + " [s]");
            }
            nextColumn += outputCount;
        }

        Size momentOfInertiaColumn = 1;
        if (doMomentOfInertia) {
            momentOfInertiaColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(momentOfInertiaColumn + c, 0, "# MOIF " + toString(c + 1));
            }
            nextColumn += outputCount;
        }

        Size rotationalAngularMomentumColumn = 1;
        if (doRotationalAngularMomentum) {
            rotationalAngularMomentumColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(
                    rotationalAngularMomentumColumn + c, 0, "# RAM " + toString(c + 1) + " [kg m^2 s^-1]");
            }
            nextColumn += outputCount;
        }

        Size axesRatiiColumn = 1;
        if (doAxesRatii) {
            axesRatiiColumn = nextColumn;
            for (Size c = 0; c < outputCount; ++c) {
                table.setCell(axesRatiiColumn + c, 0, "# c/b " + toString(c + 1));
                table.setCell(axesRatiiColumn + outputCount + c, 0, "# b/a " + toString(c + 1));
            }
            nextColumn += 2 * outputCount;
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

            if (doTotalParticles) {
                table.setCell(totalParticlesColumn, tableRow, toString(storage.getParticleCnt()));
            }

            if (doComponents) {
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
                ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
                /* ArrayView<const Float> rho;
                if (storage.has(QuantityId::DENSITY)) {
                    rho = storage.getValue<Float>(QuantityId::DENSITY);
                }*/
                Array<Float> rho(r.size());
                if (reconstructDensity) {
                    auto finder = Factory::getFinder(RunSettings::getDefaults());
                    finder->build(SEQUENTIAL, r, FinderFlag::SKIP_RANK);
                    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());
                    auto scheduler = Factory::getScheduler(RunSettings::getDefaults());
                    Array<Float> shepardCorrection(r.size());
                    shepardCorrection.fill(1._f);
                    for (int iter = 0; iter < 5; ++iter) {
                        ThreadLocal<Array<NeighborRecord>> neighsTl(*scheduler);
                        parallelFor(
                            *scheduler, neighsTl, 0, r.size(), [&](Size i, Array<NeighborRecord>& neighs) {
                                finder->findAll(i, r[i][H] * kernel.radius(), neighs);
                                rho[i] = 0.f;
                                for (const auto& n : neighs) {
                                    Size j = n.index;
                                    rho[i] +=
                                        m[j] * shepardCorrection[i] * kernel.value(r[j] - r[i], r[j][H]);
                                }
                            });
                        parallelFor(
                            *scheduler, neighsTl, 0, r.size(), [&](Size i, Array<NeighborRecord>& neighs) {
                                finder->findAll(i, r[i][H] * kernel.radius(), neighs);
                                Float sum = 0._f;
                                for (const auto& n : neighs) {
                                    Size j = n.index;
                                    sum += m[j] / rho[j] * kernel.value(r[j] - r[i], r[j][H]);
                                }
                                shepardCorrection[i] = 1.f / sum;
                            });
                    }
                } else {
                    rho = storage.getValue<Float>(QuantityId::DENSITY).clone();
                }

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
                            totalVolume += rho.empty() ? sphereVolume(r[i][H]) : m[i] / rho[i];
                            position += m[i] * r[i];
                            velocity += m[i] * v[i];

                            bodyIdxs.push(i);
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

                    if (doParticlesPerBody) {
                        table.setCell(particlesPerBodyColumn + c, tableRow, toString(bodyIdxs.size()));
                    }

                    std::cout << "Body " << c << " has mass " << totalMass << " and " << bodyIdxs.size()
                              << " particles." << std::endl;
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

                    Float radius = volumeEquivalentRadius(totalVolume);
                    if (doRadius) {
                        table.setCell(radiusColumn + c, tableRow, toString(radius));
                    }

                    if (doMomentOfInertia) {
                        Float moif = 0;
                        if (!bodyIdxs.empty()) {
                            const SymmetricTensor I = Post::getInertiaTensor(m, r, bodyIdxs);
                            const Eigen e = eigenDecomposition(I);
                            moif = maxElement(e.values) / (totalMass * sqr(radius));
                        }
                        table.setCell(momentOfInertiaColumn + c, tableRow, toString(moif));
                    }


                    if (doRotationalAngularMomentum) {
                        Vector L = Vector(0);
                        for (Size i : bodyIdxs) {
                            L += m[i] * (cross(r[i] - position, v[i] - velocity));
                        }
                        table.setCell(rotationalAngularMomentumColumn + c, tableRow, toString(getLength(L)));
                    }

                    if (doAxesRatii) {
                        Pair<Float> ratii = getSemiaxisRatios(storage, bodyIdxs);
                        table.setCell(axesRatiiColumn + c, tableRow, toString(ratii[0]));
                        table.setCell(axesRatiiColumn + outputCount + c, tableRow, toString(ratii[1]));
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
                Vector r0 = Vector(0);
                Vector v0 = Vector(0);
                if (storage.getAttractorCnt() > 0) {
                    const Attractor& a = storage.getAttractors()[0];
                    r0 = a.position;
                    v0 = a.velocity;
                }
                Vector total(0.);
                ArrayView<const Vector> r, v, dv;
                tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                for (Size i = 0; i < v.size(); ++i) {
                    total += m[i] * cross(r[i] - r0, v[i] - v0);
                }

                const double value = getLength(total);
                table.setCell(amColumn + outputCount, tableRow, toString(value));
            }

            if (doPlanetMass && storage.getAttractorCnt() > 0) {
                const Attractor& a = storage.getAttractors()[0];
                table.setCell(pmColumn, tableRow, toString(a.mass));
            }

            if ((radiusInsideKm || radiusOutsideKm) && storage.getAttractorCnt() > 0) {
                const Attractor& a = storage.getAttractors()[0];
                ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
                ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

                Float massInside = 0._f;
                Float massOutside = 0._f;

                for (Size i = 0; i < r.size(); ++i) {
                    const Float dist = getLength(r[i] - a.position);
                    if (radiusInsideKm && dist < radiusInsideKm.value() * 1e3) {
                        massInside += m[i];
                    }
                    if (radiusOutsideKm && dist > radiusOutsideKm.value() * 1e3) {
                        massOutside += m[i];
                    }
                }

                if (radiusInsideKm) {
                    table.setCell(massInsideRadiusColumn, tableRow, toString(massInside));
                }
                if (radiusOutsideKm) {
                    table.setCell(massOutsideRadiusColumn, tableRow, toString(massOutside));
                }
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
