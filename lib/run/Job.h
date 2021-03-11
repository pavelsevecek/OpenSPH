#pragma once

#include "objects/containers/UnorderedMap.h"
#include "objects/geometry/Domain.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Storage.h"
#include "run/VirtualSettings.h"
#include "system/Settings.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

class IMaterial;
class IRunCallbacks;
class IRun;
class IJobData;

enum class JobType {
    /// Job providing particles
    PARTICLES,

    /// Job providing a material
    MATERIAL,

    /// Job providing geometry
    GEOMETRY,
};


struct ParticleData {
    /// \brief Holds all particle positions and other quantities.
    Storage storage;

    /// \brief Final statistics of the simulation.
    Statistics stats;

    /// \brief Overrides associated with the particle state.
    ///
    /// This is mainly used to specify initial time for simulations resumed from saved state.
    RunSettings overrides = EMPTY_SETTINGS;
};

/// \brief Data exchanged by workers.
///
/// The context is a variant, containing either particle data, material or geometry. Each worker receives a
/// number of contexts as inputs, performs its operation and returns contexts as a result. The returned
/// context can be the same as the input context; this is mostly used by workers than perform a minor
/// modification of the input.
class JobContext : public Polymorphic {
private:
    SharedPtr<IJobData> data;

public:
    JobContext() = default;

    template <typename TValue>
    JobContext(SharedPtr<TValue> value);

    /// \brief Returns the stored value.
    ///
    /// Type \ref TValue can be either \ref ParticleData, \ref IMaterial or \ref IDomain. If the type of the
    /// stored value is different or no value is stored, \ref InvalidSetup exception is thrown.
    template <typename TValue>
    SharedPtr<TValue> getValue() const;

    /// \brief Returns the stored value or nullptr if the provided type \ref TValue does not match the type of
    /// the stored value.
    ///
    /// Type \ref TValue can be either \ref ParticleData, \ref IMaterial or \ref IDomain.
    template <typename TValue>
    SharedPtr<TValue> tryGetValue() const;

    /// \brief Duplicates the stored data.
    ///
    /// Note that \ref WorkerContext has a pointer semantics, a copy will thus reference the same object as
    /// the original. Use this function to perform a deep copy and return an independent instance.
    JobContext clone() const;

    /// \brief Releases all allocated data
    void release();
};

/// \brief Base class for all object performing an operation in a simulation hierarchy.
///
/// There are currently three type of workers: particle workers, derived from class \ref IParticleJob,
/// represent all simulations, initial conditions, particle hand-offs and other particle transforms, etc.
/// \ref IGeometryWorker provides generic geometric shapes, which can be used to define bodies, specify
/// boundary conditions etc. Finally \ref IMaterialJob provides material of bodies.
///
/// Each worker defines a number of inputs, which are provided by other workers. This number does not have to
/// be fixed, it may depend on internal state of the worker. The inputs should not be assigned by the workers
/// itself, this is provided by class \ref JobNode, which connects the workers in the worker hierarchy.
class IJob : public Polymorphic {
    friend class JobNode;

protected:
    std::string instName;

    /// Contains all input data, identified by names of input slots.
    UnorderedMap<std::string, JobContext> inputs;

public:
    explicit IJob(const std::string& name)
        : instName(name) {}

    /// \brief Unique name representing this worker
    virtual std::string instanceName() const {
        return instName;
    }

    /// \brief Name representing the type of the worker (e.e. "SPH").
    virtual std::string className() const = 0;

    /// \brief List of slots that need to be connected to evaluate the worker.
    ///
    /// The returned map shall contain names of the slots and their types. No two slots can have the same
    /// name. This list can can be changed, based on internal state of the worker.
    virtual UnorderedMap<std::string, JobType> requires() const {
        return this->getSlots();
    }

    /// \brief Lists all potential inputs of the worker.
    ///
    /// This is the superset of slots returned by function \ref requires and it has to be fixed, i.e. cannot
    /// change when internal state of the worker changes.
    virtual UnorderedMap<std::string, JobType> getSlots() const = 0;

    /// \brief Specifies the type of the worker, i.e. what kind of data the worker provides.
    virtual JobType provides() const = 0;

    /// \brief Returns a settings object which allows to query and modify the state of the worker.
    ///
    /// It is not necessary to expose all state variables this way. This is mainly intended for
    /// (de)serialization of the worker state and for connection with UI controls.
    virtual VirtualSettings getSettings() = 0;

    /// \brief Runs the operation provided by the worker.
    ///
    /// Function may be called only after required inputs are assigned; this is provided by class \ref
    /// JobNode, the worker should not be used directly.
    /// \param global Global settings, shared by all workers. Contains parameters like number of threads, etc.
    /// \param callbacks Interface allowing to get notified about current progress of the worker.
    /// \throw InvalidSetup if required input is missing or worker encountered a problem in initialization.
    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) = 0;

    /// \brief Returns the result of the worker.
    ///
    /// This can only be called after the worker is evaluated. The result is cached by the worker, so the
    /// function \ref getResult can be called multiple times once the worker finishes.
    virtual JobContext getResult() const = 0;

protected:
    /// \brief Convenient function to return input data for slot of given name.
    template <typename T>
    SharedPtr<T> getInput(const std::string& name) const;
};

