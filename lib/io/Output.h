#pragma once

#include "io/Path.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Outcome.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class Deserializer;

/// \brief Helper file generating file names for output files.
class OutputFile {
private:
    mutable Size dumpNum = 0;
    Path pathMask;

public:
    OutputFile() = default;

    OutputFile(const Path& pathMask);

    /// Returns path to the next output file, incrementing the internal counter. No file is created by this.
    Path getNextPath(const Statistics& stats) const;
};


class ITextColumn;

/// \brief Interface for saving quantities of SPH particles to a file.
///
/// Saves all values in the storage or selected few quantities, depending on implementation. It is also
/// implementation-defined whether the saved representation of storage is lossless, i.e. whether the
/// storage can be loaded with \ref load without a loss in precision.
class IOutput : public Polymorphic {
protected:
    OutputFile paths;

public:
    /// \brief Constructs output object for loading.
    ///
    /// Object in this state cannot call \ref dump, checked by assert. It can only be used to load storage
    /// using \ref load. Object capable of dumping storage can be then created using copy/move operator.
    IOutput() = default;

    /// \brief Constructs output given the file name of the output.
    ///
    /// The name is used for all output files, meaning if \ref dump is called twice, the second output
    /// file will override the previous one. To avoid this, the fileMask can contain following wildcards:
    /// - '%d' - replaced by the dump number, starting from 0, incremented every dump.
    /// - '%t' - replaced by current simulation time (with _ instead of decimal separator).
    IOutput(const Path& fileMask);

    /// \brief Saves data from particle storage into the file.
    ///
    /// Returns the filename of the dump, generated from file mask given in constructor.
    virtual Path dump(Storage& storage, const Statistics& stats) = 0;

    /// \brief Loads data from the file into the storage.
    ///
    /// This will remove any data previously stored in storage. Can be used to continue simulation from saved
    /// snapshot.
    virtual Outcome load(const Path& path, Storage& storage, Statistics& stats) = 0;
};


/// Output saving data to text (human readable) file.
class TextOutput : public IOutput {
public:
    enum class Options {
        /// No columns are created by default, they have to be added by the user
        NO_COLUMNS = 0,

        /// Basic columns are created (particle index, positions, velocitites, masses)
        BASIC_COLUMNS = 1 << 0,

        /// Additional columns for particle quantities are added. This implies option BASIC_COLUMNS.
        /// Suitable for SPH impact simulations.
        EXTENDED_COLUMNS = 1 << 1,

        /// Dumps all quantity values from the storage. This option implies BASIC_COLUMNS.
        DUMP_ALL = 1 << 2,

        /// Writes all numbers in scientific format
        SCIENTIFIC = 1 << 3,
    };

private:
    std::string runName;

    /// Flags of the output
    Flags<Options> flags;

    /// Value columns saved into the file
    Array<AutoPtr<ITextColumn>> columns;

public:
    TextOutput(const Flags<Options> flags = EMPTY_FLAGS);

    TextOutput(const Path& fileMask, const std::string& runName, const Flags<Options> flags = EMPTY_FLAGS);

    ~TextOutput();

    /// \brief Adds a new column to be saved into the file.
    ///
    /// Unless an option is specified in constructor, the file has no columns. All quantities must be
    /// explicitly added using this function. The column is added to the right end of the text file.
    /// \param column New column to save; see \ref ITextColumn and derived classes
    /// \return Reference to itself, allowing to queue calls
    TextOutput& addColumn(AutoPtr<ITextColumn>&& column);

    virtual Path dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const Path& path, Storage& storage, Statistics& stats) override;
};

/// Extension of text output that runs given gnuplot script on dumped text data.
class GnuplotOutput : public TextOutput {
private:
    std::string scriptPath;

public:
    GnuplotOutput(const Path& fileMask,
        const std::string& runName,
        const std::string& scriptPath,
        const Flags<Options> flags)
        : TextOutput(fileMask, runName, flags)
        , scriptPath(scriptPath) {}

    virtual Path dump(Storage& storage, const Statistics& stats) override;
};


