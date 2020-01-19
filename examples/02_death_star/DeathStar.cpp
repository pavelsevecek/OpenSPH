#include "Sph.h"
#include <iostream>

using namespace Sph;

// Custom domain, created by a boolean subtraction of two other domains.
class SubtractDomain : public IDomain {
private:
    IDomain& primary;
    IDomain& subtracted;

public:
    SubtractDomain(IDomain& primary, IDomain& subtracted)
        : primary(primary)
        , subtracted(subtracted) {}

    virtual Vector getCenter() const override {
        // It does not have to be the EXACT geometric center, so let's return the center of
        // the primary domain for simplicity
        return primary.getCenter();
    }

    virtual Box getBoundingBox() const override {
        // Subtracted domain can only shrink the bounding box, so it's OK to return the primary one
        return primary.getBoundingBox();
    }

    virtual Float getVolume() const override {
        // Estimate the actual volume by the volume of the primary domain. In this example, this can only
        // cause underestimation of the number of particles.
        return primary.getVolume();
    }

    virtual Float getSurfaceArea() const override {
        // Similar to the argument in getVolume
        return primary.getSurfaceArea();
    }

    virtual bool contains(const Vector& v) const override {
        // The main part - vector is contained in the domain if it is contained in the primary domain
        // and NOT contained in the subtracted domain
        return primary.contains(v) && !subtracted.contains(v);
    }

    // Additional functions needed by some components of the code that utilize the IDomain interface (such as
    // boundary conditions); they are not needed in this example, so let's simply mark them as not
    // implemented.

    virtual void getSubset(ArrayView<const Vector>, Array<Size>&, const SubsetType) const override {
        NOT_IMPLEMENTED
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED
    }

    virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
        NOT_IMPLEMENTED
    }

    virtual void addGhosts(ArrayView<const Vector>, Array<Ghost>&, const Float, const Float) const override {
        NOT_IMPLEMENTED
    }
};


class DeathStar : public IRun {
public:
    virtual void setUp(SharedPtr<Storage> storage) override {

        // base spherical domain without the crater with radius 1000m
        SphericalDomain primarySphere(Vector(0._f), 1.e3_f);

        // sphere we are going to subtract to create the crater with radius 500m
        SphericalDomain subtractedSphere(Vector(1.2e3_f, 0._f, 0._f), 5.e2_f);

        // compose the domains
        SubtractDomain domain(primarySphere, subtractedSphere);

        // the rest is the same as in "Hello Asteroid"
        InitialConditions ic(settings);
        BodySettings body;
        ic.addMonolithicBody(*storage, domain, body);

        settings.set(RunSettingsId::RUN_NAME, std::string("Death star"));
        settings.set(RunSettingsId::RUN_END_TIME, 1._f);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        BinaryOutput io(Path("output.ssf"));
        io.dump(storage, stats);
    }
};

int main() {
    try {
        DeathStar simulation;
        Storage storage;
        simulation.run(storage);
    } catch (Exception& e) {
        std::cout << "Error during simulation: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
