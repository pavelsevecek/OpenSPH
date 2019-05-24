#include "run/Config.h"
#include "io/FileSystem.h"
#include "system/Platform.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

std::string quoted(const std::string& value) {
    return "\"" + value + "\"";
}

std::string unquoted(const std::string& value) {
    const std::size_t n1 = value.find_first_of('"');
    const std::size_t n2 = value.find_last_of('"');
    if (n1 >= n2) {
        throw ConfigException("Invalid string format");
    }
    return value.substr(n1 + 1, n2 - n1 - 1);
}

SharedPtr<ConfigNode> ConfigNode::addChild(const std::string& name) {
    return children.insert(name, makeShared<ConfigNode>());
}

SharedPtr<ConfigNode> ConfigNode::getChild(const std::string& name) {
    if (!children.contains(name)) {
        throw ConfigException("Node '" + name + "' not in config.");
    }
    return children[name];
}

Size ConfigNode::size() const {
    return entries.size();
}

void ConfigNode::enumerateChildren(Function<void(std::string, ConfigNode&)> func) {
    for (auto& child : children) {
        func(child.key, *child.value);

        child.value->enumerateChildren(func);
    }
}

void ConfigNode::write(const std::string& padding, std::stringstream& source) {
    for (auto& element : entries) {
        source << padding << quoted(element.key) << " = " << element.value << "\n";
    }

    const std::string childPadding = padding + std::string(2, ' ');
    for (auto& child : children) {
        source << padding << quoted(child.key) << " [\n";
        child.value->write(childPadding, source);
        source << padding << "]\n";
    }
}

void ConfigNode::read(std::stringstream& source) {
    std::string line;
    while (std::getline(source, line)) {
        if (line.empty()) {
            continue;
        } else if (line.back() == ']') {
            break;
        }

        Array<std::string> childAndBracket = split(line, '[');
        if (childAndBracket.size() == 2) {
            SharedPtr<ConfigNode> child = makeShared<ConfigNode>();
            children.insert(unquoted(childAndBracket[0]), child);
            child->read(source);
        } else {
            Pair<std::string> keyAndValue = splitByFirst(line, '=');
            if (keyAndValue.empty()) {
                throw ConfigException("Invalid line format: " + line);
            }
            entries.insert(unquoted(keyAndValue[0]), keyAndValue[1]);
        }
    }
}

SharedPtr<ConfigNode> Config::addNode(const std::string& name) {
    return nodes.insert(name, makeShared<ConfigNode>());
}

SharedPtr<ConfigNode> Config::getNode(const std::string& name) {
    if (SharedPtr<ConfigNode> node = this->tryGetNode(name)) {
        return node;
    }
    throw ConfigException("Node '" + name + "' not in config.");
}


SharedPtr<ConfigNode> Config::tryGetNode(const std::string& name) {
    if (nodes.contains(name)) {
        return nodes[name];
    } else {
        return nullptr;
    }
}

void Config::read(std::stringstream& source) {
    nodes.clear();
    while (!source.eof()) {
        SharedPtr<ConfigNode> node = makeShared<ConfigNode>();
        std::string name;
        std::getline(source, name, '[');
        if (name.empty() || name == "\n") {
            continue;
        }
        node->read(source);
        nodes.insert(unquoted(name), node);
    }
}

std::string Config::write() {
    std::stringstream source;
    const std::string padding(2, ' ');
    for (auto& element : nodes) {
        source << quoted(element.key) << " [\n";
        element.value->write(padding, source);
        source << "]\n\n";
    }
    return source.str();
}

void Config::save(const Path& path) {
    std::ofstream ofs(path.native());
    ofs << this->write();
}

void Config::load(const Path& path) {
    std::ifstream ifs(path.native());
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    this->read(buffer);
}

void Config::enumerate(Function<void(std::string, ConfigNode&)> func) {
    for (auto& element : nodes) {
        func(element.key, *element.value);

        element.value->enumerateChildren(func);
    }
}

NAMESPACE_SPH_END
