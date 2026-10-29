#include "ESP.hpp"
#include "Renderer.hpp"
#include "GameData.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "ESPPresentation.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

// Specific Entity Headers go here
#include "SDK/CyberneticsSuspect_V2_classes.hpp"
#include "SDK/CyberneticsSwat_V2_classes.hpp"
#include "SDK/Cybernetics_Civilian_V2_classes.hpp"
#include "SDK/ReadyOrNot_classes.hpp"

namespace ESP
{
    namespace {
        constexpr size_t MaxBodyJoints = 64;
        struct BodyJoint {
            SDK::FName Name;
            int Parent = -1;
        };
        struct SkeletonDefinition {
            SDK::USkinnedAsset* Asset = nullptr;
            int ObjectIndex = 0;
            std::vector<BodyJoint> Joints;
        };
        std::vector<SkeletonDefinition> Skeletons;

        uint64_t NameKey(const SDK::FName& Name) {
            return static_cast<uint32_t>(Name.ComparisonIndex) | (static_cast<uint64_t>(Name.Number) << 32);
        }

        const SkeletonDefinition* GetSkeleton(SDK::USkeletalMeshComponent* Mesh) {
            auto* Asset = Mesh->GetSkinnedAsset();
            if (!Asset) return nullptr;
            for (const auto& Definition : Skeletons)
                if (Definition.Asset == Asset && Definition.ObjectIndex == Asset->Index) return &Definition;

            const int Count = Mesh->GetNumBones();
            if (Count < 2 || Count > 1024) return nullptr;
            std::vector<SDK::FName> BoneNames;
            std::vector<std::string> Names;
            std::unordered_map<uint64_t, int> BoneIndices;
            BoneNames.reserve(Count);
            Names.reserve(Count);
            for (int i = 0; i < Count; ++i) {
                auto Name = Mesh->GetBoneName(i);
                BoneNames.push_back(Name);
                Names.push_back(Name.ToString());
                BoneIndices.emplace(NameKey(Name), i);
            }
            std::vector<int> Parents(Count, -1);
            for (int i = 0; i < Count; ++i) {
                const auto Parent = Mesh->GetParentBone(BoneNames[i]);
                if (Parent.IsNone()) continue;
                const auto Found = BoneIndices.find(NameKey(Parent));
                if (Found != BoneIndices.end()) Parents[i] = Found->second;
            }

            SkeletonDefinition Definition;
            Definition.Asset = Asset;
            Definition.ObjectIndex = Asset->Index;
            std::vector<int> BodyIndices(Count, -1);
            for (int i = 0; i < Count && Definition.Joints.size() < MaxBodyJoints; ++i) {
                if (!ESPPresentation::IsBodyBone(Names[i])) continue;
                BodyIndices[i] = static_cast<int>(Definition.Joints.size());
                Definition.Joints.push_back({BoneNames[i], -1});
            }
            size_t Links = 0;
            for (const auto& [Child, Parent] : ESPPresentation::BoneLinks(Names, Parents)) {
                if (BodyIndices[Child] >= 0 && BodyIndices[Parent] >= 0) {
                    Definition.Joints[BodyIndices[Child]].Parent = BodyIndices[Parent];
                    ++Links;
                }
            }
            Logger::Log("[+] Skeleton layout %s: %d bones, %zu body joints, %zu links",
                Asset->GetName().c_str(), Count, Definition.Joints.size(), Links);
            if (Links < 4) {
                Logger::Log("[!] Skeleton layout not recognised; keeping the type label and box");
                Definition.Joints.clear();
            }
            if (Skeletons.size() >= 128) Skeletons.clear();
            Skeletons.push_back(std::move(Definition));
            return &Skeletons.back();
        }

        struct ScreenBounds {
            double Left = (std::numeric_limits<double>::max)();
            double Top = (std::numeric_limits<double>::max)();
            double Right = (std::numeric_limits<double>::lowest)();
            double Bottom = (std::numeric_limits<double>::lowest)();
            int Points = 0;
            void Include(SDK::FVector2D Point) {
                Left = (std::min)(Left, Point.X);
                Top = (std::min)(Top, Point.Y);
                Right = (std::max)(Right, Point.X);
                Bottom = (std::max)(Bottom, Point.Y);
                ++Points;
            }
        };
    }

    SDK::FLinearColor GetColorFromType(EEntityType Type, EEntityStatus Status, const Config::ESPSettings& Settings)
    {
        if (Status == EEntityStatus::Dead) return { 0.75f, 0.75f, 0.75f, 1.0f };
        if (Status == EEntityStatus::TrapDisabled) return { 0.75f, 0.75f, 0.75f, 1.0f };
        if (Status == EEntityStatus::Arrested) {
             Config::Color C = Settings.ColArrested;
             return { C.R, C.G, C.B, C.A };
        }

        Config::Color C = { 1.0f, 1.0f, 1.0f, 1.0f };
        switch (Type)
        {
            case EEntityType::Suspect: C = Settings.ColSuspect; break;
            case EEntityType::Civilian: C = Settings.ColCivilian; break;
            case EEntityType::Ally: C = Settings.ColAlly; break;
            case EEntityType::Trap: C = Settings.ColTrap; break;
            case EEntityType::Evidence: C = Settings.ColEvidence; break;
            case EEntityType::DroppedWeapon: C = Settings.ColDroppedWeapon; break;
            case EEntityType::Reportable: C = Settings.ColReportable; break;
            case EEntityType::Bomb: C = Settings.ColBomb; break;
            case EEntityType::Computer: case EEntityType::MissionObject: C = Settings.ColObjective; break;
        }
        return { C.R, C.G, C.B, C.A };
    }

