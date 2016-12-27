#pragma once

#include "gui/Common.h"
#include "objects/wrappers/NonOwningPtr.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Renderer : public Observable {
    public:
        virtual void draw(const std::shared_ptr<Storage>& storage) = 0;
    };
}

NAMESPACE_SPH_END
