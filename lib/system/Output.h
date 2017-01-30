#pragma once

#include "quantities/Storage.h"
#include "objects/wrappers/Outcome.h"

NAMESPACE_SPH_BEGIN

/// Interface for saving quantities of SPH particles to a file. Saves all values in the storage, and also 1st
/// derivatives for 2nd-order quantities.
namespace Abstract {
    class Output : public Polymorphic {
    protected:
        int dumpNum = 0; // number of saved files
        std::string fileMask;

    public:
        /// Constructs output given the file name of the output. The name must contain '%d', which will be
        /// replaced by the dump number, starting from 0.
        Output(const std::string& fileMask)
            : fileMask(fileMask) {
            ASSERT(fileMask.find("%d") != std::string::npos);
        }

        /// Saves data from particle storage into the file. Returns the filename of the dump.
        virtual std::string dump(Storage& storage, const Float time) = 0;

        /// Loads data from the file into the storage. This will remove any data previously stored in storage.
        /// Can be used to continue simulation from saved snapshot.
        virtual Outcome load(const std::string& path, Storage& storage) = 0;

    protected:
        std::string getFileName() const {
            std::string name = fileMask;
            std::string::size_type n = fileMask.find("%d", 0);
            ASSERT(n != std::string::npos);
            std::ostringstream ss;
            ss << std::setw(4) << std::setfill('0') << dumpNum;
            name.replace(n, 2, ss.str());
            return name;
        }
    };
}

/// Output saving data to text (human readable) file.
class TextOutput : public Abstract::Output {
private:
    std::string runName;
    Array<QuantityIds> columns;

public:
    TextOutput(const std::string& fileMask, const std::string& runName, Array<QuantityIds>&& columns);

    virtual std::string dump(Storage& storage, const Float time) override;

    virtual Outcome load(const std::string& UNUSED(path), Storage& UNUSED(storage)) override { NOT_IMPLEMENTED; }
};

/// Extension of text output that runs given gnuplot script on dumped text data.
class GnuplotOutput : public TextOutput {
private:
    std::string scriptPath;

public:
    GnuplotOutput(const std::string& fileMask,
        const std::string& runName,
        Array<QuantityIds>&& columns,
        const std::string& scriptPath)
        : TextOutput(fileMask, runName, std::move(columns))
        , scriptPath(scriptPath) {}

    virtual std::string dump(Storage& storage, const Float time) override;
};


/// Output saving data to binary data without loss of precision.
class BinaryOutput : public Abstract::Output {
private:
    std::string runName;

public:
    BinaryOutput(const std::string& fileMask, const std::string& runName);

    virtual std::string dump(Storage& storage, const Float time) override;

    virtual Outcome load(const std::string& path, Storage& storage) override;
};


NAMESPACE_SPH_END
