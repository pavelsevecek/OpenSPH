#include "gui/objects/Colorizer.h"

NAMESPACE_SPH_BEGIN

DirectionColorizer::DirectionColorizer(const Vector& axis, const Palette& palette)
    : palette(palette)
    , axis(axis) {
    SPH_ASSERT(almostEqual(getLength(axis), 1._f));
    // compute 2 perpendicular directions
    Vector ref;
    if (almostEqual(axis, Vector(0._f, 0._f, 1._f)) || almostEqual(axis, Vector(0._f, 0._f, -1._f))) {
        ref = Vector(0._f, 1._f, 0._f);
    } else {
        ref = Vector(0._f, 0._f, 1._f);
    }
    dir1 = getNormalized(cross(axis, ref));
    dir2 = cross(axis, dir1);
    SPH_ASSERT(almostEqual(getLength(dir2), 1._f));
}

Optional<float> DirectionColorizer::evalScalar(const Size idx) const {
    SPH_ASSERT(this->isInitialized());
    const Vector projected = values[idx] - dot(values[idx], axis) * axis;
    const Float x = dot(projected, dir1);
    const Float y = dot(projected - x * dir1, dir2);
    return float(PI + atan2(y, x));
}

static thread_local Array<NeighborRecord> neighs;

SummedDensityColorizer::SummedDensityColorizer(const RunSettings& settings, Palette palette)
    : palette(std::move(palette)) {
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<3>(settings);
}

void SummedDensityColorizer::initialize(const Storage& storage, const RefEnum ref) {
    m = makeArrayRef(storage.getValue<Float>(QuantityId::MASS), ref);
    r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);

    finder->build(SEQUENTIAL, r);
}

float SummedDensityColorizer::sum(const Size idx) const {
    finder->findAll(idx, r[idx][H] * kernel.radius(), neighs);
    Float rho = 0._f;
    for (const auto& n : neighs) {
        rho += m[n.index] * kernel.value(r[idx] - r[n.index], r[idx][H]);
    }
    return float(rho);
}

void CorotatingVelocityColorizer::initialize(const Storage& storage, const RefEnum ref) {
    r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    matIds = makeArrayRef(storage.getValue<Size>(QuantityId::MATERIAL_ID), ref);

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> rv = r;
    ArrayView<const Vector> vv = v;
    data.resize(storage.getMaterialCnt());

    for (Size i = 0; i < data.size(); ++i) {
        MaterialView mat = storage.getMaterial(i);
        const Size from = *mat.sequence().begin();
        const Size to = *mat.sequence().end();
        const Size size = to - from;
        data[i].center = Post::getCenterOfMass(m.subset(from, size), rv.subset(from, size));
        data[i].omega =
            Post::getAngularFrequency(m.subset(from, size), rv.subset(from, size), vv.subset(from, size));
    }
}

bool DamageActivationColorizer::hasData(const Storage& storage) const {
    return storage.has(QuantityId::DEVIATORIC_STRESS) && storage.has(QuantityId::PRESSURE) &&
           storage.has(QuantityId::EPS_MIN) && storage.has(QuantityId::DAMAGE);
}

void DamageActivationColorizer::initialize(const Storage& storage, const RefEnum UNUSED(ref)) {
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<const Float> eps_min = storage.getValue<Float>(QuantityId::EPS_MIN);
    ArrayView<const Float> damage = storage.getValue<Float>(QuantityId::DAMAGE);

    ratio.resize(p.size());
    /// \todo taken from ScalarGradyKippDamage, could be deduplicated
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView mat = storage.getMaterial(matId);
        const Float young = mat->getParam<Float>(BodySettingsId::YOUNG_MODULUS);

        /// \todo parallelize
        for (Size i : mat.sequence()) {
            const SymmetricTensor sigma = SymmetricTensor(s[i]) - p[i] * SymmetricTensor::identity();
            Float sig1, sig2, sig3;
            tie(sig1, sig2, sig3) = findEigenvalues(sigma);
            const Float sigMax = max(sig1, sig2, sig3);
            const Float young_red = max((1._f - pow<3>(damage[i])) * young, 1.e-20_f);
            const Float strain = sigMax / young_red;
            ratio[i] = float(strain / eps_min[i]);
        }
    }
}

