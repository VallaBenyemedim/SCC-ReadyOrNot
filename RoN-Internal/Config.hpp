#pragma once
#include <string>
#include <Windows.h>

namespace Config
{
    struct Color {
        float R, G, B, A;
        bool operator==(const Color&) const = default;
    };

    struct ESPSettings {
        bool bEnabled = true;
        bool bBox = true;
        bool bNameTags = true;
        bool bSkeleton = true;
        bool bDistance = true;
        bool bDeclutterLabels = true;
        bool bObjectives = true;
        bool bSuspects = true;
        bool bCivilians = true;
        bool bAllies = true;
        bool bNamedTargets = true;
        bool bBombTargets = true;
        bool bObjectiveObjects = true;
        bool bTraps = true;
        bool bEvidence = true;
        bool bDroppedWeapon = true;
        bool bReportable = true;
        float BoxThickness = 1.0f;
        float SkeletonThickness = 1.5f;
        float TextScale = 1.0f;
        float TextOpacity = 0.88f;
        float LabelBackground = 0.28f;
        float SkeletonOpacity = 0.72f;

        Color ColSuspect = { 1.0f, 0.0f, 0.0f, 1.0f };
        Color ColCivilian = { 0.0f, 1.0f, 0.0f, 1.0f };
        Color ColAlly = { 0.0f, 0.5f, 1.0f, 1.0f };
        Color ColArrested = { 1.0f, 0.5f, 0.0f, 1.0f };
        Color ColTrap = { 1.0f, 0.0f, 1.0f, 1.0f };
        Color ColEvidence = { 0.0f, 1.0f, 1.0f, 1.0f };
        Color ColDroppedWeapon = { 1.0f, 1.0f, 0.0f, 1.0f };
        Color ColReportable = { 1.0f, 0.5f, 1.0f, 1.0f };
        Color ColObjective = { 1.0f, 0.82f, 0.15f, 1.0f };
        Color ColBomb = { 1.0f, 0.35f, 0.08f, 1.0f };
        bool operator==(const ESPSettings&) const = default;
    };

    struct DebugSettings {
        bool bConsoleEnabled = false;
        bool bVerboseLog = false; 
        bool operator==(const DebugSettings&) const = default;
    };

    struct MiscSettings {
        bool bForceOfficial = true;
        bool operator==(const MiscSettings&) const = default;
    };

    struct GameplaySettings {
        bool bNoRecoil=false,bNoReload=false,bPlayerHealth=false,bNoInjuries=false,bTeamHealth=false;
        bool bMaxSpeed=false,bCrouchSpeed=false,bSlowWalkSpeed=false,bWalkSpeed=false;
        bool bForceSurrender=false,bAlwaysSurrender=false,bDropWeapons=false,bUnlockDoors=false,bTeleport=false;
        float RecoilScale=0.0f,MaxSpeed=900.0f,CrouchMultiplier=1.5f,SlowWalkMultiplier=1.5f,WalkMultiplier=1.5f;
        float NPCRange=0.0f,TeleportDistance=2.0f;
        int NPCFilter=0;
        bool operator==(const GameplaySettings&) const=default;
    };

    struct Settings {
        ESPSettings ESP;
        DebugSettings Debug;
        MiscSettings Misc;
        GameplaySettings Gameplay;
        bool operator==(const Settings&) const = default;
    };

    // Accessor
    Settings Snapshot();
    void Set(const Settings& Value);
    std::string GetConfigPath();

    // File Operations
    void Load();
    void Save();
}
