#pragma once

#include "objects/wrappers/Outcome.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class OutputFile {
private:
    mutable Size dumpNum = 0;
    std::string pathMask;

public:
    OutputFile() = default;

    OutputFile(const std::string& pathMask);

    /// Returns path to the next output file, incrementing the internal counter
    std::string getNextPath() const;
};

/// Interface for saving quantities of SPH particles to a file. Saves all values in the storage, and also 1st
/// derivatives for 2nd-order quantities.
namespace Abstract {
    class Column;

    class Output : public Polymorphic {
    protected:
        OutputFile paths;
        Array<AutoPtr<Abstract::Column>> columns;

    public:
        /// Constructs output given the file name of the output. The name must contain '%d', which will be
        /// replaced by the dump number, starting from 0.
        Output(const std::string& fileMask);

        ~Output();

        /// Adds an element to output.
        void add(AutoPtr<Abstract::Column>&& columns);

        /// Saves data from particle storage into the file. Returns the filename of the dump.
        virtual std::string dump(Storage& storage, const Statistics& stats) = 0;

        /// Loads data from the file into the storage. This will remove any data previously stored in storage.
        /// Can be used to continue simulation from saved snapshot.
        virtual Outcome load(const std::string& path, Storage& storage) = 0;
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
    Flags<Options> flags;

public:
    TextOutput(const std::string& fileMask, const std::string& runName, const Flags<Options> flags);

    virtual std::string dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const std::string& path, Storage& storage) override;
};

/// Extension of text output that runs given gnuplot script on dumped text data.
class GnuplotOutput : public TextOutput {
private:
    std::string scriptPath;

public:
    GnuplotOutput(const std::string& fileMask,
        const std::string& runName,
        const std::string& scriptPath,
        const Flags<Options> flags)
        : TextOutput(fileMask, runName, flags)
        , scriptPath(scriptPath) {}

    virtual std::string dump(Storage& storage, const Statistics& stats) override;
};


/// Output saving data to binary data without loss of precision.
class BinaryOutput : public Abstract::Output {
private:
    std::string runName;

public:
    BinaryOutput(const std::string& fileMask, const std::string& runName);

    virtual std::string dump(Storage& storage, const Statistics& stats) override;

    virtual Outcome load(const std::string& path, Storage& storage) override;
};

class NullOutput : public Abstract::Output {
public:
    NullOutput()
        : Abstract::Output("%d") {}

    virtual std::string dump(Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        return "";
    }

    virtual Outcome load(const std::string& UNUSED(path), Storage& UNUSED(storage)) override {
        return true;
    }
};

NAMESPACE_SPH_END
