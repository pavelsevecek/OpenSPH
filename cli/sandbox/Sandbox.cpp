#include "Sph.h"
#include "objects/finders/UniformGrid.h"
#include "physics/Functions.h"
#include "post/Analysis.h"
#include <fstream>
#include <iostream>
#include <metis.h>

using namespace Sph;

int main(int argc, char** argv) {
    /* BinaryInput input;
     Storage storage;
     Statistics stats;
     input.load(Path(argv[1]), storage, stats);
     TotalAngularMomentum angmom;
     Float L0 = angmom.evaluate(storage)[2];

     std::ofstream ofs("angmom.txt");
     for (Size i = 1; i < argc; ++i) {
         std::cout << "Processing " << argv[i] << std::endl;
         input.load(Path(argv[i]), storage, stats);
         const Float L = angmom.evaluate(storage)[2];
         const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
         ofs << t << "  " << (L - L0) / L0 << std::endl;
     }*/

    /*std::ofstream ts("tillotson.txt");
    TillotsonEos eos(BodySettings::getDefaults());
    for (Float u = 0._f; u < 1.e7_f; u += 1.e5_f) {
        for (Float rho = 1000._f; rho < 4000._f; rho += 30) {
            Float p = eos.evaluate(rho, u)[0];
            ts << rho << "  " << u << "  " << p << "\n";
        }
        ts << "\n";
    }
    return 0;*/

    /*CompressedInput input;
    Storage storage;
    Statistics dummy;
    input.load(Path(argv[1]), storage, dummy);

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    Float m_tot = std::accumulate(m.begin(), m.end(), 0._f);

    ArrayView<const Float> damage = storage.getValue<Float>(QuantityId::DAMAGE);
    Array<Size> idxs;
    for (Size i = 0; i < damage.size(); ++i) {
        if (damage[i] == 1.) {
            idxs.push(i);
        }
    }
    storage.remove(idxs, Storage::IndicesFlag::INDICES_SORTED);

    m = storage.getValue<Float>(QuantityId::MASS);
    Array<Size> comp = Post::findLargestComponent(storage, 1._f, EMPTY_FLAGS);
    Float m_core = 0._f;
    for (Size i : comp) {
        m_core += m[i];
    }
    std::cout << "Core mass fraction = " << m_core / m_tot << std::endl;


    return 0;

    // SharedPtr<IScheduler> scheduler = Factory::getScheduler();
    std::ofstream ofs("converg.txt");*/

    /*Storage reference;
    input.load(Path(argv[argc - 1]), reference, dummy);*/


    struct Stats {
        Float core;
        Float fragment;
        Float damaged;
    };

    std::map<Size, Stats> stats;
    SharedPtr<IScheduler> scheduler = Factory::getScheduler();
    std::mutex mutex;
    parallelFor(*scheduler, 1, argc, [&](Size i) {
        std::cout << "Processing " << argv[i] << std::endl;
        Storage storage;
        Statistics dummy;
        BinaryInput input;
        if (input.getInfo(Path(argv[i]))->particleCnt > 5e5) {
            return;
        }
        input.load(Path(argv[i]), storage, dummy);
        const Size numParticles = storage.getParticleCnt();

        Array<Size> idxs;
        ArrayView<const Float> damage = storage.getValue<Float>(QuantityId::DAMAGE);
        for (Size i = 0; i < damage.size(); ++i) {
            if (damage[i] > 0.9_f) {
                idxs.push(i);
            }
        }
        storage.remove(idxs, Storage::IndicesFlag::INDICES_SORTED);
        Array<Size> comps;
        Post::findComponents(storage, 1._f, Post::ComponentFlag::SORT_BY_MASS, comps);

        Size comp1 = std::count(comps.begin(), comps.end(), 0);
        Size comp2 = std::count(comps.begin(), comps.end(), 1);

        if (comp2 < 0.05_f * numParticles) {
            Array<Vector> points;
            {
                ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
                for (Size i = 0; i < r.size(); ++i) {
                    if (comps[i] == 0) {
                        points.push(r[i]);
                    }
                }
            }

            UniformGridFinder finder;
            finder.build(SEQUENTIAL, points);

            Array<idx_t> xadj;
            Array<idx_t> adjncy;
            Array<NeighborRecord> neighs;
            for (Size i = 0; i < points.size(); ++i) {
                finder.findAll(points[i], points[i][H], neighs);
                xadj.push(adjncy.size());
                for (const auto& n : neighs) {
                    adjncy.push(n.index);
                }
            }
            xadj.push(adjncy.size());

            idx_t nvtxs = points.size();
            idx_t ncon = 1;
            idx_t nparts = 2;
            idx_t objval;
            real_t ubvec = 1.8f;
            Array<idx_t> data(points.size());
            std::cout << "Running METIS" << std::endl;
            int res = METIS_PartGraphRecursive(&nvtxs,
                &ncon,
                &xadj[0],
                &adjncy[0],
                nullptr,
                nullptr,
                nullptr,
                &nparts,
                nullptr,
                &ubvec,
                nullptr,
                &objval,
                &data[0]);
            if (res != METIS_OK) {
                throw Exception("METIS failed");
            }
            Size comp1 = std::count(data.begin(), data.end(), 0);
            Size comp2 = std::count(data.begin(), data.end(), 1);
            if (comp1 < comp2) {
                std::swap(comp1, comp2);
            }
            std::unique_lock<std::mutex> lock(mutex);
            stats[numParticles] = {
                Float(comp1) / numParticles,
                Float(comp2) / numParticles,
                Float(idxs.size()) / numParticles,
            };

        } else {
            std::unique_lock<std::mutex> lock(mutex);
            stats[numParticles] = {
                Float(comp1) / numParticles,
                Float(comp2) / numParticles,
                Float(idxs.size()) / numParticles,
            };
        }
    });

    std::ofstream ofs("converg.txt");
    for (const auto& p : stats) {
        ofs << p.first << "   " << p.second.core << " " << p.second.fragment << " " << p.second.damaged
            << "\n";
    }

    /*std::map<float, float> map;
    map[3.f] = 10;
    map[2.6f] = 14;
    map[2.2f] = 18;
    map[1.8f] = 25;
    map[1.4f] = 34;
    map[1.f] = 46;

    Float Q1 = getImpactEnergy(50.e3_f, map.at(1.8f) * 1.e3_f / 2._f, 6.e3_f);
    Float Q1_D = evalBenzAsphaugScalingLaw(100.e3_f, 2700._f);
    Float d = getImpactorRadius(381.e3_f / 2._f, 6.e3_f, Q1 / Q1_D, 2700._f);*/
    // std::cout << "Q/Q_D^* = " << Q1 / Q1_D << std::endl;
    // std::cout << "d_imp = " << 2 * d / 1.e3_f << " km " << std::endl;
    // std::cout << "d_check = " << 2 * getImpactorRadius(50.e3_f, 4.e3_f, Q1 / Q1_D, 2700._f) << std::endl;

    return 0;
}
