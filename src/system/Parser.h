#pragma once

/// \todo Currently obsolete

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Tuple.h"
#include "objects/wrappers/Variant.h"
#include <algorithm>
#include <fstream>

NAMESPACE_SPH_BEGIN


template <typename T0, typename... TS>
struct SaveArraysImpl {
    static void action(const std::string& path, const Array<T0>& array, const Array<TS>&... others) {
        std::ofstream ofs;
        try {
            ofs.open(path);
            for (int j = 0; j < array.getSize(); ++j) {
                printLine(ofs, j, array, others...);
                ofs << std::endl;
            }
            ofs.close();
        } catch (...) {
            std::cout << "Cannot save" << std::endl;
        }
    }

    static void printLine(std::ofstream& ofs,
                          const int line,
                          const Array<T0>& array,
                          const Array<TS>&... others) {
        ;
        ofs << array[line] << "   ";
        SaveArraysImpl<TS...>::printLine(ofs, line, others...);
    }
};

template <typename T0>
struct SaveArraysImpl<T0> {
    static void action(const std::string& path, const Array<T0>& array) {
        std::ofstream ofs;
        try {
            ofs.open(path);
            for (int j = 0; j < array.getSize(); ++j) {
                ofs << array[j] << "   " << std::endl;
            }
            ofs.close();
        } catch (...) {
            std::cout << "Cannot save" << std::endl;
        }
    }

    static void printLine(std::ofstream& ofs, const int line, const Array<T0>& array) {
        ofs << array[line] << "   ";
    }
};

enum class SavingOptions { NO_UNITS };

/// Loading/saving array of generic data from/into textfiles. A column of file are determined
/// by template parameters of the class. Can save/load any kind of data as long as operators >> and << exist.
/// TODO: ignore '#' lines when loading
template <typename... TArgs>
class DataFile : public Object {
public:
    DataFile() = default;

    Array<Tuple<TArgs...>> load(const std::string& path) {
        std::ifstream ifs;
        try {
            ifs.open(path);
            const int lineCount =
                std::count(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>(), '\n');
            ifs.seekg(0);
            Array<Tuple<TArgs...>> array(lineCount);
            for (int i = 0; i < lineCount; ++i) {
                auto& tuple = array[i];
                forEach(tuple, [&ifs](auto& value) { ifs >> value; });
            }
            ifs.close();
            return array;
        } catch (...) {
            // whatever
        }
        return Array<Tuple<TArgs...>>(0);
    }

    /// Save array of tuples with the same arguments as the file
    void save(const Array<Tuple<TArgs...>>& array, const std::string& path) {
        std::ofstream ofs;
        try {
            ofs.open(path);
            for (int i = 0; i < array.getSize(); ++i) {
                forEach(array[i], [&ofs](auto& value) { ofs << std::setw(10) << value; });
                ofs << std::endl;
            }
            ofs.close();
        } catch (...) {
            std::cout << "Cannot save" << std::endl;
        }
    }

    /// Save an array of any type T that has [] operator and static constexpr int 'dimensions'.
    /// Taylored for easily saving array of vectors.
    template <typename T>
    void save(const Array<T>& array,
              const std::string& path,
              SavingOptions options = SavingOptions::NO_UNITS) {
        std::ofstream ofs;
        try {
            ofs.open(path);
            for (int i = 0; i < array.getSize(); ++i) {
                /*for (int j = 0; j < T::dimensions; ++j) {
                    if (options == SavingOptions::NO_UNITS) {
                        ofs << std::fixed << std::setw(11) << std::setprecision(6)
                            << Base<T>(array[i][j]);
                    } else {
                        ofs << std::fixed << std::setw(11) << std::setprecision(6) << array[i][j];
                    }
                }*/
                ofs << std::endl;
            }
            ofs.close();
        } catch (...) {
            std::cout << "Cannot save" << std::endl;
        }
    }

    /// Tuple of arrays
    void save(const std::string& path, const Array<TArgs>&... arrays) {
        SaveArraysImpl<TArgs...>::action(path, arrays...);
    }
};

