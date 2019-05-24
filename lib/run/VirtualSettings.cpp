#include "run/VirtualSettings.h"

NAMESPACE_SPH_BEGIN

void VirtualSettings::set(const std::string& key, const IVirtualEntry::Value& value) {
    for (auto& category : categories) {
        auto entry = category.value.entries.tryGet(key);
        if (entry) {
            entry.value()->set(value);
            return;
        }
    }
    throw InvalidSetup("Key '" + key + "' not found");
}

IVirtualEntry::Value VirtualSettings::get(const std::string& key) const {
    for (auto& category : categories) {
        auto entry = category.value.entries.tryGet(key);
        if (entry) {
            return entry.value()->get();
        }
    }
    throw InvalidSetup("Key '" + key + "' not found");
}

VirtualSettings::Category& VirtualSettings::addCategory(const std::string& name) {
    return categories.insert(name, Category{});
}

void VirtualSettings::enumerate(const IEntryProc& proc) {
    for (auto& category : categories) {
        proc.onCategory(category.key);
        for (auto& entry : category.value.entries) {
            proc.onEntry(entry.key, *entry.value);
        }
    }
}

NAMESPACE_SPH_END
