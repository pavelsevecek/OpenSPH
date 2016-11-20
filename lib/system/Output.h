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

        /// Saves data from particle storage into the file.
        virtual void dump(Storage& storage) = 0;

        /// Loads data from the file into the storage. Can be used to continue simulation from saved snapshot.
        virtual void load(const std::string& path, Storage& storage) = 0;

    protected:
        std::string getFileName() const {
            std::string name         = fileMask;
            std::string::size_type n = fileMask.find("%d", 0);
            ASSERT(n != std::string::npos);
            std::ostringstream ss;
            ss << std::setw(3) << std::setfill('0') << dumpNum;
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
    virtual void dump(Storage& storage) override {
        std::ofstream ofs(getFileName());
        const int size = storage.getQuantityCnt();
        // print description
        ofs << "# ";
        for (int i=0; i<size; ++i) {
            if (storage[i].getTemporalEnum() == TemporalEnum::SECOND_ORDER) {
                ofs << std::setw(15) << getQuantityName(storage[i].getKey()) << std::setw(15)
                    << getDerivativeName(storage[i].getKey());
            }
        }
        for (int i=0; i<size; ++i) {
            if (storage[i].getTemporalEnum() == TemporalEnum::FIRST_ORDER) {
                ofs << std::setw(15) << getQuantityName(storage[i].getKey());
            }
        }
        for (int i=0; i<size; ++i) {
            if (storage[i].getTemporalEnum() == TemporalEnum::ZERO_ORDER) {
                ofs << std::setw(15) << getQuantityName(storage[i].getKey());
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
    }

    virtual void load(const std::string& UNUSED(path), Storage& UNUSED(storage)) override { NOT_IMPLEMENTED; }
};


NAMESPACE_SPH_END
