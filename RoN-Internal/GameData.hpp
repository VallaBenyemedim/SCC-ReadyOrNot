#pragma once
#include <vector>
#include <mutex>
#include "SDK/Engine_classes.hpp"
#include "EntityTypes.hpp"
#include "MissionModel.hpp"

struct CachedEntity {
    SDK::AActor* Actor = nullptr;
    EEntityType Type = EEntityType::Unknown;
    EEntityStatus Status = EEntityStatus::Active;
    int ActorIndex = -1;
    MissionModel::Kind MissionKind = MissionModel::Kind::None;
    std::wstring MissionCaption;
    int BombSeconds = -1;
    bool Handgun = false;
};

class GameData {
public:
    static std::vector<CachedEntity> CachedEntities;
    static std::mutex DataMutex;

    // Updates the cache. Should be called from the Main Logic Thread.
    static void Update(SDK::UWorld* World);

    // Clears the cache safely.
    static void Clear();
};