/// Generic function for converting types to user-readable string
template <typename T>
std::string toString(T&& value) {
    std::stringstream ss;
    ss << std::forward<T>(value);
    return ss.str();
}

template <>
std::string toString<bool>(bool&& value) {
    if (value) {
        return "true";
    } else {
        return "false";
    }
}

template <typename T>
Optional<T> fromString(const std::string& str) {
    std::stringstream ss;
    ss << str;
    if (!ss.good()) {
        return NOTHING;
    }
    T value;
    ss >> value;
    return value;
}

class ConfigBlock;

struct ConfigVisitor {
    std::ofstream& ofs;
    ConfigBlock* parent;
    int depth;

    // output of primitive types
    template <typename T>
    void operator()(T&& value) {
        ofs << std::forward<T>(value);
    }

    void operator()(std::shared_ptr<ConfigBlock>& subBlock);

    ConfigVisitor(std::ofstream& ofs, ConfigBlock* parent, int depth)
        : ofs(ofs)
        , parent(parent)
        , depth(depth) {}
};

class ConfigBlock : public Object {
    friend class ConfigVisitor;

private:
    /// Value inside a can be a primitive type or a sub-block. This block owns all sub-blocks and will destroy
    /// them in destructor.
    using Value = Variant<int, float, double, bool, std::string, std::shared_ptr<ConfigBlock>>;

    Array<std::string> names;
    Array<Value> values;

    bool writeImpl(std::ofstream& ofs, ConfigBlock* block, int depth = 1) {
        ofs << "{" << std::endl;
        for (int i = 0; i < block->names.getSize(); ++i) {
            ofs << std::setw(depth * 10) << block->names[i] << " = ";
            block->values[i].execute(ConfigVisitor(ofs, block, depth));
            ofs << std::endl;
        }
        ofs << std::setw((depth-1) * 10) << "}" << std::endl;
        return true;
    }

public:
    ConfigBlock()
        : names(0, 100)
        , values(0, 100) {}

    template<typename T>
    void add(const std::string& name, T&& value) {
        names.push(name);
        values.push(std::forward<T>(value));
    }

    bool write(const std::string& path) {
        std::ofstream ofs;
        try {
            ofs.open(path);
            writeImpl(ofs, this);
            ofs.close();
        } catch (...) {
            return false;
        }
        return true;
    }
};


void ConfigVisitor::operator()(std::shared_ptr<ConfigBlock>& subBlock) {
    // recursive output
    parent->writeImpl(ofs, subBlock.get(), depth+1);
}

class ConfigValue : public Object {
private:
    std::string name;


    static Array<std::string> split(const std::string& s, const char delim) {
        std::stringstream ss(s);
        std::string item;
        const int cnt = std::count(s.begin(), s.end(), delim);
        Array<std::string> array(0, cnt + 1);
        while (getline(ss, item, delim)) {
            array.push(item);
        }
        return array;
    }

    static std::string trim(const std::string& s) {
        int i = 0, j = s.length() - 1;
        while (i < int(s.length()) && s[i] == ' ') {
            i++;
        }
        while (j >= 0 && s[j] == ' ') {
            j--;
        }
        return s.substr(i, j - i);
    }

public:
    /// Constructs the value from a line of config file
    ConfigValue(const std::string& line) {
        Array<std::string> parts = split(line, '=');
        if (parts.getSize() != 2) {
            return;
        }
        for (auto& s : parts) {
            trim(s);
        }
        name = parts[0];

        // std::cout << parts[0] << "=" << parts[1] << std::endl;
    }
};

/*
    ConfigBlock block;
    block.add("kernel", std::string("m4"));
    block.add("eos", std::string("Tillotson"));
    block.add("neigh_eta", 1.5f);

    ConfigBlock av;
    av.add("alpha", 1.5f);
    av.add("beta", 3.f);

    block.add("av", std::make_shared<ConfigBlock>(std::move(av)));

    block.write("config.txt");

    DataFile<Evt::T> output;
    output.save(get<Indices::R>(core.model), "vecs.txt");*/

NAMESPACE_SPH_END