/// \brief Output saving data to binary data without loss of precision.
///
/// \subsection Format specification
/// File format can store values of type uint64 (called Size hereafter), double (called Float hereafter),
/// strings encoded as chars with terminating zero and padding (zero bytes). There are no other types
/// currently used. All other types (Vectors, Tensors, enums, ...) are converted to Floats or Sizes.
/// The file is divided as follows: header, quantity info, n x [material info, quantity data] where n is the
/// number of materials.
///
/// Header is always 256 bytes, storing the following:
///  - first 4 bytes are file format identifier "SPH" (including terminating zero)
///  - time [Float] of the dump (in simulation time)
///  - total particle count [Size] in the storage (called particleCnt)
///  - number of quantities [Size] in the storage (called quantityCnt)
///  - number of materials [Size] (called materialCnt)
///  - padding until 256th byte (possibly will be used in the future)
///
/// Quantity info (summary) follows the header. It consist of quantityCnt triples of values
///  - ID of quantity [Size casted from QuantityId]
///  - Order of quantity [Size casted from OrderEnum]
///  - Value type of quantity [Size casted from ValueEum]
///
/// After than there follows material info and quantity data for every stored material. The material info
/// consist of materialCnt blocks with content:
///  - block identifier "MAT" (4 bytes with terminating zero)
///  - number of material [Size] in increasing order (from 0 up to materialCnt-1)
///  - number of BodySettings entries [Size] of the material (called paramCnt)
///  - paramCnt times:
///     . parameter ID [Size casted from BodySettingsId]
///     . type index of the value type [Size], corresponding to the type in settings variant
///     . entry value itself. The size of the value depends on its type, identified by the index read
///       previously. There is currently no upper limit for the entry size; although the largest one used is a
///       Tensor with 72 bytes, this size can be increased in the future.
///  - quantityCnt times:
///     . Quantity ID of n-th quantity [Size casted from QuantityId]
///     . Range of n-th quantity [2x Float]
///     . minimal value of n-th quantity [Float] for timestepping
/// \attention The whole material block can be missing as Storage can exist without a material. This is
/// indicated by materialCnt == 0, but also by identifier "NOMAT" (6 bytes including terminating zero)
/// in place of first "MAT" identifier if the file had material block.
///
/// The next two numbers [2x Float] are the first and one-past-last index of the particles corresponding to
/// this material. The first index for the first material is always zero, the second index for the last
/// material
/// is the particleCnt. The first index is always equal to the second index of the previous material.
/// The quantity data are stored immediately afterwards. It consist of quantityCnt blocks. Each block has 1-3
/// sub-blocks, depending on the order of n-th. Note that the orders and value types of quantities are stored
/// in the quantity info at the beginning of the file and are not repeated here.
/// Each sub-block contains directly serialized value and derivatives of n-th quantity, i.e.
///  - particleCnt times value, representing quantity value of i-th particle
///  - if quantity is of first or second order, then follows particleCnt times value, representing quantity
///    first derivative of i-th particle
///  - if quantity is of second order, then follows particleCnt times values, representing quantity second
///  derivative of i-th particle.
/// The size of a single value depends on the value type, i.e. scalar quantities are stored as Floats, Vector
/// quantities are stored as 4x Float (components X, Y, Z and H), etc.
/// The file ends immediately after the last quantity is saved.
///
/// \todo Possible todos & fixes:
///  - arbitrary precision: store doubles as floats or halfs and size_t as uint32 or uint16, based on data in
///    header
///  - deserialized materials are created using Factory::getMaterial from loaded settings. This won't be
///    correct if the material was created differently, i.e. if the material doesn't match the information in
///    the settings it holds. This should be enforced somehow.
class BinaryOutput : public IOutput {
private:
    static constexpr Size PADDING_SIZE = 212;

public:
    BinaryOutput() = default;

    BinaryOutput(const Path& fileMask);

    virtual Path dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const Path& path, Storage& storage, Statistics& stats) override;

    struct Info {
        /// Number of quantities in the file
        Size quantityCnt;

        /// Number of particles in the file
        Size particleCnt;

        /// Number of materials in the file
        Size materialCnt;

        /// Information about stored quantities
        struct QuantityInfo {

            /// ID of the quantity
            QuantityId id;

            /// Order (=number or derivatives)
            OrderEnum order;

            /// Value type of the quantity
            ValueEnum value;
        };

        Array<QuantityInfo> quantityInfo;

        /// Run time of the snapshot
        Float runTime;

        /// Current timestep of the run
        Float timeStep;
    };

    /// \brief Opens the file and reads header info without reading the rest of the file.
    Expected<Info> getInfo(const Path& path) const;
};

struct PkdgravParams {

    /// Conversion formula from SPH particles to hard spheres in pkdgrav
    enum class Radius {
        /// Compute sphere radius using R = h/3 formula
        FROM_SMOOTHING_LENGTH,

        /// Compute sphere radius so that its mass is equivalent to the mass of SPH particle
        FROM_DENSITY
    };

    Radius radius = Radius::FROM_DENSITY;

    /// Conversion factors for pkdgrav
    struct Conversion {

        Float mass = Constants::M_sun;

        Float distance = Constants::au;

        Float velocity = Constants::au * 2._f * PI / (365.25_f * 86400._f);

    } conversion;

    /// Adds additional rotation of all particles around the origin. This can be used to convert a
    /// non-intertial coordinate system used in the code to intertial system.
    Vector omega = Vector(0._f);

    /// Threshold of internal energy; particles with higher energy are considered a vapor and
    /// we discard them in the output
    Float vaporThreshold = 1.e6_f;

    /// Color indices of individual bodies in the simulations. The size of the array must be equal (or bigger)
    /// than the number of bodies in the storage.
    Array<Size> colors{ 3, 13 };
};


/// \brief Dumps data into a file that can be used as an input for pkdgrav code by Richardson et al.
///
/// SPH particles are converted into hard spheres, moving with the velocity of the particle and no angular
/// velocity. Quantities are converted into G=1 units.
/// See \cite Richardson_etal_2000.
class PkdgravOutput : public IOutput {
private:
    /// Parameters of the SPH->pkdgrav conversion
    PkdgravParams params;

public:
    PkdgravOutput() = default;

    PkdgravOutput(const Path& fileMask, PkdgravParams&& params);

    virtual Path dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const Path& UNUSED(path),
        Storage& UNUSED(storage),
        Statistics& UNUSED(stats)) override {
        NOT_IMPLEMENTED;
    }

private:
    INLINE Float getRadius(const Float h, const Float m, const Float rho) const {
        switch (params.radius) {
        case PkdgravParams::Radius::FROM_DENSITY:
            // 4/3 pi r^3 rho = m
            return cbrt(3._f * m / (4._f * PI * rho));
        case PkdgravParams::Radius::FROM_SMOOTHING_LENGTH:
            return h / 3._f;
        default:
            NOT_IMPLEMENTED;
        }
    }
};

class NullOutput : public IOutput {
public:
    NullOutput()
        : IOutput() {}

    virtual Path dump(Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        return Path();
    }

    virtual Outcome load(const Path& UNUSED(path),
        Storage& UNUSED(storage),
        Statistics& UNUSED(stats)) override {
        return SUCCESS;
    }
};

NAMESPACE_SPH_END
