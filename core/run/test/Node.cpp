#include "run/Node.h"
#include "catch.hpp"
#include "run/jobs/InitialConditionJobs.h"
#include "run/jobs/MaterialJobs.h"

using namespace Sph;

class TestCallbacks : public IJobCallbacks {
public:
    virtual void onStart(const IJob& UNUSED(job)) override {}

    virtual void onEnd(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}

    virtual void onSetUp(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onTimeStep(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

class TestJob : public IGeometryJob {
public:
    explicit TestJob()
        : IGeometryJob("test") {}

    virtual String className() const override {
        return "test job";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES }, { "material", JobType::MATERIAL } };
    }

    virtual VirtualSettings getSettings() override {
        return {};
    }

    virtual void evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) override {
        this->getInput<ParticleData>("particles");
        this->getInput<IMaterial>("material");
        result = makeShared<SphericalDomain>(Vector(0._f), 1._f);
    }
};

TEST_CASE("Run correct", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<TestJob>();
    makeNode<MaterialJob>("material")->connect(node, "material");
    makeNode<MonolithicBodyIc>("particles")->connect(node, "particles");

    RunSettings globals;
    TestCallbacks callbacks;
    REQUIRE_NOTHROW(node->run(globals, callbacks));
}

TEST_CASE("Run without inputs", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<TestJob>();

    RunSettings globals;
    TestCallbacks callbacks;
    REQUIRE_THROWS_AS(node->run(globals, callbacks), InvalidSetup);
}

TEST_CASE("Connect incorrect ", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<TestJob>();
    SharedPtr<JobNode> provider = makeNode<TestJob>();
    REQUIRE_THROWS_AS(provider->connect(node, "particles"), InvalidSetup);
    REQUIRE_THROWS_AS(provider->connect(node, "material"), InvalidSetup);

    REQUIRE_THROWS_AS(provider->connect(node, "abcd"), InvalidSetup);
}

class BadJob : public TestJob {
public:
    virtual void evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) override {
        // incorrect type
        REQUIRE_THROWS_AS(this->getInput<IMaterial>("particles"), InvalidSetup);

        // incorrect name
        REQUIRE_THROWS_AS(this->getInput<IMaterial>("materiaq"), InvalidSetup);
    }
};

TEST_CASE("Bad job", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<BadJob>();
    makeNode<MaterialJob>("material")->connect(node, "material");
    makeNode<MonolithicBodyIc>("particles")->connect(node, "particles");

    RunSettings globals;
    TestCallbacks callbacks;
    node->run(globals, callbacks);
}


class MultipleBodyJob : public IParticleJob {
public:
    explicit MultipleBodyJob()
        : IParticleJob("test") {}

    virtual String className() const override {
        return "multiple body job";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "body A", JobType::PARTICLES }, { "body B", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override {
        return {};
    }

    virtual void evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) override {
        SharedPtr<ParticleData> data1 = this->getInput<ParticleData>("body A");
        SharedPtr<ParticleData> data2 = this->getInput<ParticleData>("body B");
        REQUIRE(data1 != data2);

        REQUIRE(data1->storage.getParticleCnt() == data2->storage.getParticleCnt());
        ArrayView<const Vector> r1 = data1->storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> r2 = data2->storage.getValue<Vector>(QuantityId::POSITION);
        REQUIRE(r1 == r2);

        data1->storage.removeAll();
        REQUIRE(data1->storage.empty());
        REQUIRE(!data2->storage.empty());
    }
};

TEST_CASE("Same input connected to multiple slots", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<MultipleBodyJob>();
    SharedPtr<JobNode> particles = makeNode<MonolithicBodyIc>("particles");
    particles->connect(node, "body A");
    particles->connect(node, "body B");

    RunSettings globals;
    TestCallbacks callbacks;
    node->run(globals, callbacks);
}

TEST_CASE("Slot queries", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<TestJob>();
    REQUIRE(node->getSlotCnt() == 2);
    SlotData slot0 = node->getSlot(0);
    SlotData slot1 = node->getSlot(1);
    REQUIRE_THROWS_AS(node->getSlot(2), InvalidSetup);

    REQUIRE(slot0.name == "particles");
    REQUIRE(slot0.type == JobType::PARTICLES);
    REQUIRE(slot0.used);
    REQUIRE(slot0.provider == nullptr);

    REQUIRE(slot1.name == "material");
    REQUIRE(slot1.type == JobType::MATERIAL);
    REQUIRE(slot1.used);
    REQUIRE(slot1.provider == nullptr);
}

TEST_CASE("Checking connections", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<TestJob>();
    SharedPtr<JobNode> material = makeNode<MaterialJob>("material");
    SharedPtr<JobNode> particles = makeNode<MonolithicBodyIc>("particles");
    REQUIRE(material->getDependentCnt() == 0);
    REQUIRE(particles->getDependentCnt() == 0);

    material->connect(node, "material");
    particles->connect(node, "particles");

    REQUIRE(node->getSlot(0).provider == particles);
    REQUIRE(node->getSlot(1).provider == material);

    REQUIRE(material->getDependentCnt() == 1);
    REQUIRE(material->getDependent(0) == node);
    REQUIRE(particles->getDependentCnt() == 1);
    REQUIRE(particles->getDependent(0) == node);
}

TEST_CASE("Node disconnect", "[nodes]") {
    SharedPtr<JobNode> node = makeNode<TestJob>();
    SharedPtr<JobNode> material = makeNode<MaterialJob>("material");
    SharedPtr<JobNode> particles = makeNode<MonolithicBodyIc>("particles");
    material->connect(node, "material");
    particles->connect(node, "particles");
    REQUIRE(node->getSlot(0).provider == particles);
    REQUIRE(node->getSlot(1).provider == material);

    material->disconnect(node);
    REQUIRE(node->getSlot(0).provider == particles);
    REQUIRE(node->getSlot(1).provider == nullptr);
    REQUIRE(material->getDependentCnt() == 0);

    particles->disconnect(node);
    REQUIRE(node->getSlot(0).provider == nullptr);
    REQUIRE(node->getSlot(1).provider == nullptr);
    REQUIRE(particles->getDependentCnt() == 0);
}
