#pragma once

#include "run/Node.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_CHAISCRIPT

class ScriptNode : public INode {
private:
    Path file;
    Array<SharedPtr<JobNode>> nodes;

public:
    ScriptNode(const Path& file, Array<SharedPtr<JobNode>>&& nodes)
        : file(file)
        , nodes(std::move(nodes)) {}

    virtual void run(const RunSettings& global, IJobCallbacks& callbacks) override;
};

#endif

NAMESPACE_SPH_END
