#include "models/BasicModel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

BasicModel::BasicModel(const Settings<GlobalSettingsIds>& settings) {
    finder = Factory::getFinder((FinderEnum)settings.get<int>(GlobalSettingsIds::FINDER).get());
    kernel = Factory::getKernel((KernelEnum)settings.get<int>(GlobalSettingsIds::KERNEL).get());
}


NAMESPACE_SPH_END
