#pragma once

/// \file Column.h
/// \brief Object for printing quantity values into output
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/utility/Dynamic.h"
#include "quantities/Quantity.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// \brief Base class for conversion of quantities into the output data.
///
/// When TextOutput is selected, this represents a single column of values in the file, hence the name.
/// Ordinarily, we need to store the quantity values and their derivatives directly, derived classes
/// \ref ValueColumn and \ref DerivativeColumn can be used for this purpose. Other implementations can be used
/// to store values that are not directly saved in any quantity, such as smoothing lenghts (they are actually
/// stored as 4th component of the position vectors), or actual values of stress tensor (quantity contains
/// undamaged values).
///
/// The class can also be used to save arbitrary data, such as particle index, current time
/// of the simulation, etc. This can be useful when using the output files in additional scripts, for example
/// when creating plots in Gnuplot.
///
/// \todo There should also be a conversion from code units to user-selected output units
class ITextColumn : public Polymorphic {
public:
    /// \brief Returns the value of the output column for given particle.
    ///
    /// \param storage Storage containing all particle data
    /// \param stats Holds simulation time as well as additional solver-specific statistics.
    /// \param particleIdx Index of the particle to evaluate.
    virtual Dynamic evaluate(const Storage& storage,
        const Statistics& stats,
        const Size particleIdx) const = 0;

    /// \brief Reads the value of the column and saves it into the storage, if possible.
    ///
    /// \param storage Particle storage where the value is stored
    /// \param value Accumulated value, must be the same type as this column. Checked by assert.
    /// \param particleIdx Index of accumulated particle; if larger than current size of the storage, the
    ///                    storage is resized accordingly.
    virtual void accumulate(Storage& storage, const Dynamic value, const Size particleIdx) const = 0;

    /// \brief Returns a name of the column.
    ///
    /// The name is printed in the header of the output file.
    virtual std::string getName() const = 0;

    /// \brief Returns the value type of the column.
    virtual ValueEnum getType() const = 0;
};


/// \brief Returns values of given quantity as stored in storage.
///
/// This is the most common column. Most columns for quantities can be added using \ref OutputQuantityFlag,
/// however if additional quantities need to be saved, it can be done using:
/// \code
/// TextOutput output(outputPath, "run name", EMPTY_FLAGS);
/// // add temperature (scalar quantity)
/// output.addColumn(makeAuto<ValueColumn<Float>>(QuantityId::TEMPERATURE));
/// // add surface normal (vector quantity)
/// output.addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::SURFACE_NORMAL));
/// \endcode
template <typename TValue>
class ValueColumn : public ITextColumn {
private:
    QuantityId id;

public:
    ValueColumn(const QuantityId id)
        : id(id) {}

    virtual Dynamic evaluate(const Storage& storage,
        const Statistics& UNUSED(stats),
        const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getValue<TValue>(id);
        return value[particleIdx];
    }

    virtual void accumulate(Storage& storage, const Dynamic value, const Size particleIdx) const override {
        if (!storage.has(id)) {
            // lazy initialization
            storage.insert<TValue>(id, OrderEnum::ZERO, TValue(0._f));
        }
        Array<TValue>& array = storage.getValue<TValue>(id);
        array.resize(particleIdx + 1); /// \todo must also resize derivaties, or avoid resizing
        array[particleIdx] = value.get<TValue>();
    }

