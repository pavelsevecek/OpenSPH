#pragma once

/// \file ShearingSheet.h
/// \brief Shared helpers for local shearing-sheet dynamics

#include "objects/geometry/Box.h"
#include "objects/geometry/Indices.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "quantities/Utility.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include <cmath>

NAMESPACE_SPH_BEGIN

namespace ShearingSheet {

struct Config {
    Vector center = Vector(0._f);
    Vector boxSize = Vector(1._f);
    Indices ghosts = Indices(0);
    Float omega = 0._f;
    Float gravity = Constants::gravity;
    Float softening = 0._f;
    Float minCollisionVelocity = 0._f;
    ShearingSheetVerticalBoundaryEnum verticalBoundary = ShearingSheetVerticalBoundaryEnum::PERIODIC;
    ShearingSheetRestitutionEnum restitution = ShearingSheetRestitutionEnum::CONSTANT;

    INLINE bool enabled() const {
        return omega > 0._f;
    }

    INLINE Box box() const {
        return Box(center - 0.5_f * boxSize, center + 0.5_f * boxSize);
    }
};

struct GhostBox {
    Indices index;
    Vector positionOffset = Vector(0._f);
    Vector velocityOffset = Vector(0._f);
};

struct Diagnostics {
    Float totalMass = 0._f;
    Float surfaceDensity = 0._f;
    Float toomreWavelength = 0._f;
    Vector velocityDispersion = Vector(0._f);
};

INLINE bool isEnabled(const RunSettings& settings) {
    return settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY) == BoundaryEnum::SHEARING_SHEET;
}

INLINE bool usesSymplecticEpicycle(const RunSettings& settings) {
    return isEnabled(settings) &&
           settings.get<TimesteppingEnum>(RunSettingsId::TIMESTEPPING_INTEGRATOR) ==
               TimesteppingEnum::SYMPLECTIC_EPICYCLE;
}

INLINE Optional<Config> tryGetConfig(const RunSettings& settings) {
    if (!isEnabled(settings)) {
        return NOTHING;
    }

    Config cfg;
    cfg.center = settings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    cfg.boxSize = settings.get<Vector>(RunSettingsId::DOMAIN_SIZE);
    cfg.ghosts = Indices(settings.get<int>(RunSettingsId::SHEARING_SHEET_GHOST_X),
        settings.get<int>(RunSettingsId::SHEARING_SHEET_GHOST_Y),
        settings.get<int>(RunSettingsId::SHEARING_SHEET_GHOST_Z));
    cfg.omega = settings.get<Float>(RunSettingsId::SHEARING_SHEET_OMEGA);
    cfg.gravity = settings.get<Float>(RunSettingsId::GRAVITY_CONSTANT);
    cfg.softening = settings.get<Float>(RunSettingsId::GRAVITY_SOFTENING_LENGTH);
    cfg.minCollisionVelocity = settings.get<Float>(RunSettingsId::SHEARING_SHEET_MIN_COLLISION_VELOCITY);
    cfg.verticalBoundary =
        settings.get<ShearingSheetVerticalBoundaryEnum>(RunSettingsId::SHEARING_SHEET_VERTICAL_BOUNDARY);
    cfg.restitution = settings.get<ShearingSheetRestitutionEnum>(RunSettingsId::SHEARING_SHEET_RESTITUTION);
    return cfg;
}

INLINE Vector relativePosition(const Config& cfg, const Vector& r) {
    return clearH(r) - cfg.center;
}

INLINE Float wrapCoordinate(Float value, const Float size) {
    const Float half = 0.5_f * size;
    while (value > half) {
        value -= size;
    }
    while (value < -half) {
        value += size;
    }
    return value;
}

INLINE Float xVelocityOffset(const Config& cfg, const int ix) {
    return -1.5_f * Float(ix) * cfg.omega * cfg.boxSize[X];
}

