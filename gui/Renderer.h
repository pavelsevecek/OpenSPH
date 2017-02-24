#pragma once

#include "gui/Common.h"
#include "quantities/Storage.h"
#include <memory>

NAMESPACE_SPH_BEGIN

class Bitmap;

namespace Abstract {
    class Renderer : public Polymorphic {
    public:
        /// Updates and displays rendered image using current settings.
        virtual void draw(const std::shared_ptr<Storage>& storage, const Statistics& stats) = 0;

        /// Returns rendered image as bitmap.
        virtual Bitmap getRender() const = 0;

        /// Changes the quantity used as color scale.
        virtual void setQuantity(const QuantityIds key) = 0;
    };
}

NAMESPACE_SPH_END
