#pragma once

#include "io/Path.h"
#include "objects/wrappers/Outcome.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class OutputFile {
private:
    mutable Size dumpNum = 0;
    Path pathMask;

public:
    OutputFile() = default;

    OutputFile(const Path& pathMask);

    /// Returns path to the next output file, incrementing the internal counter. No file is created by this.
    Path getNextPath() const;
};

/// Interface for saving quantities of SPH particles to a file. Saves all values in the storage, and also 1st
/// derivatives for 2nd-order quantities.
namespace Abstract {
    class Column;

    class Output : public Polymorphic {
    protected:
        OutputFile paths;

    public:
        /// Constructs output given the file name of the output. The name must contain '%d', which will be
        /// replaced by the dump number, starting from 0.
        Output(const Path& fileMask);

        /// Saves data from particle storage into the file. Returns the filename of the dump.
        virtual Path dump(Storage& storage, const Statistics& stats) = 0;

        /// Loads data from the file into the storage. This will remove any data previously stored in storage.
        /// Can be used to continue simulation from saved snapshot.
        virtual Outcome load(const Path& path, Storage& storage) = 0;
    };
}

/// Output saving data to text (human readable) file.
class TextOutput : public Abstract::Output {
public:
    enum class Options {
        SCIENTIFIC = 1 << 0, ///< Writes all numbers in scientific format
    };

private:
    std::string runName;

    /// Flags of the output
    Flags<Options> flags;

    /// Value columns saved into the file
    Array<AutoPtr<Abstract::Column>> columns;

public:
    TextOutput(const Path& fileMask, const std::string& runName, const Flags<Options> flags);

    ~TextOutput();

    /// Adds an element to output.
    void add(AutoPtr<Abstract::Column>&& columns);

    virtual Path dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const Path& path, Storage& storage) override;
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
///   - arbitrary precision: store doubles as floats or halfs and size_t as uint32 or uint16, based on data in
///   header
class BinaryOutput : public Abstract::Output {
private:
    std::string runName;

    static constexpr Size PADDING_SIZE = 220;

public:
    BinaryOutput(const Path& fileMask, const std::string& runName);

    virtual Path dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const Path& path, Storage& storage) override;
};

struct PkdgravParams {

    /// Conversion formula from SPH particles to hard spheres in pkdgrav
    enum class Radius {
        /// Compute sphere radius using R = h/3 formula
        FROM_SMOOTHING_LENGTH,

        /// Compute sphere radius so that its mass is equivalent to the mass of SPH particle
        FROM_DENSITY
    } radius;

    /// Threshold of internal energy; particles with higher energy are considered a vapor and
    /// we discard them in the output
    Float vaporThreshold = 1.e6_f;

    /// Color indices of individual bodies in the simulations. The size of the array must be equal (or bigger)
    /// than the number of bodies in the storage.
    Array<Size> colors{ 3, 13 };
};

class PkdgravOutput : public Abstract::Output {
private:
    std::string runName;

    PkdgravParams params;

    /// conversion factors for pkdgrav
    struct Conversion {

        Float mass = Constants::M_sun;

        Float distance = Constants::au;

        Float velocity = Constants::au * 2._f * PI / (365.25_f * 86400._f);

    } conversion;

public:
    PkdgravOutput(const Path& fileMask, const std::string& runName, PkdgravParams&& params);

    virtual Path dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const Path& UNUSED(path), Storage& UNUSED(storage)) override {
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

class NullOutput : public Abstract::Output {
public:
    NullOutput()
        : Abstract::Output(Path("%d")) {}

    virtual Path dump(Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        return Path();
    }

    virtual Outcome load(const Path& UNUSED(path), Storage& UNUSED(storage)) override {
        return SUCCESS;
    }
};

NAMESPACE_SPH_END