    void DrawEntity(SDK::UCanvas* Canvas, SDK::APlayerController* PC, const CachedEntity& Entity,
                    const SDK::FVector& ViewerOrigin, bool HasViewer)
    {
        if (!Entity.Actor) return;
        if (Entity.ActorIndex >= 0 && Entity.Actor->Index != Entity.ActorIndex) return;
        if (SDK::UObject::GObjects->GetByIndex(Entity.Actor->Index) != Entity.Actor) return;
        auto Origin = Entity.Actor->K2_GetActorLocation();
        if (Entity.Type == EEntityType::DroppedWeapon)
        {
            auto* Item = static_cast<SDK::ABaseItem*>(Entity.Actor);
            if (Item->ItemMesh) Origin = Item->ItemMesh->K2_GetComponentLocation();
        }
        const auto Settings = Config::Snapshot().ESP;
        const auto Color = GetColorFromType(Entity.Type, Entity.Status, Settings);
        const bool MissionTarget = Settings.bObjectives && !Entity.MissionCaption.empty();
        const auto GoalColor = Settings.ColObjective;
        const SDK::FLinearColor Highlight{GoalColor.R, GoalColor.G, GoalColor.B, GoalColor.A};
        const bool Person = ESPPresentation::IsPerson(Entity.Type);
        ScreenBounds Bounds;
        std::array<SDK::FVector2D, MaxBodyJoints> Points{};
        std::array<SDK::FVector, MaxBodyJoints> WorldPoints{};
        std::array<bool, MaxBodyJoints> Projected{};
        const SkeletonDefinition* Skeleton = nullptr;
        if (Person) {
            auto* Character = static_cast<SDK::ACharacter*>(Entity.Actor);
            if (Character->Mesh) Skeleton = GetSkeleton(Character->Mesh);
            if (Skeleton) {
                for (size_t i = 0; i < Skeleton->Joints.size(); ++i) {
                    auto WorldPoint = Character->Mesh->GetSocketLocation(Skeleton->Joints[i].Name);
                    // Reject an invalid pose rather than drawing a line to the world origin.
                    const double DX = WorldPoint.X - Origin.X, DY = WorldPoint.Y - Origin.Y, DZ = WorldPoint.Z - Origin.Z;
                    if (!std::isfinite(DX + DY + DZ) || DX*DX + DY*DY + DZ*DZ > 1000.0*1000.0) continue;
                    Projected[i] = Renderer::WorldToScreen(PC, WorldPoint, Points[i]);
                    WorldPoints[i] = WorldPoint;
                    if (Projected[i]) Bounds.Include(Points[i]);
                }
            }
        }
        if (Bounds.Points < 4) {
            Bounds = {};
            double HalfHeight = Person ? 90.0 : 20.0;
            if (Entity.Type == EEntityType::Trap || Entity.Type == EEntityType::Reportable) HalfHeight = 30.0;
            auto Head = Origin, Feet = Origin;
            Head.Z += HalfHeight;
            Feet.Z -= HalfHeight;
            SDK::FVector2D Top, Bottom;
            if (!Renderer::WorldToScreen(PC, Head, Top) || !Renderer::WorldToScreen(PC, Feet, Bottom)) return;
            Bounds.Top = (std::min)(Top.Y, Bottom.Y);
            Bounds.Bottom = (std::max)(Top.Y, Bottom.Y);
            const double HalfWidth = (std::max)(6.0, (Bounds.Bottom - Bounds.Top) * (Person ? 0.25 : 0.5));
            Bounds.Left = (Top.X + Bottom.X) * 0.5 - HalfWidth;
            Bounds.Right = Bounds.Left + HalfWidth * 2.0;
        } else {
            Bounds.Left -= 5.0;
            Bounds.Right += 5.0;
            Bounds.Top -= 9.0;
            Bounds.Bottom += 5.0;
        }
        if (Bounds.Bottom < 0 || Bounds.Top > Canvas->ClipY || Bounds.Right < 0 || Bounds.Left > Canvas->ClipX) return;
        if (Bounds.Right - Bounds.Left > Canvas->ClipX * 4.0 || Bounds.Bottom - Bounds.Top > Canvas->ClipY * 4.0) return;

        if (Settings.bBox) Renderer::DrawBox(Canvas, {Bounds.Left, Bounds.Top},
            {Bounds.Right - Bounds.Left, Bounds.Bottom - Bounds.Top}, Settings.BoxThickness, MissionTarget ? Highlight : Color);
        int SegmentsDrawn = 0;
        if (Settings.bSkeleton && Skeleton) {
            for (size_t i = 0; i < Skeleton->Joints.size(); ++i) {
                const int Parent = Skeleton->Joints[i].Parent;
                if (Parent < 0 || !Projected[i] || !Projected[Parent]) continue;
                const auto A=WorldPoints[i],B=WorldPoints[Parent];
                const double X=A.X-B.X,Y=A.Y-B.Y,Z=A.Z-B.Z;
                if (X*X+Y*Y+Z*Z>150.0*150.0) continue;
                auto BoneColor=Color; BoneColor.A*=Settings.SkeletonOpacity;
                Renderer::DrawLine(Canvas, Points[i], Points[Parent], Settings.SkeletonThickness, BoneColor);
                ++SegmentsDrawn;
            }
        }
        static bool SkeletonDrawLogged = false;
        if (SegmentsDrawn >= 4 && !SkeletonDrawLogged) {
            Logger::Log("[+] Skeleton rendering active: %d live bone segments", SegmentsDrawn);
            SkeletonDrawLogged = true;
        }
        if (Settings.bNameTags) {
            int Distance = -1;
            if (Settings.bDistance && HasViewer) {
                const double DX = Origin.X - ViewerOrigin.X, DY = Origin.Y - ViewerOrigin.Y, DZ = Origin.Z - ViewerOrigin.Z;
                const double Meters = std::sqrt(DX*DX + DY*DY + DZ*DZ) / 100.0;
                if (std::isfinite(Meters) && Meters < 100000.0) Distance = static_cast<int>(std::lround(Meters));
            }
            auto Text = ESPPresentation::Label(Entity.Type, Entity.Status, Distance, Entity.Handgun);
            if (Entity.Type == EEntityType::Bomb && Entity.BombSeconds >= 0) {
                wchar_t Timer[32]{};
                swprintf_s(Timer, L" | %02d:%02d", Entity.BombSeconds / 60, Entity.BombSeconds % 60);
                Text += Timer;
            }
            if (MissionTarget) {
                if (Renderer::DrawString(Canvas, Entity.MissionCaption,
                    {(Bounds.Left + Bounds.Right) * 0.5, Bounds.Top}, Highlight, Settings.TextScale, true, 100)) {
                    static bool MissionDrawLogged = false;
                    if (!MissionDrawLogged) {
                        Logger::Log("[+] Mission target labels rendering active");
                        MissionDrawLogged = true;
                    }
                }
            }
            if (Renderer::DrawString(Canvas, Text, {(Bounds.Left + Bounds.Right) * 0.5, Bounds.Bottom}, Color, Settings.TextScale, false, MissionTarget?80:Person?40:20)) {
                if (Entity.Handgun) {
                    static bool HandgunLabelLogged=false;
                    if (!HandgunLabelLogged) {Logger::Log("[ESP] Handgun label queued: Tabanca");HandgunLabelLogged=true;}
                }
                static unsigned LabelsLogged = 0;
                const unsigned TypeBit = 1u << static_cast<unsigned>(Entity.Type);
                if (!(LabelsLogged & TypeBit)) {
                    Logger::Log("[+] Type label rendering active: type=%d, Turkish text enabled", static_cast<int>(Entity.Type));
                    LabelsLogged |= TypeBit;
                }
            }
        }
    }

