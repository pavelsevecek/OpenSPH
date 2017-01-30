#pragma once

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Quantity;
enum class QuantityIds;
struct Material;
class Statistics;
enum class StatisticsIds;

template <typename TEnum>
class Settings;

enum class BodySettingsIds;
enum class GlobalSettingsIds;

using GlobalSettings = Settings<GlobalSettingsIds>;
using BodySettings = Settings<BodySettingsIds>;

namespace Abstract {
    class Finder;
    class Logger;
    class Domain;
    class Solver;
    class TimeStepping;
    class TimeStepCriterion;
}
struct Ghost;
struct NeighbourRecord;


NAMESPACE_SPH_END
