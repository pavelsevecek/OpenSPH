#include "geometry/Domain.h"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "system/Profiler.h"

using namespace Sph;

int main() {
    Storage sph5;
    sph5.insert<Vector, OrderEnum::SECOND_ORDER>(QuantityIds::POSITIONS, Array<Vector>{});
    sph5.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::DAMAGE, 0._f);
    sph5.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY, 0._f);
    sph5.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::PRESSURE, 0._f);
    sph5.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::SOUND_SPEED, 0._f);
    sph5.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::HEATING, 0._f);

    TextOutput dump("%d", "", EMPTY_FLAGS);
    Statistics stats;
    dump.add(std::make_unique<ParticleNumberColumn>());
    dump.add(std::make_unique<TimeColumn>(&stats));
    dump.add(std::make_unique<SmoothingLengthColumn>());
    dump.add(std::make_unique<ValueColumn<Vector>>(QuantityIds::POSITIONS));
    dump.add(std::make_unique<DerivativeColumn<Vector>>(QuantityIds::POSITIONS));
    dump.add(std::make_unique<ValueColumn<Float>>(QuantityIds::DAMAGE));
    dump.add(std::make_unique<ValueColumn<Float>>(QuantityIds::ENERGY));
    dump.add(std::make_unique<ValueColumn<Float>>(QuantityIds::PRESSURE));
    dump.add(std::make_unique<ValueColumn<Float>>(QuantityIds::SOUND_SPEED));
    dump.add(std::make_unique<ValueColumn<Float>>(QuantityIds::HEATING));

    dump.load("", sph5);


    return 0;
}
