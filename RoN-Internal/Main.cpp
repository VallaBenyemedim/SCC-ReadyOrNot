#include <Windows.h>
#include <vector>
#include <string>
#include <chrono>

#include "SDK/Engine_classes.hpp"
#include "SDK/ReadyOrNot_classes.hpp"
#include "Hooks.hpp"
#include "GameData.hpp"
#include "Logger.hpp"
#include "Config.hpp"
#include "OfficialScore.hpp"
#include "MenuUI.hpp"
#include "GraphicsOverlay.hpp"
#include "GameInput.hpp"
#include "Gameplay.hpp"

// Helper function to isolate SEH usage
void SafeLoopBody(bool& bHookInstalled, SDK::ULevel*& LastLevel, SDK::AHUD*& IgnoreHUD, int& IgnoreHUDIndex)
{
    __try 
    {
        SDK::UEngine* Engine = SDK::UEngine::GetEngine();
        SDK::UWorld* World = SDK::UWorld::GetWorld();

        if (!Engine || (uintptr_t)Engine < 0x10000) return;
        if (!World || (uintptr_t)World < 0x10000) return;

        OfficialScore::Update(World);

        // Check if we're transitioning levels
        if (World->PersistentLevel != LastLevel)
        {
            if (LastLevel != nullptr)
            {
                // Marking the current (old) HUD as ignored so we don't re-hook it instantly :p 
                if (World->OwningGameInstance && World->OwningGameInstance->LocalPlayers.Num() > 0 && World->OwningGameInstance->LocalPlayers[0])
                {
                     SDK::APlayerController* PC = World->OwningGameInstance->LocalPlayers[0]->PlayerController;
                     if (PC && PC->MyHUD) {
                         IgnoreHUD = PC->MyHUD;
                         IgnoreHUDIndex = PC->MyHUD->Name.ComparisonIndex;
                     }
                }

                // After some fiddling, level change seems to destoroy the HUD. This means we need to reset the hook state.
                Hooks::Reset();
                bHookInstalled = false;

                GameData::Clear();
            }
            
            LastLevel = World->PersistentLevel;
        }

        if (World->PersistentLevel)
        {
            if (!bHookInstalled)
            {
                auto GI = World->OwningGameInstance;
                auto LP = (GI && GI->LocalPlayers.Num() > 0) ? GI->LocalPlayers[0] : nullptr;
                auto MyController = (LP) ? LP->PlayerController : nullptr;

                if (MyController)
                {
                    bool bCanHook = false;

                    // Check if pointer is usable (if pointer not valid, HUD probably is destroyed)
                    if (MyController->MyHUD == nullptr)
                    {
                        IgnoreHUD = nullptr;
                        IgnoreHUDIndex = 0;
                    }
                    else if (MyController->MyHUD != IgnoreHUD)
                    {
                        bCanHook = true;
                    }
                    else
                    {
                        // Apparently the HUD memory address can stay the same even if the previous HUD got destoyed.
                        // This means we have to check the name index
                        if (MyController->MyHUD->Name.ComparisonIndex != IgnoreHUDIndex) 
                        {
                            bCanHook = true;
                        }
                    }

                    if (bCanHook)
                    {
                        if (Hooks::Init(MyController->MyHUD))
                        {
                            bHookInstalled = true;
                        }
                        else
                        {
                            Sleep(500); // Failed to hook, might have to wait a bit for HUD to spawn
                        }
                    } 
                } 
            } 

            if (bHookInstalled) 
            {
                GameData::Update(World);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // Just a silent catch
    }
}

DWORD MainThread(HMODULE Module)
{
    Config::Load();
    Logger::Init(Config::Snapshot().Debug.bConsoleEnabled);
    Logger::Log("[+] SCC for Ready or Not loaded");
    Logger::Log("[+] SCC v5: configurable gameplay controls, clean ImGui ESP; INSERT menu");
    Logger::Log("[+] SDK updated for Steam build 25784851");
    Logger::Log("[+] Config Loaded from: %s",Config::GetConfigPath().c_str());

    HWND Window=nullptr;
    EnumWindows([](HWND Candidate,LPARAM Param)->BOOL {
        DWORD Id=0; GetWindowThreadProcessId(Candidate,&Id);
        if (Id==GetCurrentProcessId() && IsWindowVisible(Candidate) && !GetWindow(Candidate,GW_OWNER)) {
            RECT Rect{}; GetClientRect(Candidate,&Rect);
            if (Rect.right>320 && Rect.bottom>200) { *reinterpret_cast<HWND*>(Param)=Candidate; return FALSE; }
        }
        return TRUE;
    },reinterpret_cast<LPARAM>(&Window));
    if (!Window || !GraphicsOverlay::Initialize(Window,GameInput::Update)) {
        Logger::Log("[UI] Could not start the menu; unloading this copy");
        Logger::Cleanup(); FreeLibraryAndExitThread(Module,1);
    }
    wchar_t EventName[96]{};
    swprintf_s(EventName,L"Local\\SCCReadyOrNot.Stop.%lu",GetCurrentProcessId());
    HANDLE StopEvent=CreateEventW(nullptr,TRUE,FALSE,EventName);
    if (StopEvent) ResetEvent(StopEvent);
    bool HookInstalled=false;
    SDK::ULevel* LastLevel=nullptr;
    SDK::AHUD* IgnoreHUD=nullptr;
    int IgnoreHUDIndex=0;
    const auto Start=GetTickCount64();
    while (!MenuUI::UnloadRequested() && (!StopEvent || WaitForSingleObject(StopEvent,0)!=WAIT_OBJECT_0)) {
        const auto Commands=MenuUI::TakeCommands();
        if (Commands & MenuUI::Defaults) { Config::Set(Config::Settings{}); MenuUI::Notify("Varsayılan ayarlar uygulandı."); Logger::Log("[UI] Defaults restored"); }
        if (Commands & MenuUI::Load) { Config::Load(); MenuUI::Notify("Kayıtlı ayarlar yüklendi."); Logger::Log("[UI] Settings loaded"); }
        if (Commands & MenuUI::Save) { Config::Save(); MenuUI::Notify("Ayarlar kaydedildi."); Logger::Log("[UI] Settings saved"); }
        if (Commands & MenuUI::Dump) {
            std::lock_guard<std::mutex> Lock(GameData::DataMutex);
            for (const auto& Entity:GameData::CachedEntities)
                Logger::Log("[UI] Entity: type=%d | index=%d | actor=%p | objective=%d",static_cast<int>(Entity.Type),Entity.ActorIndex,Entity.Actor,static_cast<int>(Entity.MissionKind));
            MenuUI::Notify("Nesne listesi çalışma kaydına yazıldı.");
        }
        SafeLoopBody(HookInstalled,LastLevel,IgnoreHUD,IgnoreHUDIndex);
        MenuUI::Telemetry State; State.Connected=HookInstalled;
        { std::lock_guard<std::mutex> Lock(GameData::DataMutex); State.Entities=static_cast<int>(GameData::CachedEntities.size());
          for (const auto& Entity:GameData::CachedEntities) if (!Entity.MissionCaption.empty()) ++State.Targets; }
        MenuUI::SetTelemetry(State);
        if (!GraphicsOverlay::Ready() && GetTickCount64()-Start>20000) {
            Logger::Log("[UI] No render frames received; restoring the game and unloading"); break;
        }
        Sleep(50);
    }
    Logger::Log("[UI] Complete unload requested");
    Gameplay::Shutdown();
    while (!GraphicsOverlay::Shutdown()) Sleep(100);
    Hooks::Shutdown();
    if (StopEvent) CloseHandle(StopEvent);
    Logger::Log("[UI] Complete unload finished");
    Logger::Cleanup();
    FreeLibraryAndExitThread(Module,0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE Module,DWORD Reason,LPVOID)
{
    if (Reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(Module);
        if (HANDLE Thread=CreateThread(nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(MainThread),Module,0,nullptr)) CloseHandle(Thread);
    }
    return TRUE;
}