/// \brief Provides a descriptor of a worker that allows to create new instances.
///
/// This is mainly intended to provide a way to enumerate all available workers and create new instances
/// of workers without having a specific type at hand. Each class derived from \ref IJob should be
/// associated with one \ref IJobDesc implementation. It is not necessary to implement this interface
/// manually, consider using helper class \ref JobRegistrar.
class IJobDesc : public Polymorphic {
public:
    /// \brief Returns the class name of the worker.
    ///
    /// It must be the same name as returned by \ref IJob::className. It is necessary to duplicate the name
    /// here, as we need to get the class name without having to create a new instance of the worker.
    virtual std::string className() const = 0;

    /// \brief Returns a name of the category of worker.
    ///
    /// There is no predefined list of categories, it can be anything that describes the worker, for example
    /// "simulations", "initial conditions", etc. For consistency, use plural nouns as in the examples.
    virtual std::string category() const = 0;

    /// \brief Returns a help message, explaining what the worker does and how it should be used.
    virtual std::string tooltip() const {
        return "";
    }

    /// \brief Creates a new worker instance.
    ///
    /// \param instanceName Name of the instance, may be NOTHING in which case a generic name will be
    /// assigned based on the class name.
    virtual AutoPtr<IJob> create(Optional<std::string> instanceName) const = 0;
};

using CreateJobFunc = Function<AutoPtr<IJob>(std::string name)>;

/// \brief Helper class, allowing to register worker into the global list of workers.
///
/// This is mainly used by UI controls to get the list of all workers without having to manually specify them.
/// The expected usage is to create a static variable of this type and pass required strings and callbacks in
/// the constructor.
struct JobRegistrar {
    /// \brief Registers a new worker.
    ///
    /// \param className Class name of the worker, must be the same as returned by \ref IJob::className.
    /// \param category Name of the worker category.
    /// \param func Functor returning a new instance of the worker.
    /// \param tooltip Optional description of the worker
    JobRegistrar(std::string className, std::string category, CreateJobFunc func, std::string tooltip);

    /// \brief Registers a new worker.
    ///
    /// \param className Class name of the worker, must be the same as returned by \ref IJob::className.
    /// \param shortName Name used for newly created instances, useful in case the class name is too long.
    /// \param category Name of the worker category.
    /// \param func Functor returning a new instance of the worker.
    /// \param tooltip Optional description of the worker
    JobRegistrar(std::string className,
        std::string shortName,
        std::string category,
        CreateJobFunc func,
        std::string tooltip);
};

/// \brief Returns a view of all currently registered workers.
ArrayView<const AutoPtr<IJobDesc>> enumerateRegisteredJobs();

/// \brief Returns a worker descriptor for given class name.
///
/// Only descriptors registered via \ref WorkerRegistrar can be obtained this way. If no descriptor with given
/// name exists, the function returns nullptr.
RawPtr<IJobDesc> getJobDesc(const std::string& name);

/// \brief Adds a common settings category, used by all workers.
///
/// The category currently obtains only an entry for the instance name of the worker.
VirtualSettings::Category& addGenericCategory(VirtualSettings& connector, std::string& instanceName);


/// \brief Base class for all workers providing particle data.
class IParticleJob : public IJob {
protected:
    /// Data filled by the worker when it finishes.
    SharedPtr<ParticleData> result;

public:
    explicit IParticleJob(const std::string& name);

    ~IParticleJob();

    virtual JobType provides() const override final {
        return JobType::PARTICLES;
    }

    virtual JobContext getResult() const override final {
        return result;
    }
};

/// \brief Base class for workers running a simulation.
///
/// Simulation workers can either derive from this or from more generic base class \ref IParticleJob. This
/// class allows connecting workers with \ref IRun interface, so a simulation can be easily plugged into the
/// worker hierarchy if it is already implemented as \ref IRun.
class IRunJob : public IParticleJob {
public:
    explicit IRunJob(const std::string& name);

    ~IRunJob();

    /// \brief Returns the actual simulation object.
    ///
    /// This provides a way to implement the worker functionality, as function \ref evaluate is final and used
    /// only internally.
    /// \param overrides Settings overriding the current settings of the worker.
    virtual AutoPtr<IRun> getRun(const RunSettings& overrides) const = 0;

protected:
    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override final;
};

/// \brief Base class for workers providing a geometric shape.
class IGeometryJob : public IJob {
protected:
    /// Data filled by the worker when it finishes.
    SharedPtr<IDomain> result;

public:
    IGeometryJob(const std::string& name)
        : IJob(name) {}

    virtual JobType provides() const override final {
        return JobType::GEOMETRY;
    }

    virtual JobContext getResult() const override final {
        return result;
    }
};

/// \brief Base class for workers providing a material.
class IMaterialJob : public IJob {
protected:
    /// Data filled by the worker when it finishes.
    SharedPtr<IMaterial> result;

public:
    IMaterialJob(const std::string& name)
        : IJob(name) {}

    virtual JobType provides() const override final {
        return JobType::MATERIAL;
    }

    virtual JobContext getResult() const override final {
        return result;
    }
};


NAMESPACE_SPH_END
