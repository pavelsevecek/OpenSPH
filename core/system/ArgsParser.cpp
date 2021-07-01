#include "system/ArgsParser.h"
#include "objects/utility/StringUtils.h"

NAMESPACE_SPH_BEGIN

ArgParser::ArgParser(ArrayView<const ArgDesc> args) {
    descs.pushAll(args.begin(), args.end());
    descs.push(ArgDesc{ "h", "help", ArgEnum::NONE, "Prints this help", [this] {
                           StringLogger logger;
                           logger.write("List of parameters:");
                           printHelp(logger);
                           throw HelpException(logger.toString());
                       } });
}

void ArgParser::parse(const int argc, char** argv) {
    params.clear();
    for (int i = 1; i < argc; ++i) {
        const std::string name = argv[i];
        auto iter =
            std::find_if(descs.begin(), descs.end(), [&name](ArgDesc& desc) { return desc.matches(name); });
        if (iter != descs.end()) {
            if (iter->type == ArgEnum::NONE) {
                parseValuelessArg(*iter);
            } else {
                if (i + 1 == argc) {
                    throw ArgError("Missing parameter value: " + name);
                }
                parseValueArg(*iter, argv[i + 1]);
                ++i;
            }
        } else {
            throw ArgError("Unknown parameter: " + name);
        }
    }
}

INLINE std::string toString(const ArgEnum type) {
    switch (type) {
    case ArgEnum::INT:
        return "INT";
    case ArgEnum::FLOAT:
        return "FLOAT";
    case ArgEnum::STRING:
        return "STRING";
    default:
        NOT_IMPLEMENTED;
    }
}

void ArgParser::printHelp(ILogger& logger) {
    for (ArgDesc& arg : descs) {
        std::string line = "-" + arg.shortName + ", --" + arg.longName + " ";
        if (arg.type != ArgEnum::NONE) {
            line += toString(arg.type);
        }
        /// \todo make padding adaptive
        line += std::string(max(1, 35 - int(line.size())), ' ');

        const std::string desc = setLineBreak(arg.desc, 40);
        line += replaceAll(desc, "\n", "\n" + std::string(35, ' '));
        logger.write(line);
    }
}

void ArgParser::parseValuelessArg(const ArgDesc& desc) {
    if (params.contains(desc.shortName)) {
        throw ArgError("Duplicate parameter: " + desc.shortName);
    }
    params.insert(desc.shortName, true);

    if (desc.callback) {
        desc.callback();
    }
}

void ArgParser::parseValueArg(const ArgDesc& desc, const std::string& value) {
    if (params.contains(desc.shortName)) {
        throw ArgError("Duplicate parameter: " + desc.shortName);
    }

    switch (desc.type) {
    case ArgEnum::INT:
        insertArg<int>(desc.shortName, value);
        break;
    case ArgEnum::FLOAT:
        insertArg<Float>(desc.shortName, value);
        break;
    case ArgEnum::STRING:
        insertArg<std::string>(desc.shortName, value);
        break;
    default:
        STOP;
    }

    if (desc.callback) {
        desc.callback();
    }
}

template <typename TValue>
void ArgParser::insertArg(const std::string& name, const std::string& textValue) {
    const Optional<TValue> value = fromString<TValue>(textValue);
    if (!value) {
        throw ArgError("Cannot parse value of parameter " + name);
    }
    params.insert(name, value.value());
}

void ArgParser::throwIfUnknownArg(const std::string& name) const {
    auto iter = std::find_if(descs.begin(), descs.end(), [&name](const ArgDesc& desc) { //
        return desc.shortName == name;
    });
    if (iter == descs.end()) {
        throw ArgError("Unknown argument " + name);
    }
}

NAMESPACE_SPH_END