INLINE Float xBoundaryPositionOffset(const Config& cfg, const int sign, const Float t) {
    const Float shear = 1.5_f * cfg.omega * cfg.boxSize[X] * t;
    if (sign > 0) {
        return -std::fmod(-shear + 0.5_f * cfg.boxSize[Y], cfg.boxSize[Y]) - 0.5_f * cfg.boxSize[Y];
    } else {
        return -std::fmod(shear - 0.5_f * cfg.boxSize[Y], cfg.boxSize[Y]) + 0.5_f * cfg.boxSize[Y];
    }
}

INLINE Float ghostShiftY(const Config& cfg, const int ix, const Float t) {
    const Float vy = xVelocityOffset(cfg, ix);
    if (ix == 0) {
        return -std::fmod(vy * t, cfg.boxSize[Y]);
    }
    if (ix > 0) {
        return -std::fmod(vy * t - 0.5_f * cfg.boxSize[Y], cfg.boxSize[Y]) - 0.5_f * cfg.boxSize[Y];
    }
    return -std::fmod(vy * t + 0.5_f * cfg.boxSize[Y], cfg.boxSize[Y]) + 0.5_f * cfg.boxSize[Y];
}

INLINE GhostBox getGhostBox(const Config& cfg, const Indices& index, const Float t) {
    GhostBox gb;
    gb.index = index;
    gb.positionOffset = Vector(Float(index[X]) * cfg.boxSize[X],
        Float(index[Y]) * cfg.boxSize[Y] - ghostShiftY(cfg, index[X], t),
        Float(index[Z]) * cfg.boxSize[Z]);
    gb.velocityOffset = Vector(0._f, xVelocityOffset(cfg, index[X]), 0._f);
    return gb;
}

template <typename TFunc>
INLINE void iterateGhostBoxes(const Config& cfg, const Float t, const bool innerRingOnly, TFunc&& func) {
    const int nx = innerRingOnly ? min(cfg.ghosts[X], 1) : cfg.ghosts[X];
    const int ny = innerRingOnly ? min(cfg.ghosts[Y], 1) : cfg.ghosts[Y];
    const int nzPeriodic = innerRingOnly ? min(cfg.ghosts[Z], 1) : cfg.ghosts[Z];
    const int nz = cfg.verticalBoundary == ShearingSheetVerticalBoundaryEnum::PERIODIC ? nzPeriodic : 0;

    for (int ix = -nx; ix <= nx; ++ix) {
        for (int iy = -ny; iy <= ny; ++iy) {
            for (int iz = -nz; iz <= nz; ++iz) {
                func(getGhostBox(cfg, Indices(ix, iy, iz), t));
            }
        }
    }
}

INLINE void remap(Vector& r, Vector& v, const Config& cfg, const Float t) {
    Vector pos = relativePosition(cfg, r);

    while (pos[X] > 0.5_f * cfg.boxSize[X]) {
        pos[X] -= cfg.boxSize[X];
        pos[Y] += xBoundaryPositionOffset(cfg, +1, t);
        v[Y] += 1.5_f * cfg.omega * cfg.boxSize[X];
    }
    while (pos[X] < -0.5_f * cfg.boxSize[X]) {
        pos[X] += cfg.boxSize[X];
        pos[Y] += xBoundaryPositionOffset(cfg, -1, t);
        v[Y] -= 1.5_f * cfg.omega * cfg.boxSize[X];
    }

    pos[Y] = wrapCoordinate(pos[Y], cfg.boxSize[Y]);

    switch (cfg.verticalBoundary) {
    case ShearingSheetVerticalBoundaryEnum::PERIODIC:
        pos[Z] = wrapCoordinate(pos[Z], cfg.boxSize[Z]);
        break;
    case ShearingSheetVerticalBoundaryEnum::REFLECTING:
        while (pos[Z] > 0.5_f * cfg.boxSize[Z]) {
            pos[Z] = cfg.boxSize[Z] - pos[Z];
            v[Z] *= -1._f;
        }
        while (pos[Z] < -0.5_f * cfg.boxSize[Z]) {
            pos[Z] = -cfg.boxSize[Z] - pos[Z];
            v[Z] *= -1._f;
        }
        break;
    case ShearingSheetVerticalBoundaryEnum::OPEN:
        break;
    default:
        NOT_IMPLEMENTED;
    }

    r = setH(cfg.center + pos, r[H]);
}