    void SafeDrawEntity(SDK::UCanvas* Canvas, SDK::APlayerController* PC, const CachedEntity& Entity,
                        const SDK::FVector& ViewerOrigin, bool HasViewer) {
        __try { DrawEntity(Canvas, PC, Entity, ViewerOrigin, HasViewer); }
        __except (EXCEPTION_EXECUTE_HANDLER) { /* A despawned actor only skips its own draw. */ }
    }

    void Draw(SDK::UCanvas* Canvas)
    {
        Renderer::BeginFrame(Canvas);
        struct PublishFrame {~PublishFrame(){Renderer::EndFrame();}} Publish;
        if (!Config::Snapshot().ESP.bEnabled || !Canvas) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!World || !World->OwningGameInstance || World->OwningGameInstance->LocalPlayers.Num() == 0 || !World->OwningGameInstance->LocalPlayers[0]) return;
        SDK::APlayerController* PC = World->OwningGameInstance->LocalPlayers[0]->PlayerController;
        if (!PC) return;
        const bool HasViewer = PC->Pawn != nullptr;
        const auto ViewerOrigin = HasViewer ? PC->Pawn->K2_GetActorLocation() : SDK::FVector{};

        std::vector<CachedEntity> LocalList;
        
        {
            std::lock_guard<std::mutex> Lock(GameData::DataMutex);
            LocalList = GameData::CachedEntities;
        }

        for (const CachedEntity& Ent : LocalList)
        {
            SafeDrawEntity(Canvas, PC, Ent, ViewerOrigin, HasViewer);
        }
        static bool FirstDrawLogged = false;
        if (!FirstDrawLogged && !LocalList.empty()) {
            Logger::Log("[+] ESP rendering active: %zu cached entities", LocalList.size());
            FirstDrawLogged = true;
        }
    }
}
