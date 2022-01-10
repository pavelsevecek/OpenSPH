#include "run/Config.h"
#include "io/FileSystem.h"
#include "objects/utility/Streams.h"
#include "system/Platform.h"

NAMESPACE_SPH_BEGIN

String quoted(const String& value) {
    return "\"" + value + "\"";
}

String unquoted(const String& value) {
    const Size n1 = value.find('"');
    const Size n2 = value.findLast('"');
    if (n1 >= n2) {
        throw ConfigException("Invalid string format");
    }
    return value.substr(n1 + 1, n2 - n1 - 1);
}

SharedPtr<ConfigNode> ConfigNode::addChild(const String& name) {
    return children.insert(name, makeShared<ConfigNode>());
}

SharedPtr<ConfigNode> ConfigNode::getChild(const String& name) {
    if (!children.contains(name)) {
        throw ConfigException("Node '" + name + "' not in config.");
    }
    return children[name];
}

Size ConfigNode::size() const {
    return entries.size();
}

void ConfigNode::enumerateChildren(Function<void(String, ConfigNode&)> func) {
    for (auto& child : children) {
        func(child.key(), *child.value());

        child.value()->enumerateChildren(func);
    }
}

void ConfigNode::write(const String& padding, std::wstringstream& source) {
    for (auto& element : entries) {
        source << padding << quoted(element.key()) << " = " << element.value() << "\n";
    }

    const String childPadding = padding + String::fromChar(' ', 2);
    for (auto& child : children) {
        source << padding << quoted(child.key()) << " [\n";
        child.value()->write(childPadding, source);
        source << padding << "]\n";
    }
}

void ConfigNode::read(ITextInputStream& source) {
    String line;
    while (source.readLine(line)) {
        line = line.trim(String::TrimFlag::SPACE | String::TrimFlag::TAB | String::TrimFlag::RETURN);
        if (line.empty()) {
            continue;
        } else if (line[line.size() - 1] == L']') {
            break;
        }

        Array<String> childAndBracket = split(line, L'[');
        if (childAndBracket.size() == 2) {
            SharedPtr<ConfigNode> child = makeShared<ConfigNode>();
            children.insert(unquoted(childAndBracket[0]), child);
            child->read(source);
        } else {
            Pair<String> keyAndValue = splitByFirst(line, '=');
            if (keyAndValue.empty()) {
                throw ConfigException("Invalid line format: " + line);
            }
            entries.insert(unquoted(keyAndValue[0]), keyAndValue[1]);
        }
    }
}

SharedPtr<ConfigNode> Config::addNode(const String& name) {
    return nodes.insert(name, makeShared<ConfigNode>());
}

SharedPtr<ConfigNode> Config::getNode(const String& name) {
    if (SharedPtr<ConfigNode> node = this->tryGetNode(name)) {
        return node;
    }
    throw ConfigException("Node '" + name + "' not in config.");
}


SharedPtr<ConfigNode> Config::tryGetNode(const String& name) {
    if (nodes.contains(name)) {
        return nodes[name];
    } else {
        return nullptr;
    }
}

void Config::read(ITextInputStream& source) {
    nodes.clear();
    while (source.good()) {
        SharedPtr<ConfigNode> node = makeShared<ConfigNode>();
        String line;
        source.readLine(line, L'[');
        line = line.trim(String::TrimFlag::SPACE | String::TrimFlag::END_LINE | String::TrimFlag::RETURN |
                         String::TrimFlag::TAB);
        if (line.empty() || line == L"\n") {
            continue;
        }
        node->read(source);
        nodes.insert(unquoted(line), node);
    }
}

String Config::write() {
    std::wstringstream source;
    const String padding = String::fromChar(' ', 2);
    for (auto& element : nodes) {
        source << quoted(element.key()) << " [\n";
        element.value()->write(padding, source);
        source << "]\n\n";
    }
    std::wstring wstr = source.str();
    return String::fromWstring(wstr);
}

void Config::save(const Path& path) {
    FileTextOutputStream stream(path);
    const String text = this->write();
    if (!stream.write(text)) {
        throw ConfigException("Cannot save file '{}'", path.string());
    }
}

void Config::load(const Path& path) {
    FileTextInputStream stream(path);
    if (!stream.good()) {
        throw ConfigException("Cannot open file '{}'", path.string());
    }
    this->read(stream);
}

void Config::enumerate(Function<void(String, ConfigNode&)> func) {
    for (auto& element : nodes) {
        func(element.key(), *element.value());

        element.value()->enumerateChildren(func);
    }
}

NAMESPACE_SPH_END
