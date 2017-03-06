#pragma once

#include "quantities/AbstractMaterial.h"

NAMESPACE_SPH_BEGIN

/// Material holding equation of state
class EosMaterial : public Abstract::Material {
private:
    std::unique_ptr<Abstract::Eos> eos;

public:
    virtual void initialize(Storage& storage) override {


        matIdxs = storage.getValue<Size>(QuantityIds::MATERIAL_IDX);
        tie(rho, u) = storage.getValues<Float>(QuantityIds::DENSITY, QuantityIds::ENERGY);
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            if (matIdxs[i] == id) {
                // this material
                /// \todo zamichana zavislost, material by asi vubec nemel zaviset na storage ...
                eos->evaluate(rho[i], u[i]);
            }
        }
    }

    /// Called after derivatives are computed.
    virtual void finalize(Storage& storage) = 0;

    /// Returns values of quantity from the material. If the material does not affect the quantity in any
    /// way, simply returns the quantity as stored in storage. Can only be called between calls of \ref
    /// initialize and \ref finalize each step.
    /// \todo move modifier here
    virtual Quantity& getValue(const QuantityIds& key) {
        // does not modify, return quantity
        return storage.getQuantity(key);
    }
};


/// Solid material is a generalization of material with equation of state, also having rheology that modifies
/// pressure and stress tensor.
class SolidMaterial : public EosMaterial {
private:
    std::unique_ptr<Abstract::Rheology> rheology;

public:
    virtual void initialize(Storage& storage) override {
        EosMaterial::initialize(storage);
    }

    /// somehow get rid of this circular dependency
    ///  -----> add option to storage to iterate over given material!!!!!!!
    /// subsetIterator? asi neni potreba obecne, proste storage vraci Neco co ma begin() a end() a ty vraci
    /// MaterialIterator, ktery se pri ++ posune na dalsi castici stejneho materialu, nebo na end()
    virtual void finalize(Storage& storage) override {
        rheology->integrate(storage);
    }

    virtual Quantity& getValue(const QuantityIds& key) {
        switch (key) {
        case QuantityIds::PRESSURE:
            return rhelogy->getPressure();
        case QuantityIds::DEVIATORIC_STRESS:
            return rheology->getStressTensor();
        default:
            EosMaterial::getValue(key);
        }
    }
}


NAMESPACE_SPH_END
