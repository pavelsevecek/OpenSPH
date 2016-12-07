#pragma once

#include "storage/Storage.h"
#include <fstream>

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

        /// Loads data from the file into the storage. Can be used to continue simulation from saved snapshot.
        virtual void load(const std::string& path, Storage& storage) = 0;

    protected:
        std::string getFileName() const {
            std::string name         = fileMask;
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
public:
    TextOutput(const std::string& fileMask)
        : Abstract::Output(fileMask) {}

    /// \todo rewrite in less retarded way
    virtual std::string dump(Storage& storage, const Float time) override {
        const std::string fileName = getFileName();
        std::ofstream ofs(fileName);
        // print description
        ofs << "# SPH dump, time = " << time << std::endl;
        ofs << "# ";
        for (auto&& q : storage) {
            if (q.second.getOrderEnum() == OrderEnum::SECOND_ORDER) {
                ofs << std::setw(15) << getQuantityName(q.first) << std::setw(15)
                    << getDerivativeName(q.first);
            }
        }
        for (auto&& q : storage) {
            if (q.second.getOrderEnum() == OrderEnum::FIRST_ORDER) {
                ofs << std::setw(15) << getQuantityName(q.first);
            }
        }
        for (auto&& q : storage) {
            if (q.second.getOrderEnum() == OrderEnum::ZERO_ORDER) {
                ofs << std::setw(15) << getQuantityName(q.first);
            }
        }
        ofs << std::endl;

        // print data lines, starting with second-order quantities
        for (int i = 0; i < storage.getParticleCnt(); ++i) {
            iterate<VisitorEnum::SECOND_ORDER>(storage, [i, &ofs](auto&& v, auto&& dv, auto&& UNUSED(d2v)) {
                ofs << std::setw(15) << v[i] << std::setw(15) << dv[i];
            });
            iterate<VisitorEnum::FIRST_ORDER>(storage, [i, &ofs](auto&& v, auto&& UNUSED(dv)) {
                ofs << std::setw(15) << v[i];
            });
            iterate<VisitorEnum::ZERO_ORDER>(storage, [i, &ofs](auto&& v) { ofs << std::setw(15) << v[i]; });
            ofs << std::endl;
        }
        ofs.close();
        this->dumpNum++;
        return fileName;
    }

    virtual void load(const std::string& UNUSED(path), Storage& UNUSED(storage)) override { NOT_IMPLEMENTED; }
};

/// Extension of text output that runs given gnuplot script on dumped text data.
class GnuplotOutput : public TextOutput {
private:
    std::string scriptPath;

public:
    GnuplotOutput(const std::string& fileMask, const std::string& scriptPath)
        : TextOutput(fileMask)
        , scriptPath(scriptPath) {}

    virtual std::string dump(Storage& storage, const Float time) override {
        const std::string fileName       = TextOutput::dump(storage, time);
        const std::string nameWithoutExt = fileName.substr(0, fileName.find_last_of("."));
        const std::string command        = "gnuplot -e \"filename='" + nameWithoutExt + "'; time=" +
                                    std::to_string(time) + "\" " + scriptPath;
        system(command.c_str());
        return fileName;
    }
};


NAMESPACE_SPH_END
