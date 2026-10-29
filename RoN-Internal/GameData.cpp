#include "GameData.hpp"
#include "Config.hpp"
#include "MissionTargets.hpp"
#include "WeaponESP.hpp"
#include "OverlayScene.hpp"
#include "Logger.hpp"
#include <unordered_map>
#include <cmath>
#include "SDK/ReadyOrNot_classes.hpp"
#include "SDK/CyberneticsSuspect_V2_classes.hpp"
#include "SDK/CyberneticsSwat_V2_classes.hpp"
#include "SDK/Cybernetics_Civilian_V2_classes.hpp"

std::vector<CachedEntity> GameData::CachedEntities;
std::mutex GameData::DataMutex;

void GameData::Update(SDK::UWorld* World)
{
    if (!World || !World->PersistentLevel) return;
    const auto Settings = Config::Snapshot().ESP;

    std::vector<CachedEntity> LocalBuffer;
    std::unordered_map<SDK::AActor*, size_t> Seen;
    const auto Mission = MissionTargets::Read(World);
    bool HasBombGoal = false, HasDoorGoal = false;
    for (const auto& Rule : Mission.Rules) {
        HasBombGoal |= Rule.Action == MissionModel::Kind::Defuse;
        HasDoorGoal |= Rule.Action == MissionModel::Kind::Interact;
    }
    auto AddEntity = [&](CachedEntity Entity, MissionModel::Category Category = MissionModel::Category::Other, bool RequireMission = false) {
        const bool IsTarget = MissionTargets::Apply(Entity, Mission, Category);
        if (RequireMission && !IsTarget) return;
        if (!IsTarget && ((Entity.Type == EEntityType::Suspect && !Settings.bSuspects)
            || (Entity.Type == EEntityType::Civilian && !Settings.bCivilians)
            || (Entity.Type == EEntityType::Ally && !Settings.bAllies))) return;
        Entity.ActorIndex = Entity.Actor->Index;
        const auto Found = Seen.find(Entity.Actor);
        if (Found != Seen.end()) {
            if (IsTarget && LocalBuffer[Found->second].MissionCaption.empty()) {
                LocalBuffer[Found->second].MissionCaption = std::move(Entity.MissionCaption);
                LocalBuffer[Found->second].MissionKind = Entity.MissionKind;
                if (Entity.Type == EEntityType::Computer) LocalBuffer[Found->second].Type = Entity.Type;
            }
            return;
        }
        Seen.emplace(Entity.Actor, LocalBuffer.size());
        LocalBuffer.push_back(std::move(Entity));
    };
    auto AddWeapon = [&](SDK::ABaseItem* Item) {
        if (!Settings.bDroppedWeapon || !Item) return;
        auto* Previous=Item->PreviousOwner;
        auto* Evidence=Item->EvidenceComponent;
        WeaponESP::State State;
        State.IsWeapon=Item->IsA(SDK::ABaseWeapon::StaticClass());
        State.InInventory=Item->bInInventory;
        State.HasMesh=Item->ItemMesh!=nullptr;
        State.HasCurrentOwner=Item->Owner!=nullptr;
        State.PreviousSuspect=Previous && Previous->IsA(SDK::ACyberneticsSuspect_V2_C::StaticClass());
        State.PreviousAlly=Previous && Previous->IsA(SDK::ACyberneticsSwat_V2_C::StaticClass());
        State.Collected=Evidence && (Evidence->bEvidenceExtracted
            || Evidence->EvidenceState==SDK::EEvidenceActorState::Collected
            || Evidence->EvidenceState==SDK::EEvidenceActorState::Extraction);
        State.MarkUnownedEvidence=Evidence && Evidence->bMarkAsEvidenceWhenNoOwner;
        if (!WeaponESP::ShouldShow(State)) return;
        CachedEntity Entity;
        Entity.Actor=Item; Entity.Type=EEntityType::DroppedWeapon;
        Entity.Handgun=Item->ItemClass==SDK::EItemClass::IC_Pistol;
        if (Entity.Handgun) {
            static bool Logged=false;
            if (!Logged) {Logger::Log("[ESP] Ground handgun discovered: %s; previous owner %s",Item->GetName().c_str(),Previous?"present":"absent");Logged=true;}
        }
        AddEntity(std::move(Entity),MissionModel::Category::Evidence);
    };
    
    SDK::APawn* MyPawn = nullptr;
    if (World->OwningGameInstance && World->OwningGameInstance->LocalPlayers.Num() > 0 && World->OwningGameInstance->LocalPlayers[0])
    {
        if (auto PC = World->OwningGameInstance->LocalPlayers[0]->PlayerController)
        {
            MyPawn = PC->Pawn;
        }
    }

    // Iterate ALL levels (PersistentLevel + streaming sub-levels)
    // Evidence, weapons, etc. are often in sub-levels, not PersistentLevel
    SDK::TArray<SDK::ULevel*>& Levels = World->Levels;

    for (int lvl = 0; lvl < Levels.Num(); lvl++)
    {
        SDK::ULevel* Level = Levels[lvl];
        if (!Level) continue;

        SDK::TArray<SDK::AActor*>& Actors = Level->Actors;

        for (int i = 0; i < Actors.Num(); i++)
        {
            SDK::AActor* Actor = Actors[i];
            if (!Actor || Actor == MyPawn) continue;

        CachedEntity NewEntity;
        NewEntity.Actor = Actor;
        NewEntity.Type = EEntityType::Unknown;
        NewEntity.Status = EEntityStatus::Active;

        // Some world/evidence weapons have no PreviousOwner or are absent from AllItems.
        if (Settings.bDroppedWeapon && Actor->IsA(SDK::ABaseItem::StaticClass())) {
            AddWeapon(static_cast<SDK::ABaseItem*>(Actor));
            continue;
        }

        if (HasBombGoal && Actor->IsA(SDK::ABombActor::StaticClass())) {
            auto* Bomb = static_cast<SDK::ABombActor*>(Actor);
            if (Bomb->BombState == SDK::EBombState::BS_Active) {
                NewEntity.Type = EEntityType::Bomb;
                if (std::isfinite(Bomb->TimeUntilExplodes) && Bomb->TimeUntilExplodes >= 0 && Bomb->TimeUntilExplodes < 86400)
                    NewEntity.BombSeconds = static_cast<int>(std::ceil(Bomb->TimeUntilExplodes));
                AddEntity(std::move(NewEntity), MissionModel::Category::Bomb, true);
            }
            continue;
        }
        if (HasDoorGoal && Actor->IsA(SDK::ADoor::StaticClass())) {
            NewEntity.Type = EEntityType::MissionObject;
            AddEntity(std::move(NewEntity), MissionModel::Category::Door, true);
            continue;
        }

        if (Settings.bTraps && Actor->IsA(SDK::ATrapActor::StaticClass()))
        {
            NewEntity.Type = EEntityType::Trap;
            SDK::ATrapActor* Trap = static_cast<SDK::ATrapActor*>(Actor);
            
            if (Trap->TrapStatus == SDK::ETrapState::TS_Live)
                NewEntity.Status = EEntityStatus::TrapLive;
            else
                NewEntity.Status = EEntityStatus::TrapDisabled;

            AddEntity(std::move(NewEntity));
            continue;
        }

        // AEvidenceActor (It is not the suspect's dropped weapon.)
        if ((Settings.bEvidence || !Mission.Rules.empty()) && Actor->IsA(SDK::AEvidenceActor::StaticClass()))
        {
            NewEntity.Type = EEntityType::Evidence;
            NewEntity.Status = EEntityStatus::Active;
            AddEntity(std::move(NewEntity), MissionModel::Category::Evidence, !Settings.bEvidence);
            continue;
        }

        if (Actor->IsA(SDK::AReadyOrNotCharacter::StaticClass()))
        {
            SDK::AReadyOrNotCharacter* Character = static_cast<SDK::AReadyOrNotCharacter*>(Actor);

            SDK::FVector Loc = Actor->K2_GetActorLocation();
            // Filter camera entities specifically near 0,0,-1000, allowing valid underground entities
            if (Loc.Z > -1050.0f && Loc.Z < -950.0f && 
                Loc.X > -50.0f && Loc.X < 50.0f && 
                Loc.Y > -50.0f && Loc.Y < 50.0f) 
            {
                continue; 
            }

            // Determine Status
            if (Character->IsDeadOrUnconscious()) {
                NewEntity.Status = EEntityStatus::Dead;
            }
            else if (Character->IsArrested()) {
                NewEntity.Status = EEntityStatus::Arrested;
            }

            // Determine Type
            if (Actor->IsA(SDK::ACyberneticsSuspect_V2_C::StaticClass())) {
                NewEntity.Type = EEntityType::Suspect;
            }
            else if (Actor->IsA(SDK::ACybernetics_Civilian_V2_C::StaticClass())) {
                NewEntity.Type = EEntityType::Civilian;
            }
            else if (Actor->IsA(SDK::ACyberneticsSwat_V2_C::StaticClass())) {
                NewEntity.Type = EEntityType::Ally;
            }

            if (NewEntity.Type != EEntityType::Unknown)
            {
                AddEntity(std::move(NewEntity), MissionModel::Category::Person);
            }
        }
        } // end actor loop
    } // end level loop

    // GameState-based detection: Dropped weapons & Reportable actors
    if (World->GameState)
    {
        auto* GS = static_cast<SDK::AReadyOrNotGameState*>(World->GameState);

        // Registry fallback; Seen prevents duplicates with streamed-level discovery.
        if (Settings.bDroppedWeapon)
        {
            for (int i = 0; i < GS->AllItems.Num(); i++)
            {
                AddWeapon(GS->AllItems[i]);
            }
        }

        // Reportable actors (mission-critical evidence NPCs/objects)
        // Skip already reported ones (bHasBeenReported at 0x02C1)
        if (Settings.bReportable || !Mission.Rules.empty())
        {
            for (int i = 0; i < GS->AllReportableActors.Num(); i++)
            {
                auto* RA = GS->AllReportableActors[i];
                if (!RA) continue;
                if (!RA->bReportableEnabled) continue;
                if (RA->bHasBeenReported) continue;

                CachedEntity NewEntity;
                NewEntity.Actor = RA;
                NewEntity.Type = EEntityType::Reportable;
                NewEntity.Status = EEntityStatus::Active;
                AddEntity(std::move(NewEntity), MissionModel::Category::Reportable, !Settings.bReportable);
            }
        }
    }

    std::lock_guard<std::mutex> Lock(DataMutex);
    CachedEntities = std::move(LocalBuffer);
}

void GameData::Clear()
{
    OverlayScene::Clear();
    std::lock_guard<std::mutex> Lock(DataMutex);
    CachedEntities.clear();
}
