#include "run/ScriptUtils.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_CHAISCRIPT

Chai::Particles::Particles(const Size particleCnt) {
    Array<Vector> r(particleCnt);
    r.fill(Vector(0._f, 0._f, 0._f, EPS));
    storage = alignedNew<Storage>();
    storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));
    owns = true;

    this->resize(particleCnt);
}

Chai::Particles::~Particles() {
    if (owns) {
        alignedDelete(storage);
    }
}

void Chai::Particles::bindToStorage(Storage& input) {
    if (owns) {
        alignedDelete(storage);
    }
    storage = &input;
    owns = false;
    this->resize(storage->getParticleCnt());

    /// \todo lazy init!
    if (storage->has(QuantityId::POSITION)) {
        ArrayView<const Vector> r, v, dv;
        tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
        std::copy(r.begin(), r.end(), positions.begin());
        std::copy(v.begin(), v.end(), velocities.begin());
        std::copy(dv.begin(), dv.end(), accelerations.begin());
        for (Size i = 0; i < r.size(); ++i) {
            radii[i] = r[i][H];
        }
    }

    if (storage->has(QuantityId::MASS)) {
        ArrayView<const Float> m = storage->getValue<Float>(QuantityId::MASS);
        std::copy(m.begin(), m.end(), masses.begin());
    }
}

const Storage& Chai::Particles::store() const {
    const Size N = positions.size();
    Array<Vector> r(N), v(N), dv(N);
    Array<Float> m(N);
    for (Size i = 0; i < N; ++i) {
        r[i] = positions[i];
        r[i][H] = radii[i];
        v[i] = velocities[i];
        dv[i] = accelerations[i];
        m[i] = masses[i];
    }
    storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));
    storage->getDt<Vector>(QuantityId::POSITION) = std::move(v);
    storage->getD2t<Vector>(QuantityId::POSITION) = std::move(dv);
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(m));
    return *storage;
}

Size Chai::Particles::getParticleCnt() const {
    return storage->getParticleCnt();
}

void Chai::Particles::resize(const Size particleCnt) {
    positions.resize(particleCnt);
    velocities.resize(particleCnt);
    accelerations.resize(particleCnt);
    masses.resize(particleCnt);
    radii.resize(particleCnt);
}

std::vector<Float>& Chai::Particles::getMasses() {
    /// \todo lazy init!
    return masses;
}

std::vector<Float>& Chai::Particles::getRadii() {
    return radii;
}

std::vector<Chai::Vec3>& Chai::Particles::getPositions() {
    return positions;
}

std::vector<Chai::Vec3>& Chai::Particles::getVelocities() {
    return velocities;
}

std::vector<Chai::Vec3>& Chai::Particles::getAccelerations() {
    return accelerations;
}

Chai::Box3 Chai::Particles::getBoundingBox() const {
    Box3 box;
    for (Size i = 0; i < positions.size(); ++i) {
        box.extend(positions[i]);
    }
    return box;
}

Float Chai::Particles::getTotalMass() const {
    return std::accumulate(masses.begin(), masses.end(), 0._f);
}

Chai::Vec3 Chai::Particles::getCenterOfMass() const {
    Vec3 r_com(0, 0, 0);
    Float m = 0._f;
    for (Size i = 0; i < positions.size(); ++i) {
        r_com += positions[i] * masses[i];
        m += masses[i];
    }
    return r_com / m;
}

Chai::Vec3 Chai::Particles::getTotalMomentum() const {
    Vec3 p(0, 0, 0);
    for (Size i = 0; i < velocities.size(); ++i) {
        p += velocities[i] * masses[i];
    }
    return p;
}

Chai::Vec3 Chai::Particles::getTotalAngularMomentum() const {
    const Vec3 r0 = this->getCenterOfMass();
    Vec3 L(0, 0, 0);
    for (Size i = 0; i < velocities.size(); ++i) {
        L += cross(positions[i] - r0, velocities[i]) * masses[i];
    }
    return L;
}

Chai::Vec3 Chai::Particles::getAngularFrequency() const {
    Array<Vector> r(positions.size()), v(positions.size());
    for (Size i = 0; i < r.size(); ++i) {
        r[i] = positions[i];
        v[i] = velocities[i];
    }
    return Post::getAngularFrequency(ArrayView<const Float>(masses.data(), masses.size()), r, v);
}

void Chai::Particles::merge(Particles& other) {
    storage->merge(std::move(*other.storage));

    positions.insert(positions.end(), other.positions.begin(), other.positions.end());
    velocities.insert(velocities.end(), other.velocities.begin(), other.velocities.end());
    masses.insert(masses.end(), other.masses.begin(), other.masses.end());
    radii.insert(radii.end(), other.radii.begin(), other.radii.end());
}

