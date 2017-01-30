#pragma once

#include "gui/Common.h"
#include "quantities/Storage.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Renderer : public Polymorphic {
    public:
        virtual void draw(const std::shared_ptr<Storage>& storage) = 0;

        virtual void setQuantity(const QuantityIds key) = 0;
    };
}

NAMESPACE_SPH_END