INLINE void remap(Storage& storage, const Config& cfg, const Float t) {
    if (!storage.has<Vector>(QuantityId::POSITION, OrderEnum::SECOND)) {
        return;
    }
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        remap(r[i], v[i], cfg, t);
    }
}

INLINE Vector hillAcceleration(const Config& cfg, const Vector& r, const Vector& v) {
    const Vector pos = relativePosition(cfg, r);
    return Vector(2._f * cfg.omega * v[Y] + 3._f * sqr(cfg.omega) * pos[X],
        -2._f * cfg.omega * v[X],
        -sqr(cfg.omega) * pos[Z]);
}

INLINE Vector perturbingAcceleration(const Config& cfg, const Vector& r, const Vector& v, const Vector& dv) {
    return dv - clearH(hillAcceleration(cfg, r, v));
}

INLINE void applyHillForces(Storage& storage, const Config& cfg) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        dv[i] += clearH(hillAcceleration(cfg, r[i], v[i]));
        dv[i][H] = 0._f;
    }
}

INLINE Float restitutionBridges(const Float speed) {
    Float eps = 0.32_f * pow(abs(speed) * 100._f, -0.234_f);
    eps = min(1._f, eps);
    eps = max(0._f, eps);
    return eps;
}

INLINE Diagnostics evaluateDiagnostics(const Storage& storage, const Config& cfg) {
    Diagnostics diagnostics;
    diagnostics.totalMass = getTotalMass(storage);
    diagnostics.surfaceDensity = diagnostics.totalMass / (cfg.boxSize[X] * cfg.boxSize[Y]);
    diagnostics.toomreWavelength =
        4._f * sqr(PI) * cfg.gravity * diagnostics.surfaceDensity / sqr(cfg.omega);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);

    Vector mean(0._f);
    Vector sumSq(0._f);
    for (Size i = 0; i < r.size(); ++i) {
        const Vector rel(v[i][X],
            v[i][Y] + 1.5_f * cfg.omega * relativePosition(cfg, r[i])[X],
            v[i][Z]);
        mean += rel;
        sumSq += rel * rel;
    }
    if (!r.empty()) {
        mean /= Float(r.size());
        sumSq /= Float(r.size());
    }
    const Vector variance = max(sumSq - mean * mean, Vector(0._f));
    diagnostics.velocityDispersion = Vector(sqrt(variance[X]), sqrt(variance[Y]), sqrt(variance[Z]));
    return diagnostics;
}

INLINE void storeDiagnostics(const Diagnostics& diagnostics, Statistics& stats) {
    stats.set(StatisticsId::SHEARING_SHEET_TOTAL_MASS, diagnostics.totalMass);
    stats.set(StatisticsId::SHEARING_SHEET_SURFACE_DENSITY, diagnostics.surfaceDensity);
    stats.set(StatisticsId::SHEARING_SHEET_TOOMRE_WAVELENGTH, diagnostics.toomreWavelength);
    stats.set(StatisticsId::SHEARING_SHEET_VELOCITY_DISPERSION_X, diagnostics.velocityDispersion[X]);
    stats.set(StatisticsId::SHEARING_SHEET_VELOCITY_DISPERSION_Y, diagnostics.velocityDispersion[Y]);
    stats.set(StatisticsId::SHEARING_SHEET_VELOCITY_DISPERSION_Z, diagnostics.velocityDispersion[Z]);
}

} // namespace ShearingSheet

NAMESPACE_SPH_END
