#include "GameInput.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/UMG_classes.hpp"
#include "Logger.hpp"

namespace GameInput {
    namespace {
        SDK::APlayerController* Controller=nullptr;
        int ControllerIndex=-1;
        bool Applied=false, OriginalCursor=false;
        bool ValidController() {
            return Controller && SDK::UObject::GObjects->GetByIndex(ControllerIndex)==Controller;
        }
        void SetMode(SDK::APlayerController* PC,bool UI) {
            auto* Class=SDK::UWidgetBlueprintLibrary::StaticClass();
            if (!Class) return;
            auto* Function=Class->GetFunction("WidgetBlueprintLibrary",UI?"SetInputMode_GameAndUIEx":"SetInputMode_GameOnly");
            auto* Default=SDK::UWidgetBlueprintLibrary::GetDefaultObj();
            if (!Function || !Default) return;
            struct UIParams { SDK::APlayerController* Player; SDK::UWidget* Focus; SDK::EMouseLockMode Lock; bool Hide; bool Flush; };
            struct GameParams { SDK::APlayerController* Player; bool Flush; };
            static_assert(offsetof(UIParams,Lock)==16 && offsetof(UIParams,Flush)==18);
            static_assert(offsetof(GameParams,Flush)==8);
            const auto Flags=Function->FunctionFlags;
            Function->FunctionFlags|=0x400;
            if (UI) {
                UIParams Params{PC,nullptr,SDK::EMouseLockMode::DoNotLock,false,true};
                Default->ProcessEvent(Function,&Params);
            } else {
                GameParams Params{PC,true}; Default->ProcessEvent(Function,&Params);
            }
            Function->FunctionFlags=Flags;
        }
        void UpdateImpl(bool Open) {
            auto* World=SDK::UWorld::GetWorld();
            SDK::APlayerController* Current=nullptr;
            if (World && World->OwningGameInstance && World->OwningGameInstance->LocalPlayers.Num()>0
                && World->OwningGameInstance->LocalPlayers[0]) Current=World->OwningGameInstance->LocalPlayers[0]->PlayerController;
            if (Applied && (!Open || Current!=Controller || !ValidController())) {
                if (ValidController()) {
                    Controller->SetIgnoreLookInput(false); Controller->SetIgnoreMoveInput(false);
                    Controller->bShowMouseCursor=OriginalCursor;
                    SetMode(Controller,OriginalCursor);
                }
                Applied=false; Controller=nullptr; ControllerIndex=-1;
                Logger::Log("[UI] Game input restored");
            }
            if (Open && Current && !Applied) {
                Controller=Current; ControllerIndex=Current->Index; OriginalCursor=Current->bShowMouseCursor;
                Current->bShowMouseCursor=true;
                SetMode(Current,true);
                Current->SetIgnoreLookInput(true); Current->SetIgnoreMoveInput(true);
                ClipCursor(nullptr);
                Applied=true;
                Logger::Log("[UI] Menu input active; camera and movement blocked");
            }
        }
    }
    void Update(bool MenuOpen) {
        __try { UpdateImpl(MenuOpen); }
        __except(EXCEPTION_EXECUTE_HANDLER) { /* Level changes can invalidate the old controller. */ }
    }
}
