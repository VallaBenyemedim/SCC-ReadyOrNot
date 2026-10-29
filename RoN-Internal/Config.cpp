#include "Config.hpp"
#include <filesystem>
#include <shlwapi.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>

#pragma comment(lib, "Shlwapi.lib")

namespace Config
{
    Settings StoredSettings;
    std::mutex SettingsMutex;

    Settings Snapshot()
    {
        std::lock_guard<std::mutex> Lock(SettingsMutex);
        return StoredSettings;
    }

    void Set(const Settings& Value)
    {
        std::lock_guard<std::mutex> Lock(SettingsMutex);
        StoredSettings = Value;
    }

    std::string GetConfigPath()
    {
        char Path[MAX_PATH];
        HMODULE hModule = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Snapshot, &hModule);
        GetModuleFileNameA(hModule, Path, MAX_PATH);

        std::filesystem::path DllPath(Path);
        return DllPath.replace_extension(".ini").string();
    }

    // Helpers
    void WriteColor(const char* Section, const char* Key, const Color& C, const std::string& Path) {
        char Buf[64];
        sprintf_s(Buf, "%.6f,%.6f,%.6f,%.6f", C.R, C.G, C.B, C.A);
        WritePrivateProfileStringA(Section, Key, Buf, Path.c_str());
    }

    Color ReadColor(const char* Section, const char* Key, Color Default, const std::string& Path) {
        char Buf[64];
        char DefBuf[64];
        sprintf_s(DefBuf, "%.2f,%.2f,%.2f,%.2f", Default.R, Default.G, Default.B, Default.A);
        
        GetPrivateProfileStringA(Section, Key, DefBuf, Buf, 64, Path.c_str());
        
        Color C = Default;
        sscanf_s(Buf, "%f,%f,%f,%f", &C.R, &C.G, &C.B, &C.A);
        return C;
    }

    void Load()
    {
        Settings InternalSettings;
        std::string Path = GetConfigPath();

        // ESP
        InternalSettings.ESP.bEnabled = GetPrivateProfileIntA("ESP", "Enabled", 1, Path.c_str());
        InternalSettings.ESP.bBox = GetPrivateProfileIntA("ESP", "Box", 1, Path.c_str());
        InternalSettings.ESP.bNameTags = GetPrivateProfileIntA("ESP", "NameTags", 1, Path.c_str());
        InternalSettings.ESP.bSkeleton = GetPrivateProfileIntA("ESP", "Skeleton", 1, Path.c_str());
        InternalSettings.ESP.bDistance = GetPrivateProfileIntA("ESP", "Distance", 1, Path.c_str());
        InternalSettings.ESP.bDeclutterLabels = GetPrivateProfileIntA("ESP", "DeclutterLabels", 1, Path.c_str());
        InternalSettings.ESP.bObjectives = GetPrivateProfileIntA("ESP", "Objectives", 1, Path.c_str());
        InternalSettings.ESP.bSuspects = GetPrivateProfileIntA("ESP", "Suspects", 1, Path.c_str());
        InternalSettings.ESP.bCivilians = GetPrivateProfileIntA("ESP", "Civilians", 1, Path.c_str());
        InternalSettings.ESP.bAllies = GetPrivateProfileIntA("ESP", "Allies", 1, Path.c_str());
        InternalSettings.ESP.bNamedTargets = GetPrivateProfileIntA("ESP", "NamedTargets", 1, Path.c_str());
        InternalSettings.ESP.bBombTargets = GetPrivateProfileIntA("ESP", "BombTargets", 1, Path.c_str());
        InternalSettings.ESP.bObjectiveObjects = GetPrivateProfileIntA("ESP", "ObjectiveObjects", 1, Path.c_str());
        InternalSettings.ESP.bTraps = GetPrivateProfileIntA("ESP", "Traps", 1, Path.c_str());
        InternalSettings.ESP.bEvidence = GetPrivateProfileIntA("ESP", "Evidence", 1, Path.c_str());
        InternalSettings.ESP.bDroppedWeapon = GetPrivateProfileIntA("ESP", "DroppedWeapon", 1, Path.c_str());
        InternalSettings.ESP.bReportable = GetPrivateProfileIntA("ESP", "Reportable", 1, Path.c_str());

        char Buffer[32];
        auto ReadFloat = [&](const char* Key, const char* Default, float Min, float Max) {
            GetPrivateProfileStringA("ESP", Key, Default, Buffer, 32, Path.c_str());
            char* End = nullptr;
            float Value = std::strtof(Buffer, &End);
            if (End == Buffer || !std::isfinite(Value)) Value = std::strtof(Default, nullptr);
            return std::clamp(Value, Min, Max);
        };
        InternalSettings.ESP.BoxThickness = ReadFloat("BoxThickness", "1.0", 0.5f, 6.0f);
        InternalSettings.ESP.SkeletonThickness = ReadFloat("SkeletonThickness", "1.5", 0.5f, 6.0f);
        InternalSettings.ESP.TextScale = ReadFloat("TextScale", "1.0", 0.6f, 2.0f);
        InternalSettings.ESP.TextOpacity = ReadFloat("TextOpacity", "0.88", 0.25f, 1.0f);
        InternalSettings.ESP.LabelBackground = ReadFloat("LabelBackground", "0.28", 0.0f, 0.85f);
        InternalSettings.ESP.SkeletonOpacity = ReadFloat("SkeletonOpacity", "0.72", 0.2f, 1.0f);

        // Colors
        InternalSettings.ESP.ColSuspect = ReadColor("ESP", "ColorSuspect", { 1.0f, 0.0f, 0.0f, 1.0f }, Path);
        InternalSettings.ESP.ColCivilian = ReadColor("ESP", "ColorCivilian", { 0.0f, 1.0f, 0.0f, 1.0f }, Path);
        InternalSettings.ESP.ColAlly = ReadColor("ESP", "ColorAlly", { 0.0f, 0.5f, 1.0f, 1.0f }, Path);
        InternalSettings.ESP.ColArrested = ReadColor("ESP", "ColorArrested", { 1.0f, 0.5f, 0.0f, 1.0f }, Path);
        InternalSettings.ESP.ColTrap = ReadColor("ESP", "ColorTrap", { 1.0f, 0.0f, 1.0f, 1.0f }, Path); // Default Purple
        InternalSettings.ESP.ColEvidence = ReadColor("ESP", "ColorEvidence", { 0.0f, 1.0f, 1.0f, 1.0f }, Path); // Default Cyan
        InternalSettings.ESP.ColDroppedWeapon = ReadColor("ESP", "ColorDroppedWeapon", { 1.0f, 1.0f, 0.0f, 1.0f }, Path); // Default Yellow
        InternalSettings.ESP.ColReportable = ReadColor("ESP", "ColorReportable", { 1.0f, 0.5f, 1.0f, 1.0f }, Path); // Default Pink
        InternalSettings.ESP.ColObjective = ReadColor("ESP", "ColorObjective", { 1.0f, 0.82f, 0.15f, 1.0f }, Path);
        InternalSettings.ESP.ColBomb = ReadColor("ESP", "ColorBomb", { 1.0f, 0.35f, 0.08f, 1.0f }, Path);

        // Debug
        InternalSettings.Debug.bConsoleEnabled = GetPrivateProfileIntA("Debug", "ConsoleEnabled", 0, Path.c_str());
        InternalSettings.Debug.bVerboseLog = GetPrivateProfileIntA("Debug", "VerboseLog", 0, Path.c_str());

        // Misc
        InternalSettings.Misc.bForceOfficial = GetPrivateProfileIntA("Misc", "ForceOfficial", 1, Path.c_str());
        auto& G=InternalSettings.Gameplay;
#define READ_GAME_BOOL(Field,Key) G.Field=GetPrivateProfileIntA("Gameplay",Key,0,Path.c_str())!=0
        READ_GAME_BOOL(bNoRecoil,"NoRecoil");READ_GAME_BOOL(bNoReload,"NoReload");
        READ_GAME_BOOL(bPlayerHealth,"PlayerHealth");READ_GAME_BOOL(bNoInjuries,"NoInjuries");READ_GAME_BOOL(bTeamHealth,"TeamHealth");
        READ_GAME_BOOL(bMaxSpeed,"MaxSpeedEnabled");READ_GAME_BOOL(bCrouchSpeed,"CrouchSpeed");READ_GAME_BOOL(bSlowWalkSpeed,"SlowWalkSpeed");READ_GAME_BOOL(bWalkSpeed,"WalkSpeed");
        READ_GAME_BOOL(bForceSurrender,"ForceSurrender");READ_GAME_BOOL(bAlwaysSurrender,"AlwaysSurrender");READ_GAME_BOOL(bDropWeapons,"DropWeapons");READ_GAME_BOOL(bUnlockDoors,"UnlockDoors");READ_GAME_BOOL(bTeleport,"Teleport");
#undef READ_GAME_BOOL
        auto GameFloat=[&](const char* Key,const char* Default,float Min,float Max) {
            GetPrivateProfileStringA("Gameplay",Key,Default,Buffer,32,Path.c_str());
            char* End=nullptr;float V=std::strtof(Buffer,&End);
            return std::clamp(End==Buffer || !std::isfinite(V)?std::strtof(Default,nullptr):V,Min,Max);
        };
        G.RecoilScale=GameFloat("RecoilScale","0",0,1);G.MaxSpeed=GameFloat("MaxSpeed","900",100,3000);
        G.CrouchMultiplier=GameFloat("CrouchMultiplier","1.5",0.25f,5);G.SlowWalkMultiplier=GameFloat("SlowWalkMultiplier","1.5",0.25f,5);
        G.WalkMultiplier=GameFloat("WalkMultiplier","1.5",0.25f,5);G.NPCRange=GameFloat("NPCRange","0",0,250);
        G.TeleportDistance=GameFloat("TeleportDistance","2",1,6);
        G.NPCFilter=std::clamp(static_cast<int>(GetPrivateProfileIntA("Gameplay","NPCFilter",0,Path.c_str())),0,3);
        Set(InternalSettings);
        if (!std::filesystem::exists(Path)) Save();
    }

    void Save()
    {
        const auto InternalSettings = Snapshot();
        std::string Path = GetConfigPath();

        // ESP
        WritePrivateProfileStringA("ESP", "Enabled", InternalSettings.ESP.bEnabled ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Box", InternalSettings.ESP.bBox ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "NameTags", InternalSettings.ESP.bNameTags ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Skeleton", InternalSettings.ESP.bSkeleton ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Distance", InternalSettings.ESP.bDistance ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "DeclutterLabels", InternalSettings.ESP.bDeclutterLabels ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Objectives", InternalSettings.ESP.bObjectives ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Suspects", InternalSettings.ESP.bSuspects ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Civilians", InternalSettings.ESP.bCivilians ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Allies", InternalSettings.ESP.bAllies ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "NamedTargets", InternalSettings.ESP.bNamedTargets ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "BombTargets", InternalSettings.ESP.bBombTargets ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "ObjectiveObjects", InternalSettings.ESP.bObjectiveObjects ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Traps", InternalSettings.ESP.bTraps ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Evidence", InternalSettings.ESP.bEvidence ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "DroppedWeapon", InternalSettings.ESP.bDroppedWeapon ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "Reportable", InternalSettings.ESP.bReportable ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("ESP", "BoxThickness", std::to_string(InternalSettings.ESP.BoxThickness).c_str(), Path.c_str());
        WritePrivateProfileStringA("ESP", "SkeletonThickness", std::to_string(InternalSettings.ESP.SkeletonThickness).c_str(), Path.c_str());
        WritePrivateProfileStringA("ESP", "TextScale", std::to_string(InternalSettings.ESP.TextScale).c_str(), Path.c_str());
        WritePrivateProfileStringA("ESP", "TextOpacity", std::to_string(InternalSettings.ESP.TextOpacity).c_str(), Path.c_str());
        WritePrivateProfileStringA("ESP", "LabelBackground", std::to_string(InternalSettings.ESP.LabelBackground).c_str(), Path.c_str());
        WritePrivateProfileStringA("ESP", "SkeletonOpacity", std::to_string(InternalSettings.ESP.SkeletonOpacity).c_str(), Path.c_str());

        // Colors
        WriteColor("ESP", "ColorSuspect", InternalSettings.ESP.ColSuspect, Path);
        WriteColor("ESP", "ColorCivilian", InternalSettings.ESP.ColCivilian, Path);
        WriteColor("ESP", "ColorAlly", InternalSettings.ESP.ColAlly, Path);
        WriteColor("ESP", "ColorArrested", InternalSettings.ESP.ColArrested, Path);
        WriteColor("ESP", "ColorTrap", InternalSettings.ESP.ColTrap, Path);
        WriteColor("ESP", "ColorEvidence", InternalSettings.ESP.ColEvidence, Path);
        WriteColor("ESP", "ColorDroppedWeapon", InternalSettings.ESP.ColDroppedWeapon, Path);
        WriteColor("ESP", "ColorReportable", InternalSettings.ESP.ColReportable, Path);
        WriteColor("ESP", "ColorObjective", InternalSettings.ESP.ColObjective, Path);
        WriteColor("ESP", "ColorBomb", InternalSettings.ESP.ColBomb, Path);

        // Debug
        WritePrivateProfileStringA("Debug", "ConsoleEnabled", InternalSettings.Debug.bConsoleEnabled ? "1" : "0", Path.c_str());
        WritePrivateProfileStringA("Debug", "VerboseLog", InternalSettings.Debug.bVerboseLog ? "1" : "0", Path.c_str());

        WritePrivateProfileStringA("Keys", nullptr, nullptr, Path.c_str());

        // Misc
        WritePrivateProfileStringA("Misc", "ForceOfficial", InternalSettings.Misc.bForceOfficial ? "1" : "0", Path.c_str());
        const auto& G=InternalSettings.Gameplay;
#define WRITE_GAME_BOOL(Field,Key) WritePrivateProfileStringA("Gameplay",Key,G.Field?"1":"0",Path.c_str())
        WRITE_GAME_BOOL(bNoRecoil,"NoRecoil");WRITE_GAME_BOOL(bNoReload,"NoReload");WRITE_GAME_BOOL(bPlayerHealth,"PlayerHealth");WRITE_GAME_BOOL(bNoInjuries,"NoInjuries");WRITE_GAME_BOOL(bTeamHealth,"TeamHealth");
        WRITE_GAME_BOOL(bMaxSpeed,"MaxSpeedEnabled");WRITE_GAME_BOOL(bCrouchSpeed,"CrouchSpeed");WRITE_GAME_BOOL(bSlowWalkSpeed,"SlowWalkSpeed");WRITE_GAME_BOOL(bWalkSpeed,"WalkSpeed");
        WRITE_GAME_BOOL(bForceSurrender,"ForceSurrender");WRITE_GAME_BOOL(bAlwaysSurrender,"AlwaysSurrender");WRITE_GAME_BOOL(bDropWeapons,"DropWeapons");WRITE_GAME_BOOL(bUnlockDoors,"UnlockDoors");WRITE_GAME_BOOL(bTeleport,"Teleport");
#undef WRITE_GAME_BOOL
#define WRITE_GAME_FLOAT(Field) WritePrivateProfileStringA("Gameplay",#Field,std::to_string(G.Field).c_str(),Path.c_str())
        WRITE_GAME_FLOAT(RecoilScale);WRITE_GAME_FLOAT(MaxSpeed);WRITE_GAME_FLOAT(CrouchMultiplier);WRITE_GAME_FLOAT(SlowWalkMultiplier);WRITE_GAME_FLOAT(WalkMultiplier);WRITE_GAME_FLOAT(NPCRange);WRITE_GAME_FLOAT(TeleportDistance);
#undef WRITE_GAME_FLOAT
        WritePrivateProfileStringA("Gameplay","NPCFilter",std::to_string(G.NPCFilter).c_str(),Path.c_str());
    }
}
