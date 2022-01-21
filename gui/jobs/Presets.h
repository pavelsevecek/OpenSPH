#include "run/jobs/Presets.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

enum class GuiId {
    BLACK_HOLE = 100,
};

using ExtId = ExtendedEnum<Id>;

SharedPtr<JobNode> make(const ExtId id, UniqueNameManager& nameMgr, const Size particleCnt = 10000);

SharedPtr<JobNode> makeBlackHole(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

} // namespace Presets

SPH_EXTEND_ENUM(Presets::GuiId, Presets::Id);

NAMESPACE_SPH_END
