#pragma once
#include "GameData.hpp"

namespace MissionTargets {
    struct Snapshot {
        int Mode = 0;
        std::vector<MissionModel::Rule> Rules;
    };
    Snapshot Read(SDK::UWorld* World);
    bool Apply(CachedEntity& Entity, const Snapshot& Mission, MissionModel::Category Category);
    std::wstring ObjectName(SDK::AActor* Actor);
}
