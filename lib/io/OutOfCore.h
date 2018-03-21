#pragma once

/// \file OutOfCore.h
/// \brief Saving and accessing array of elements on the disk
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "io/FileSystem.h"
#include "io/Logger.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

template <typename T>
class DiskArray {
private:
    Path path;

    static_assert(std::is_pod<T>::value, "DiskArray can only be used for POD types");

public:
    explicit DiskArray(const Path& path)
        : path(path) {}

    /// \brief Adds a value into the array.
    void push(const T& value) {
        // std::unique_lock<FileSystem::FileLock> guard(lock);
        std::ofstream ofs(path.native(), std::ofstream::app | std::ofstream::binary);
        T data = value; // need the copy to get rid of const
        if (!ofs.write(reinterpret_cast<char*>(std::addressof(data)), sizeof(T))) {
            throw IoError("Failed to write data into the DiskArray file");
        }
    }

    /// \brief Returns the number of items in the array.
    Size size() const {
        if (!FileSystem::pathExists(path)) {
            // no items have been pushed yet, ergo zero size
            return 0;
        }
        std::ifstream ifs(path.native(), std::ifstream::in | std::ifstream::binary);
        if (!ifs.seekg(0, std::ifstream::end)) {
            throw IoError("File access error of DiskArray file");
        }
        const Size pos = ifs.tellg();
        if (pos % sizeof(T) != 0) {
            throw IoError("Invalid data of DiskArray file");
        }
        return pos / sizeof(T);
    }

    /// \brief Checks if the array is empty
    bool empty() const {
        return this->size() == 0;
    }

    /// \brief Returns the elements with given index from the array
    T operator[](const Size idx) const {
        std::ifstream ifs(path.native(), std::ifstream::in | std::ifstream::binary);
        if (!ifs.seekg(idx * sizeof(T), std::ifstream::beg)) {
            throw IoError("Item with given index is not in the DiskArray file");
        }
        T value;
        if (!ifs.read(reinterpret_cast<char*>(std::addressof(value)), sizeof(T))) {
            throw IoError("Failed to read the data from DiskArray file");
        }
        return value;
    }

    /// \brief Loads all elements of the file to memory and returns them as Array
    Array<T> getAll() const {
        if (this->empty()) {
            return {};
        }
        std::ifstream ifs(path.native(), std::ifstream::in | std::ifstream::binary);
        Array<T> data;
        T value;
        while (ifs.read(reinterpret_cast<char*>(std::addressof(value)), sizeof(T))) {
            data.push(value);
        }
        return data;
    }

    /// \brief Removes all elements from the array.
    ///
    /// This removes the file from the disk.
    void clear() {
        FileSystem::removePath(path);
    }
};

NAMESPACE_SPH_END
