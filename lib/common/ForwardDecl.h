#pragma once

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Quantity;
enum class QuantityId;
class Statistics;
enum class StatisticsId;

template <typename TEnum>
class Settings;

enum class BodySettingsId;
enum class RunSettingsId;

using RunSettings = Settings<RunSettingsId>;
using BodySettings = Settings<BodySettingsId>;

namespace Abstract {
    class Material;
    class Finder;
    class Logger;
    class Domain;
    class Solver;
    class TimeStepping;
    class TimeStepCriterion;
}
struct Ghost;
struct NeighbourRecord;
class MaterialView;


NAMESPACE_SPH_END