    virtual std::string getName() const override {
        return getMetadata(id).quantityName;
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// \brief Returns first derivatives of given quantity as stored in storage.
///
/// Quantity must contain derivative, checked by assert.
template <typename TValue>
class DerivativeColumn : public ITextColumn {
private:
    QuantityId id;

public:
    DerivativeColumn(const QuantityId id)
        : id(id) {}

    virtual Dynamic evaluate(const Storage& storage,
        const Statistics& UNUSED(stats),
        const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getDt<TValue>(id);
        return value[particleIdx];
    }

    virtual void accumulate(Storage& storage, const Dynamic value, const Size particleIdx) const override {
        if (!storage.has(id)) {
            // lazy initialization
            storage.insert<TValue>(id, OrderEnum::FIRST, TValue(0._f));
        } else if (storage.getQuantity(id).getOrderEnum() == OrderEnum::ZERO) {
            // has the quantity, but not derivatives; we need to resize it manually to side-step the check
            // of equality in Storage::insert.
            storage.getQuantity(id).setOrder(OrderEnum::FIRST);
        }
        Array<TValue>& array = storage.getDt<TValue>(id);
        array.resize(particleIdx + 1);
        array[particleIdx] = value.get<TValue>();
    }

    virtual std::string getName() const override {
        return getMetadata(id).derivativeName;
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// \brief Returns second derivatives of given quantity as stored in storage.
///
/// Quantity must contain second derivative, checked by assert.
template <typename TValue>
class SecondDerivativeColumn : public ITextColumn {
private:
    QuantityId id;

public:
    SecondDerivativeColumn(const QuantityId id)
        : id(id) {}

    virtual Dynamic evaluate(const Storage& storage,
        const Statistics& UNUSED(stats),
        const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getAll<TValue>(id)[2];
        return value[particleIdx];
    }

    virtual void accumulate(Storage& storage, const Dynamic value, const Size particleIdx) const override {
        if (!storage.has(id)) {
            // lazy initialization
            storage.insert<TValue>(id, OrderEnum::SECOND, TValue(0._f));
        } else if (storage.getQuantity(id).getOrderEnum() < OrderEnum::SECOND) {
            // has the quantity, but not derivatives; we need to resize it manually to side-step the check
            // of equality in Storage::insert.
            storage.getQuantity(id).setOrder(OrderEnum::SECOND);
        }
        Array<TValue>& array = storage.getD2t<TValue>(id);
        array.resize(particleIdx + 1);
        array[particleIdx] = value.get<TValue>();
    }

    virtual std::string getName() const override {
        return getMetadata(id).secondDerivativeName;
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// \brief Returns smoothing lengths of particles
class SmoothingLengthColumn : public ITextColumn {
public:
    virtual Dynamic evaluate(const Storage& storage,
        const Statistics& UNUSED(stats),
        const Size particleIdx) const override {
        ArrayView<const Vector> value = storage.getValue<Vector>(QuantityId::POSITION);
        return value[particleIdx][H];
    }

    virtual void accumulate(Storage& storage, const Dynamic value, const Size particleIdx) const override {
        if (!storage.has(QuantityId::POSITION)) {
            // lazy initialization
            storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, Vector(0._f));
        }
        Array<Vector>& array = storage.getValue<Vector>(QuantityId::POSITION);
        array.resize(particleIdx + 1);
        array[particleIdx][H] = value.get<Float>();
    }

    virtual std::string getName() const override {
        return "Smoothing length";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};

/// \brief Prints actual values of scalar damage.
///
/// Needed because damage is stored in storage as third roots. Can be used for both scalar and tensor damage.
template <typename TValue>
class DamageColumn : public ITextColumn {
public:
    virtual Dynamic evaluate(const Storage& storage,
        const Statistics& UNUSED(stats),
        const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getValue<TValue>(QuantityId::DAMAGE);
        return pow<3>(value[particleIdx]);
    }

    virtual void accumulate(Storage& storage, const Dynamic value, const Size particleIdx) const override {
        Array<TValue>& array = storage.getValue<TValue>(QuantityId::DAMAGE);
        array.resize(particleIdx + 1);
        array[particleIdx] = root<3>(value.get<TValue>());
    }

    virtual std::string getName() const override {
        return "Damage";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};

/// \brief Helper column printing particle numbers.
class ParticleNumberColumn : public ITextColumn {
public:
    virtual Dynamic evaluate(const Storage& UNUSED(storage),
        const Statistics& UNUSED(stats),
        const Size particleIdx) const override {
        return particleIdx;
    }

    virtual void accumulate(Storage&, const Dynamic, const Size) const override {
        // do nothing
    }

    virtual std::string getName() const override {
        return "Particle index";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::INDEX;
    }
};

/// \brief Helper column printing current run time. This value is the same for every particle.
class TimeColumn : public ITextColumn {
public:
    virtual Dynamic evaluate(const Storage& UNUSED(storage),
        const Statistics& stats,
        const Size UNUSED(particleIdx)) const override {
        return stats.get<Float>(StatisticsId::RUN_TIME);
    }

    virtual void accumulate(Storage&, const Dynamic, const Size) const override {
        // do nothing
    }

    virtual std::string getName() const override {
        return "Time";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};


NAMESPACE_SPH_END
