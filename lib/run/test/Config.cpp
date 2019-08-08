#include "run/Config.h"
#include "catch.hpp"
#include "objects/containers/FlatMap.h"

using namespace Sph;

TEST_CASE("Config serialize", "[config]") {
    Config config;
    SharedPtr<ConfigNode> node = config.addNode("node");
    node->set("number", 5);
    node->set("string", std::string("test"));

    const std::string serialized = config.write();
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
    node2->set("text2", std::string("test"));
    node2->set("vector2", Vector(1._f, 2._f, 3._f));

    std::string serialized = config.write();

    std::stringstream ss(serialized);
    REQUIRE_NOTHROW(config.read(ss));

    FlatMap<std::string, ConfigNode*> readNodes;
    config.enumerate([&](const std::string& name, ConfigNode& node) { //
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
    REQUIRE(readNode2->get<std::string>("text2") == "test");
    REQUIRE(readNode2->get<Vector>("vector2") == Vector(1._f, 2._f, 3._f));
}

TEST_CASE("Config write and read children", "[config]") {
    Config config;
    SharedPtr<ConfigNode> rootNode = config.addNode("root");
    rootNode->set("rootValue", 1.5_f);

    SharedPtr<ConfigNode> childNode = rootNode->addChild("child");
    childNode->set("childValue", 5.1_f);

    std::string serialized = config.write();

    std::stringstream ss(serialized);
    REQUIRE_NOTHROW(config.read(ss));
    SharedPtr<ConfigNode> readRootNode = config.getNode("root");
    REQUIRE(readRootNode->size() == 1);
    REQUIRE(readRootNode->get<Float>("rootValue") == 1.5_f);
    REQUIRE_THROWS_AS(readRootNode->get<Float>("childValue"), ConfigException);

    SharedPtr<ConfigNode> readChildNode = readRootNode->getChild("child");
    REQUIRE(readChildNode->size() == 1);
    REQUIRE(readChildNode->get<Float>("childValue") == 5.1_f);
}
