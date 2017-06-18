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


/// Output saving data to binary data without loss of precision.
class BinaryOutput : public Abstract::Output {
private:
    std::string runName;

    static constexpr Size PADDING_SIZE = 228;

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