void Chai::registerBindings(chaiscript::ChaiScript& chai) {
    // math functions
    chai.add(chaiscript::fun(&Sph::sqr<double>), "sqr");
    chai.add(chaiscript::fun(&Sph::sqrt<double>), "sqrt");
    chai.add(chaiscript::fun(&Sph::cos<double>), "cos");
    chai.add(chaiscript::fun(&Sph::sin<double>), "sin");
    chai.add(chaiscript::fun(&Sph::lerp<double, double>), "lerp");
    chai.add(chaiscript::fun(&Sph::abs<double>), "abs");
    chai.add(chaiscript::fun(&Sph::pow<double>), "pow");

    // vector utils
    chai.add(chaiscript::user_type<Vec3>(), "Vec3");
    chai.add(chaiscript::constructor<Vec3()>(), "Vec3");
    chai.add(chaiscript::constructor<Vec3(const Vec3&)>(), "Vec3");
    chai.add(chaiscript::constructor<Vec3(Float, Float, Float)>(), "Vec3");
    chai.add(chaiscript::fun(&Vec3::x), "x");
    chai.add(chaiscript::fun(&Vec3::y), "y");
    chai.add(chaiscript::fun(&Vec3::z), "z");
    chai.add(chaiscript::fun(&Vec3::operator+), "+");
    chai.add(chaiscript::fun(&Vec3::operator-), "-");
    chai.add(chaiscript::fun(&Vec3::operator*), "*");
    chai.add(chaiscript::fun(&Vec3::operator/), "/");
    chai.add(chaiscript::fun(&Vec3::operator=), "=");
    chai.add(chaiscript::fun(&Vec3::operator+=), "+=");
    chai.add(chaiscript::fun(&dot), "dotProd");
    chai.add(chaiscript::fun(&cross), "crossProd");
    chai.add(chaiscript::fun(&length), "length");
    chai.add(chaiscript::fun(&max), "max");
    chai.add(chaiscript::fun(&min), "min");
    chai.add(chaiscript::fun(&normalized), "normalized");

    // box utils
    chai.add(chaiscript::user_type<Box3>(), "Box3");
    chai.add(chaiscript::constructor<Box3()>(), "Box3");
    chai.add(chaiscript::constructor<Box3(const Box3&)>(), "Box3");
    chai.add(chaiscript::constructor<Box3(const Vec3&, const Vec3&)>(), "Box3");
    chai.add(chaiscript::fun(&Box3::operator=), "=");
    chai.add(chaiscript::fun(&Box3::size), "size");
    chai.add(chaiscript::fun(&Box3::extend), "extend");

    // particle quantities
    chai.add(chaiscript::user_type<Particles>(), "Particles");
    chai.add(chaiscript::constructor<Particles(Size)>(), "Particles");
    chai.add(chaiscript::fun(&Particles::getParticleCnt), "getParticleCnt");
    chai.add(chaiscript::fun(&Particles::getMasses), "getMasses");
    chai.add(chaiscript::fun(&Particles::getPositions), "getPositions");
    chai.add(chaiscript::fun(&Particles::getVelocities), "getVelocities");
    chai.add(chaiscript::fun(&Particles::getAccelerations), "getAccelerations");
    chai.add(chaiscript::fun(&Particles::getRadii), "getRadii");
    chai.add(chaiscript::fun(&Particles::merge), "merge");
    chai.add(chaiscript::fun(&Particles::getBoundingBox), "getBoundingBox");
    chai.add(chaiscript::fun(&Particles::getCenterOfMass), "getCenterOfMass");
    chai.add(chaiscript::fun(&Particles::getTotalMass), "getTotalMass");
    chai.add(chaiscript::fun(&Particles::getTotalMomentum), "getTotalMomentum");
    chai.add(chaiscript::fun(&Particles::getTotalAngularMomentum), "getTotalAngularMomentum");
    chai.add(chaiscript::fun(&Particles::getAngularFrequency), "getAngularFrequency");

    // allow using float and vector std::vector's
    chai.add(chaiscript::bootstrap::standard_library::vector_type<std::vector<Float>>("VectorFloat"));
    chai.add(chaiscript::bootstrap::standard_library::vector_type<std::vector<Vec3>>("VectorVec3"));

    // auxiliary constants
    chai.add_global_const(chaiscript::const_var(DEG_TO_RAD), "DEG_TO_RAD");
    chai.add_global_const(chaiscript::const_var(RAD_TO_DEG), "RAD_TO_DEG");
    chai.add_global_const(chaiscript::const_var(Constants::M_earth), "M_earth");
    chai.add_global_const(chaiscript::const_var(Constants::M_sun), "M_sun");
    chai.add_global_const(chaiscript::const_var(Constants::gravity), "G");
}

#endif

NAMESPACE_SPH_END
