#pragma once

#include "objects/containers/Map.h"
#include "objects/utility/StringUtils.h"
#include "objects/wrappers/SharedPtr.h"
#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN

enum class ArgEnum {
    INT,
    FLOAT,
    STRING,
};

INLINE std::string toString(const ArgEnum type) {
    switch (type) {
    case ArgEnum::INT:
        return "Int";
    case ArgEnum::FLOAT:
        return "Float";
    case ArgEnum::STRING:
        return "String";
    default:
        NOT_IMPLEMENTED;
    }
}

using ArgValue = Variant<int, float, std::string>;

/// Exception thrown if the arguments are invalid
class ArgsError : public std::exception {
private:
    std::string message;

public:
    explicit ArgsError(const std::string& message)
        : message(message) {}

    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};

class IArg : public Polymorphic {
public:
    /// \brief Tries to parse this argument.
    ///
    /// If the argument matches, the parsed value is returned and the input arrayview is moved to the next
    /// argument. Otherwise, NOTHING is returned.
    virtual Optional<ArgValue> tryParse(ArrayView<char*>& args) const = 0;

    virtual bool isNamed() const = 0;

    virtual bool isOptional() const = 0;
};

static Optional<ArgValue> tryParseValue(const ArgEnum type, const std::string& s) {
    switch (type) {
    case ArgEnum::INT: {
        Optional<int> value = fromString<int>(s);
        if (value) {
            return value.value();
        } else {
            return NOTHING;
        }
    }
    case ArgEnum::FLOAT: {
        Optional<float> value = fromString<float>(s);
        if (value) {
            return value.value();
        } else {
            return NOTHING;
        }
    }
    case ArgEnum::STRING: {
        Optional<std::string> value = fromString<std::string>(s);
        if (value) {
            return value.value();
        } else {
            return NOTHING;
        }
    }
    default:
        NOT_IMPLEMENTED;
    }
}

class NamedArg : public IArg {
private:
    ArgEnum _type;
    std::string _shortName;
    std::string _longName;

public:
    NamedArg(const ArgEnum type, const std::string& shortName, const std::string& longName)
        : _type(type)
        , _shortName(shortName)
        , _longName(longName) {}

    virtual Optional<ArgValue> tryParse(ArrayView<char*>& args) const override {
        if (args.size() < 2) {
            throw ArgsError(std::string("Missing value of parameter ") + args[0]);
        }
        if (args[0] == _shortName || args[0] == _longName) {
            Optional<ArgValue> parsedValue = tryParseValue(_type, args[1]);
            if (parsedValue) {
                args = args.subset(2, args.size() - 2);
                return parsedValue;
            } else {
                throw ArgsError("Invalid parameter, expected " + toString(_type) + ", got + " + args[1]);
            }
        }
        return NOTHING;
    }

    virtual bool isNamed() const override {
        return true;
    }

    virtual bool isOptional() const override {
        return true;
    }
};

enum class OptionalEnum {
    OPTIONAL,
    MANDATORY,
};

class UnnamedArg : public IArg {
private:
    ArgEnum _type;
    OptionalEnum _optional;

public:
    UnnamedArg(const ArgEnum type, const OptionalEnum optional)
        : _type(type)
        , _optional(optional) {}

    virtual Optional<ArgValue> tryParse(ArrayView<char*>& args) const override {
        ASSERT(!args.empty());
        Optional<ArgValue> parsedValue = tryParseValue(_type, args[0]);
        if (parsedValue) {
            args = args.subset(1, args.size() - 1);
            return parsedValue;
        } else if (_optional == OptionalEnum::MANDATORY) {
            throw ArgsError("Invalid parameter, expected " + toString(_type) + ", got " + args[0]);
        }
        return NOTHING;
    }

    virtual bool isNamed() const override {
        return false;
    }

    virtual bool isOptional() const override {
        return _optional == OptionalEnum::OPTIONAL;
    }
};

template <typename TEnum>
class Arg {
private:
    TEnum _id;
    SharedPtr<IArg> _impl;

public:
    /// \brief Creates a named argument.
    Arg(const TEnum id, const ArgEnum type, const std::string& shortName, const std::string& longName = "")
        : _id(id) {
        _impl = makeShared<NamedArg>(type, shortName, longName);
    }

    /// \brief Creates an unnamed argument.
    Arg(const TEnum id, const ArgEnum type, const OptionalEnum optional)
        : _id(id) {
        _impl = makeShared<UnnamedArg>(type, optional);
    }

    Optional<ArgValue> tryParse(ArrayView<char*>& args) {
        return _impl->tryParse(args);
    }

    TEnum id() const {
        return _id;
    }

    bool isNamed() const {
        return _impl->isNamed();
    }

    bool isOptional() const {
        return _impl->isOptional();
    }
};

template <typename TEnum>
class ArgsParser {
private:
    Array<Arg<TEnum>> _namedArgs;
    Array<Arg<TEnum>> _unnamedArgs;

public:
    ArgsParser(Array<Arg<TEnum>>&& args) {
        for (auto& a : args) {
            if (a.isNamed()) {
                _namedArgs.emplaceBack(std::move(a));
            } else {
                _unnamedArgs.emplaceBack(std::move(a));
            }
        }
    }

    Map<TEnum, ArgValue> parse(int argc, char** argv) {
        Map<TEnum, ArgValue> map;
        if (argc == 0) {
            throw ArgsError("Empty parameter list");
        }
        ArrayView<char*> argsView(argv + 1, argc - 1);

        // keep track of already parsed args to avoid duplicates
        Array<bool> namedParsed(_namedArgs.size());
        namedParsed.fill(false);
        Array<bool> unnamedParsed(_unnamedArgs.size());
        unnamedParsed.fill(false);

        while (!argsView.empty()) {
            bool matchFound = false;
            if (argsView[0][0] == '-') {
                for (Size i = 0; i < _namedArgs.size(); ++i) {
                    Arg<TEnum>& a = _namedArgs[i];
                    Optional<ArgValue> parsedValue = a.tryParse(argsView);
                    if (parsedValue) {
                        if (!namedParsed[i]) {
                            map.insert(a.id(), parsedValue.value());
                            namedParsed[i] = true;
                            matchFound = true;
                            break;
                        } else {
                            throw ArgsError(std::string("Duplicate parameter: ") + argsView[0]);
                        }
                    }
                }
                if (!matchFound) {
                    throw ArgsError(std::string("Unknown parameter: ") + argsView[0]);
                }
            } else {
                for (Size i = 0; i < _unnamedArgs.size(); ++i) {
                    if (unnamedParsed[i]) {
                        continue;
                    }
                    Arg<TEnum>& a = _unnamedArgs[i];
                    Optional<ArgValue> parsedValue = a.tryParse(argsView);
                    if (parsedValue) {
                        map.insert(a.id(), parsedValue.value());
                        matchFound = true;
                        unnamedParsed[i] = true;
                        break;
                    }
                }
                if (!matchFound) {
                    // too many unnamed parameters?
                    throw ArgsError(std::string("Invalid parameter: ") + argsView[0]);
                }
            }
        }

        // check that all mandatory unnamed parameters has been parsed
        for (Size i = 0; i < _unnamedArgs.size(); ++i) {
            if (!_unnamedArgs[i].isNamed() && !_unnamedArgs[i].isOptional() && !unnamedParsed[i]) {
                throw ArgsError("Too few parameters");
            }
        }
        return map;
    }
};

NAMESPACE_SPH_END
