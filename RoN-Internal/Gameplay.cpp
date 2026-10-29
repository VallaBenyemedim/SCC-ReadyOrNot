#include "Gameplay.hpp"
#include "GameplayUI.hpp"
#include "PatchLedger.hpp"
#include "GameplayPolicy.hpp"
#include "Config.hpp"
#include "MenuUI.hpp"
#include "Logger.hpp"
#include "SDK/ReadyOrNot_classes.hpp"
#include "SDK/CyberneticsSuspect_V2_classes.hpp"
#include "SDK/CyberneticsSwat_V2_classes.hpp"
#include "SDK/Cybernetics_Civilian_V2_classes.hpp"
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace Gameplay {
    namespace {
        using RuntimePatches::Owner;
        RuntimePatches::Ledger Changes;
        std::mutex Mutex;
        std::atomic<bool> Stop{false},Stopped{false};
        SDK::UWorld* LastWorld=nullptr;
        uint64_t LastTick=0,LastScan=0;
        struct Person {Owner Ref;int Kind;};
        struct DoorState {Owner Ref;bool Locked;};
        std::vector<Person> People;
        std::vector<Owner> Doors;
        std::vector<DoorState> DoorBackups;
        std::unordered_set<uint64_t> Forced,InjuriesCleared,Healed;
        std::unordered_map<uint64_t,uint64_t> SurrenderAttempts;
        Config::GameplaySettings Previous;
        uint64_t Name(const SDK::FName& N) {return static_cast<uint32_t>(N.ComparisonIndex)|(static_cast<uint64_t>(N.Number)<<32);}
        Owner Ref(SDK::UObject* O) {return {reinterpret_cast<uintptr_t>(O),O->Index,Name(O->Name),O->Outer?Name(O->Outer->Name):0};}
        bool Valid(Owner O) {
            __try {
                auto* Current=SDK::UObject::GObjects->GetByIndex(O.Index);
                return Current && reinterpret_cast<uintptr_t>(Current)==O.Address && Name(Current->Name)==O.Name
                    && (Current->Outer?Name(Current->Outer->Name):0)==O.OuterName;
            } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
        }
        uint64_t Id(Owner O) {return (static_cast<uint64_t>(static_cast<uint32_t>(O.Index))<<32)|static_cast<uint32_t>(O.Name>>32);}
        template<class T>T* Get(Owner O) {return Valid(O)?reinterpret_cast<T*>(O.Address):nullptr;}
        void Bit(SDK::UObject* O,size_t Offset,unsigned char Mask,bool On) {Changes.Bit(Ref(O),reinterpret_cast<unsigned char*>(O)+Offset,Mask,On);}
        template<class O,class T>void Set(O* Obj,T* Field,T Value) {Changes.Set(Ref(Obj),Field,Value);}
        template<class O>void Scale(O* Obj,float* Field,float Factor) {Set(Obj,Field,Changes.Original(Ref(Obj),Field)*Factor);}
        double Distance(SDK::FVector A,SDK::FVector B) {const auto X=A.X-B.X,Y=A.Y-B.Y,Z=A.Z-B.Z;return std::sqrt(X*X+Y*Y+Z*Z)/100.0;}
        void RestoreDoors(bool Native) {
            for(const auto& B:DoorBackups) if(auto* D=Get<SDK::ADoor>(B.Ref)) {
                if(!D->bLocked) {if(Native) D->SetLocked(B.Locked);else D->bLocked=B.Locked;}
            }
            DoorBackups.clear();
        }
        void Restore(bool Native) {
            RestoreDoors(Native);Changes.Restore(Valid);
            People.clear();Doors.clear();Forced.clear();InjuriesCleared.clear();Healed.clear();SurrenderAttempts.clear();
            GameplayUI::Clear();
        }
        void Scan(SDK::UWorld* World,SDK::APawn* Player) {
            People.clear();Doors.clear();
            for(auto* Level:World->Levels) if(Level) for(auto* Actor:Level->Actors) {
                if(!Actor || Actor==Player) continue;
                if(Actor->IsA(SDK::ADoor::StaticClass())) {Doors.push_back(Ref(Actor));continue;}
                int Kind=0;
                if(Actor->IsA(SDK::ACyberneticsSuspect_V2_C::StaticClass())) Kind=1;
                else if(Actor->IsA(SDK::ACybernetics_Civilian_V2_C::StaticClass())) Kind=2;
                else if(Actor->IsA(SDK::ACyberneticsSwat_V2_C::StaticClass())) Kind=3;
                if(Kind) People.push_back({Ref(Actor),Kind});
            }
        }
        void Health(SDK::AReadyOrNotCharacter* C) {
            if(!C || C->IsDeadOrUnconscious()) return;
            Bit(C,0x17B8,1,true); // bGodMode; layout checked by game_probe.
            Bit(C,0x62,4,false);  // AActor.bCanBeDamaged.
            if(auto* H=C->CharacterHealth) {
                Bit(H,0xFC,1,true);
                if(Healed.insert(Id(Ref(C))).second) H->SetCurrentResourceToMax();
                if(std::isfinite(H->MaxResource) && H->MaxResource>0) H->Resource=H->MaxResource;
            }
        }
        void Injuries(SDK::APlayerCharacter* C) {
            if(auto* H=C->CharacterHealth) {
                for(auto* Limb:{&H->RightLeg,&H->LeftLeg,&H->RightArm,&H->LeftArm,&H->Head}) Set(H,&Limb->LimbDamageMultiplier,0.0f);
                if(InjuriesCleared.insert(Id(Ref(C))).second) {H->ResetAllLimbHealth();H->ResetAllLimbTickets();}
            }
            if(C->BleedComponent && C->BleedComponent->IsBleeding()) C->BleedComponent->StopBleeding();
        }
        void Weapons(SDK::APlayerCharacter* P,const Config::GameplaySettings& G,GameplayUI::State& State) {
            auto* Item=P->InventoryComp?P->InventoryComp->GetEquippedItem():nullptr;
            if(!Item || !Item->IsA(SDK::ABaseWeapon::StaticClass())) return;
            auto* W=static_cast<SDK::ABaseWeapon*>(Item);State.Weapon=W->GetName();
            if(G.bNoRecoil) {
                for(auto* Field:{&W->ADSRecoilMultiplier,&W->FirstShotRecoil,&W->VelocityRecoilMultiplier,&W->RecoilMultiplierPitch,&W->RecoilMultiplierYaw,&W->RecoilFireStrength,&W->RecoilFireStrengthFirst,&W->RecoilAngleStrength,&W->RecoilBuildupADSModifier}) Scale(W,Field,G.RecoilScale);
                if(G.RecoilScale==0) {
                    Set(W,&W->bCalculateProcRecoil,false);
                    Set(P,&P->MeshspaceRecoilMovementMultiplier,SDK::FVector{});
                    P->PendingRecoil={};W->RecoilRotationBuildup={};W->RecoilPositionBuildup={};
                }
            }
            if(G.bNoReload && W->IsA(SDK::ABaseMagazineWeapon::StaticClass())) {
                auto* Gun=static_cast<SDK::ABaseMagazineWeapon*>(W);
                Bit(Gun,0x16FC,1,true);
                if(Gun->MagIndex>=0 && Gun->MagIndex<Gun->Magazines.Num() && Gun->Magazines.Num()<128)
                    Gun->Magazines[Gun->MagIndex].Ammo=static_cast<uint16_t>(std::clamp(static_cast<int>(Gun->AmmoMax),1,65535));
            }
        }
        void Movement(SDK::APlayerCharacter* P,const Config::GameplaySettings& G) {
            if(G.bMaxSpeed) {Set(P,&P->RunSpeed,G.MaxSpeed);Set(P,&P->MaxRunSpeedPercent,1.0f);}
            if(G.bCrouchSpeed) Scale(P,&P->SpeedModifier_Crouch,G.CrouchMultiplier);
            if(G.bSlowWalkSpeed && P->bWalking) Scale(P,&P->SlowDownSpeedMultiplier,G.SlowWalkMultiplier);
            if(G.bWalkSpeed) Scale(P,&P->WalkSpeedMultiplier,G.WalkMultiplier);
        }
        void Requests(SDK::APlayerCharacter* P,const Config::GameplaySettings& G) {
            for(const auto& Request:GameplayUI::Take()) {
                if(!G.bTeleport) continue;
                auto Found=std::find_if(People.begin(),People.end(),[&](const Person& N){return Id(N.Ref)==Request.NPC;});
                if(Found==People.end()) {MenuUI::Notify("NPC artık bu görevde değil.");continue;}
                auto* Target=Get<SDK::AReadyOrNotCharacter>(Found->Ref);
                if(!Target || Target->IsDeadOrUnconscious()) {MenuUI::Notify("Geçerli, canlı bir NPC seç.");continue;}
                SDK::AActor* Mover=Request.Type==GameplayUI::Action::TeleportToNPC?static_cast<SDK::AActor*>(P):Target;
                SDK::AActor* Anchor=Mover==P?static_cast<SDK::AActor*>(Target):P;
                const auto Origin=Anchor->K2_GetActorLocation(),Forward=Anchor->GetActorForwardVector();
                SDK::FVector Dest{Origin.X+Forward.X*G.TeleportDistance*100,Origin.Y+Forward.Y*G.TeleportDistance*100,Origin.Z};
                const bool OK=Mover->K2_TeleportTo(Dest,Mover->K2_GetActorRotation());
                MenuUI::Notify(OK?"Işınlanma tamamlandı.":"Konum engelli; mesafeyi değiştirip yeniden dene.");
                Logger::Log("[Gameplay] Teleport %s: NPC %d, result %s",Mover==P?"player to NPC":"NPC to player",Found->Ref.Index,OK?"OK":"blocked");
            }
        }
        void TickImpl() {
            std::lock_guard<std::mutex> Lock(Mutex);
            if(Stop.load()) {Restore(true);Stopped=true;return;}
            const auto Now=GetTickCount64();if(Now-LastTick<40) return;LastTick=Now;
            auto* World=SDK::UWorld::GetWorld();
            if(World!=LastWorld) {Restore(true);LastWorld=World;LastScan=0;}
            auto* GI=World?World->OwningGameInstance:nullptr;
            auto* LP=GI && GI->LocalPlayers.Num()>0?GI->LocalPlayers[0]:nullptr;
            auto* PC=LP?LP->PlayerController:nullptr;
            auto* Pawn=PC?PC->Pawn:nullptr;
            if(!Pawn || !Pawn->IsA(SDK::APlayerCharacter::StaticClass())) {Restore(true);return;}
            auto* Player=static_cast<SDK::APlayerCharacter*>(Pawn);
            const auto G=Config::Snapshot().Gameplay;
            if(Now-LastScan>500) {Scan(World,Pawn);LastScan=Now;}
            GameplayUI::State State;State.Ready=true;State.Authority=PC->HasAuthority();State.Doors=static_cast<int>(Doors.size());
            if(auto* H=Player->CharacterHealth) {State.Health=H->Resource;State.MaxHealth=H->MaxResource;}
            const auto Origin=Player->K2_GetActorLocation();
            Changes.Begin();
            if(State.Authority) {
                Weapons(Player,G,State);Movement(Player,G);
                if(G.bPlayerHealth) Health(Player);else Healed.erase(Id(Ref(Player)));
                if(G.bNoInjuries) Injuries(Player);else InjuriesCleared.clear();
            }
            if(!G.bForceSurrender) Forced.clear();
            if(!G.bForceSurrender && !G.bAlwaysSurrender) SurrenderAttempts.clear();
            for(const auto& N:People) if(auto* C=Get<SDK::AReadyOrNotCharacter>(N.Ref)) {
                if(C->IsDeadOrUnconscious()) continue;
                const auto Position=C->K2_GetActorLocation();
                if(Position.Z>-1050 && Position.Z<-950 && std::abs(Position.X)<50 && std::abs(Position.Y)<50) continue;
                const float Meters=static_cast<float>(Distance(Origin,Position));
                auto Display=C->SpeechCharacterName.ToString();
                if(Display.empty() || Display=="None") Display=std::string(N.Kind==1?"Düşman":N.Kind==2?"Sivil":"Takım")+" #"+std::to_string(N.Ref.Index);
                State.People.push_back({Id(N.Ref),std::move(Display),Meters,N.Kind});
                if(N.Kind==3) {++State.Allies;if(State.Authority && G.bTeamHealth) Health(C);else Healed.erase(Id(N.Ref));}
                if(N.Kind!=1) continue;
                ++State.Suspects;
                if(!State.Authority || (G.NPCRange>0 && Meters>G.NPCRange) || C->IsArrested()) continue;
                auto* AI=static_cast<SDK::ACyberneticCharacter*>(C);
                if(G.bAlwaysSurrender) {
                    Set(AI,&AI->bIsFakeSurrender,false);
                    if(C->Controller && C->Controller->IsA(SDK::ACyberneticController::StaticClass()))
                        if(auto* M=static_cast<SDK::ACyberneticController*>(C->Controller)->MoraleComponent) Set(M,&M->Resource,0.0f);
                }
                const auto NPCId=Id(N.Ref);
                const bool ForceOnce=G.bForceSurrender && !Forced.contains(NPCId);
                if(C->bSurrendered && ForceOnce) Forced.insert(NPCId);
                if((G.bAlwaysSurrender || ForceOnce) && GameplayPolicy::CanRequestSurrender(Now,SurrenderAttempts[NPCId],AI->GetIsSurrenderAnimPlaying(),C->bSurrendered)) {
                    AI->Surrender();SurrenderAttempts[NPCId]=Now;if(ForceOnce) Forced.insert(NPCId);
                }
                if(G.bDropWeapons && C->InventoryComp) {
                    auto* Inventory=C->InventoryComp;bool HasWeapon=false;
                    for(auto* Item:Inventory->InventoryItems) if(Item && Item->bInInventory && Item->IsA(SDK::ABaseWeapon::StaticClass())) {HasWeapon=true;break;}
                    if(HasWeapon) Inventory->ThrowAllWeapons();
                }
            }
            if(State.Authority && G.bUnlockDoors) {
                for(auto RefDoor:Doors) if(auto* D=Get<SDK::ADoor>(RefDoor)) {
                    if(std::none_of(DoorBackups.begin(),DoorBackups.end(),[&](const DoorState& B){return B.Ref==RefDoor;})) DoorBackups.push_back({RefDoor,D->bLocked!=0});
                    Bit(D,0x780,1,false);
                    if(D->bLocked) D->SetLocked(false);
                }
            } else RestoreDoors(true);
            if(State.Authority) Requests(Player,G);else GameplayUI::Take();
            Changes.Finish(Valid);State.ActivePatches=static_cast<int>(Changes.Size());
            std::sort(State.People.begin(),State.People.end(),[](const GameplayUI::NPC& A,const GameplayUI::NPC& B){return A.Distance<B.Distance;});
            GameplayUI::Publish(std::move(State));
            if(G!=Previous) {Logger::Log("[Gameplay] Controls applied: recoil=%d reload=%d health=%d injuries=%d team=%d movement=%d%d%d%d surrender=%d/%d drop=%d doors=%d",G.bNoRecoil,G.bNoReload,G.bPlayerHealth,G.bNoInjuries,G.bTeamHealth,G.bMaxSpeed,G.bCrouchSpeed,G.bSlowWalkSpeed,G.bWalkSpeed,G.bForceSurrender,G.bAlwaysSurrender,G.bDropWeapons,G.bUnlockDoors);Previous=G;}
        }
    }
    void Tick() {
        __try {TickImpl();}
        __except(EXCEPTION_EXECUTE_HANDLER) {static int Errors=0;if(++Errors<=4) Logger::Log("[Gameplay] Frame skipped after invalid game object (%08X)",GetExceptionCode());}
    }
    void Shutdown() {
        Stop=true;
        for(int I=0;I<150 && !Stopped;++I) Sleep(10);
        if(!Stopped) {std::lock_guard<std::mutex> Lock(Mutex);Restore(false);Stopped=true;}
        Logger::Log("[Gameplay] Owned gameplay values restored");
    }
}
