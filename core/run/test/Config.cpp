#include "run/Config.h"
#include "catch.hpp"
#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "objects/containers/FlatMap.h"
#include "objects/utility/Streams.h"

using namespace Sph;

TEST_CASE("Config serialize", "[config]") {
    Config config;
    SharedPtr<ConfigNode> node = config.addNode("node");
    node->set("number", 5);
    node->set("string", String("test"));

    const String serialized = config.write();
    REQUIRE(serialized == R"("node" [
  "number" = 5
  "string" = "test"
]

)");
}

TEST_CASE("Config write and read", "[config]") {
    Config config;
    SharedPtr<ConfigNode> node1 = config.addNode("node1");
    node1->set("value1", 5.31_f);
    node1->set("count1", 3);
    node1->set("path1", Path("test"));

    SharedPtr<ConfigNode> node2 = config.addNode("node2");
    node2->set("value2", 3.14_f);
    node2->set("text2", String(L"test \u03C1"));
    node2->set("vector2", Vector(1._f, 2._f, 3._f));

    String serialized = config.write();

    StringTextInputStream ss(serialized);
    REQUIRE_NOTHROW(config.read(ss));

    FlatMap<String, ConfigNode*> readNodes;
    config.enumerate([&](const String& name, ConfigNode& node) { //
        readNodes.insert(name, &node);
    });

    REQUIRE(readNodes.size() == 2);
    REQUIRE(readNodes.contains("node1"));
    REQUIRE(readNodes.contains("node2"));

    ConfigNode* readNode1 = readNodes["node1"];
    REQUIRE(readNode1->get<Float>("value1") == 5.31_f);
    REQUIRE(readNode1->get<int>("count1") == 3);
    REQUIRE(readNode1->get<Path>("path1") == Path("test"));
    REQUIRE_THROWS_AS(readNode1->get<Float>("dummy"), ConfigException);
    REQUIRE_THROWS_AS(readNode1->get<int>("value1"), ConfigException);

    ConfigNode* readNode2 = readNodes["node2"];
    REQUIRE(readNode2->get<Float>("value2") == 3.14_f);
    REQUIRE(readNode2->get<String>("text2") == L"test \u03C1");
    REQUIRE(readNode2->get<Vector>("vector2") == Vector(1._f, 2._f, 3._f));
}

TEST_CASE("Config write and read children", "[config]") {
    Config config;
    SharedPtr<ConfigNode> rootNode = config.addNode("root");
    rootNode->set("rootValue", 1.5_f);

    SharedPtr<ConfigNode> childNode = rootNode->addChild("child");
    childNode->set("childValue", 5.1_f);

    String serialized = config.write();

    StringTextInputStream ss(serialized);
    REQUIRE_NOTHROW(config.read(ss));
    SharedPtr<ConfigNode> readRootNode = config.getNode("root");
    REQUIRE(readRootNode->size() == 1);
    REQUIRE(readRootNode->get<Float>("rootValue") == 1.5_f);
    REQUIRE_THROWS_AS(readRootNode->get<Float>("childValue"), ConfigException);

    SharedPtr<ConfigNode> readChildNode = readRootNode->getChild("child");
    REQUIRE(readChildNode->size() == 1);
    REQUIRE(readChildNode->get<Float>("childValue") == 5.1_f);
}

TEST_CASE("Config file I/O", "[config]") {
    Config config;
    SharedPtr<ConfigNode> node = config.addNode("node");
    node->set("value", 5.31_f);
    node->set("count", 3);

    String text(L"\u03B1\u03B2\u03B3");
    node->set("text", text);

    RandomPathManager manager;
    Path path = manager.getPath(L"\u03B1sph"); // must work with unicode paths
    REQUIRE_NOTHROW(config.save(path));
    REQUIRE(FileSystem::pathExists(path));
    REQUIRE(FileSystem::fileSize(path) > 20);

    Config loaded;
    REQUIRE_NOTHROW(loaded.load(path));
    node = loaded.getNode("node");
    REQUIRE(node->get<Float>("value") == 5.31_f);
    REQUIRE(node->get<int>("count") == 3);
    REQUIRE(node->get<String>("text") == text);

    REQUIRE_THROWS(loaded.load(Path("nonexistent")));
}