BeautyColorizer::BeautyColorizer() {
    palette = Palette({ { u_0, Rgba(0.5f, 0.5f, 0.5) },
                          { u_glow, Rgba(0.5f, 0.5f, 0.5f) },
                          { u_red, Rgba(0.8f, 0.f, 0.f) },
                          { u_yellow, Rgba(1.f, 1.f, 0.6f) } },
        PaletteScale::LOGARITHMIC);
    f_glow = (log10(u_glow) - log10(u_0)) / (log10(u_yellow) - log10(u_0));
}

BoundaryColorizer::BoundaryColorizer(const Detection detection, const Float threshold)
    : detection(detection) {
    if (detection == Detection::NEIGBOUR_THRESHOLD) {
        neighbors.threshold = Size(threshold);
    } else {
        normals.threshold = threshold;
    }
}

bool BoundaryColorizer::hasData(const Storage& storage) const {
    if (detection == Detection::NORMAL_BASED) {
        return storage.has(QuantityId::SURFACE_NORMAL);
    } else {
        return storage.has(QuantityId::NEIGHBOR_CNT);
    }
}

void BoundaryColorizer::initialize(const Storage& storage, const RefEnum ref) {
    if (detection == Detection::NORMAL_BASED) {
        normals.values = makeArrayRef(storage.getValue<Vector>(QuantityId::SURFACE_NORMAL), ref);
    } else {
        neighbors.values = makeArrayRef(storage.getValue<Size>(QuantityId::NEIGHBOR_CNT), ref);
    }
}

bool BoundaryColorizer::isInitialized() const {
    return (detection == Detection::NORMAL_BASED && !normals.values.empty()) ||
           (detection == Detection::NEIGBOUR_THRESHOLD && !neighbors.values.empty());
}

Rgba BoundaryColorizer::evalColor(const Size idx) const {
    if (isBoundary(idx)) {
        return Rgba::red();
    } else {
        return Rgba::gray();
    }
}

bool BoundaryColorizer::isBoundary(const Size idx) const {
    switch (detection) {
    case Detection::NEIGBOUR_THRESHOLD:
        SPH_ASSERT(!neighbors.values.empty());
        return neighbors.values[idx] < neighbors.threshold;
    case Detection::NORMAL_BASED:
        SPH_ASSERT(!normals.values.empty());
        return getLength(normals.values[idx]) > normals.threshold;
    default:
        NOT_IMPLEMENTED;
    }
}


/// \todo possibly move elsewhere
static uint64_t getHash(const uint64_t value, const Size seed) {
    // https://stackoverflow.com/questions/8317508/hash-function-for-a-string
    constexpr int A = 54059;
    constexpr int B = 76963;
    constexpr int FIRST = 37;

    uint64_t hash = FIRST + seed;
    StaticArray<uint8_t, sizeof(uint64_t)> data;
    std::memcpy(&data[0], &value, data.size());
    for (uint i = 0; i < sizeof(uint64_t); ++i) {
        hash = (hash * A) ^ (data[i] * B);
    }
    return hash;
}

static Rgba getRandomizedColor(const Size idx, const Size seed = 0) {
    const uint64_t hash = getHash(idx, seed);
    const uint8_t r = (hash & 0x00000000FFFF);
    const uint8_t g = (hash & 0x0000FFFF0000) >> 16;
    const uint8_t b = (hash & 0xFFFF00000000) >> 32;
    return Rgba(r / 255.f, g / 255.f, b / 255.f);
}

template <typename TDerived>
Rgba IdColorizerTemplate<TDerived>::evalColor(const Size idx) const {
    const Optional<Size> id = static_cast<const TDerived*>(this)->evalId(idx);
    if (!id) {
        return Rgba::gray();
    }
    const Rgba color = getRandomizedColor(id.value(), seed);
    return color;
}

template <typename TDerived>
Optional<Particle> IdColorizerTemplate<TDerived>::getParticle(const Size idx) const {
    Particle particle(idx);
    const Optional<Size> id = static_cast<const TDerived*>(this)->evalId(idx);
    if (id) {
        particle.addValue(QuantityId::FLAG, id.value());
    }
    return particle;
}

template class IdColorizerTemplate<ParticleIdColorizer>;
template class IdColorizerTemplate<ComponentIdColorizer>;
template class IdColorizerTemplate<AggregateIdColorizer>;
template class IdColorizerTemplate<IndexColorizer>;

