#pragma once

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class RawPtr;
template <typename T>
class AutoPtr;
template <typename T>
class SharedPtr;
template <typename T>
class Function;
template <typename T>
class Optional;

class IScheduler;
class Task;
class ThreadPool;
template <typename T>
class ThreadLocal;

template <typename T>
class List;

template <typename T>
class Flags;

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
class IDistribution;
class IBasicFinder;
class ILogger;
class IDomain;
class ISolver;
class ITimeStepping;
class ITimeStepCriterion;

class DerivativeHolder;

struct Ghost;
struct NeighbourRecord;
class MaterialView;


NAMESPACE_SPH_END
