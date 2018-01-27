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

class IMaterial;
class IBasicFinder;
class ILogger;
class IDomain;
class ISolver;
class ITimeStepping;
class ITimeStepCriterion;

struct Ghost;
struct NeighbourRecord;
class MaterialView;


NAMESPACE_SPH_END