void ParticleIdColorizer::initialize(const Storage& storage, const RefEnum ref) {
    if (storage.has(QuantityId::PERSISTENT_INDEX)) {
        persistentIdxs = makeArrayRef(storage.getValue<Size>(QuantityId::PERSISTENT_INDEX), ref);
    }
}

Optional<Particle> ParticleIdColorizer::getParticle(const Size idx) const {
    Particle particle(idx);
    particle.addValue(QuantityId::FLAG, idx);
    if (!persistentIdxs.empty() && idx < persistentIdxs.size()) {
        particle.addValue(QuantityId::PERSISTENT_INDEX, persistentIdxs[idx]);
    }
    return particle;
}


ComponentIdColorizer::ComponentIdColorizer(const GuiSettings& gui,
    const Flags<Post::ComponentFlag> connectivity,
    const Optional<Size> highlightIdx)
    : IdColorizerTemplate<ComponentIdColorizer>(gui)
    , connectivity(connectivity)
    , highlightIdx(highlightIdx) {}

void ComponentIdColorizer::setHighlightIdx(const Optional<Size> newHighlightIdx) {
    if (newHighlightIdx) {
        highlightIdx = min(newHighlightIdx.value(), components.size() - 1);
    } else {
        highlightIdx = NOTHING;
    }
}

Rgba ComponentIdColorizer::evalColor(const Size idx) const {
    if (highlightIdx) {
        if (highlightIdx.value() == components[idx]) {
            return Rgba(1.f, 0.65f, 0.f);
        } else {
            return Rgba::gray(0.3f);
        }
    } else {
        return IdColorizerTemplate<ComponentIdColorizer>::evalColor(idx);
    }
}

Optional<Particle> ComponentIdColorizer::getParticle(const Size idx) const {
    Particle particle(idx);
    const Optional<Size> id = this->evalId(idx);
    particle.addValue(QuantityId::FLAG, id.value());

    Array<Size> indices;
    for (Size i = 0; i < r.size(); ++i) {
        if (components[i] == id.value()) {
            indices.push(i);
        }
    }
    if (indices.size() > 1) {
        const Vector omega = Post::getAngularFrequency(m, r, v, indices);
        particle.addValue(QuantityId::ANGULAR_FREQUENCY, getLength(omega));
    }
    return particle;
}

bool ComponentIdColorizer::hasData(const Storage& storage) const {
    return hasVelocity(storage);
}

void ComponentIdColorizer::initialize(const Storage& storage, const RefEnum ref) {
    const Array<Vector>& current = storage.getValue<Vector>(QuantityId::POSITION);
    if (current == cached.r) {
        // optimization, very poorly done
        return;
    }

    m = makeArrayRef(storage.getValue<Float>(QuantityId::MASS), ref);
    r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);

    cached.r = current.clone();

    Post::findComponents(storage, 2._f, connectivity, components);
}

std::string ComponentIdColorizer::name() const {
    if (connectivity.has(Post::ComponentFlag::ESCAPE_VELOCITY)) {
        return "Bound component ID";
    } else if (connectivity.has(Post::ComponentFlag::SEPARATE_BY_FLAG)) {
        return "Component ID (flag)";
    } else {
        return "Component ID";
    }
}

void MaterialColorizer::initialize(const Storage& storage, const RefEnum ref) {
    IndexColorizer::initialize(storage, ref);

    const Size matCnt = storage.getMaterialCnt();
    eosNames.resize(matCnt);
    rheoNames.resize(matCnt);
    for (Size matId = 0; matId < matCnt; ++matId) {
        const IMaterial& mat = storage.getMaterial(matId);
        const EosEnum eos = mat.getParam<EosEnum>(BodySettingsId::EOS);
        const YieldingEnum yield = mat.getParam<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
        eosNames[matId] = EnumMap::toString(eos);
        rheoNames[matId] = EnumMap::toString(yield);
    }
}

Optional<Particle> MaterialColorizer::getParticle(const Size idx) const {
    Particle particle(idx);
    const Optional<Size> id = IndexColorizer::evalId(idx);
    if (id) {
        particle.addValue(QuantityId::MATERIAL_ID, id.value());
        particle.addParameter(BodySettingsId::EOS, eosNames[id.value()]);
        particle.addParameter(BodySettingsId::RHEOLOGY_YIELDING, rheoNames[id.value()]);
    }
    return particle;
}

NAMESPACE_SPH_END
