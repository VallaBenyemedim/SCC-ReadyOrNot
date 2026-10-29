#include "MissionTargets.hpp"
#include "SDK/ReadyOrNot_classes.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include <unordered_map>
#include <unordered_set>

namespace MissionTargets {
    namespace {
        SDK::ULevel* LastLevel = nullptr;
        struct TextCache { int Index; std::wstring Title, Description, Tag; };
        std::unordered_map<SDK::AObjective*, TextCache> Texts;
        std::unordered_set<uint64_t> LoggedTargets;

        uint64_t NameKey(const SDK::FName& Name) {
            return static_cast<uint32_t>(Name.ComparisonIndex) | (static_cast<uint64_t>(Name.Number) << 32);
        }

        bool CopyText(const SDK::FText& Text, wchar_t* Buffer, int Capacity) {
            __try {
                if (!Text.TextData) return false;
                const auto& Source = Text.GetStringRef();
                if (Source.Num() <= 0 || Source.Num() > 4096 || !Source.CStr()) return false;
                int Length = (std::min)(Source.Num() - 1, Capacity - 1);
                for (int i = 0; i < Length; ++i) Buffer[i] = Source.CStr()[i];
                Buffer[Length] = L'\0';
                return true;
            } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }
        std::wstring ReadText(const SDK::FText& Text) {
            wchar_t Buffer[1024]{};
            return CopyText(Text, Buffer, 1024) ? std::wstring(Buffer) : L"";
        }
        std::wstring Wide(const std::string& Text) {
            int Count = MultiByteToWideChar(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), nullptr, 0);
            std::wstring Out(Count, L'\0');
            if (Count) MultiByteToWideChar(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), Out.data(), Count);
            return Out;
        }
        std::string Utf8(const std::wstring& Text) {
            int Count = WideCharToMultiByte(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
            std::string Out(Count, '\0');
            if (Count) WideCharToMultiByte(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), Out.data(), Count, nullptr, nullptr);
            return Out;
        }
    }

    Snapshot Read(SDK::UWorld* World) {
        Snapshot Result;
        const auto Settings = Config::Snapshot().ESP;
        if (!World || !World->GameState || !Settings.bObjectives) return Result;
        if (!World->GameState->IsA(SDK::AReadyOrNotGameState::StaticClass())) return Result;
        if (LastLevel != World->PersistentLevel) {
            Texts.clear();
            LoggedTargets.clear();
            LastLevel = World->PersistentLevel;
        }
        auto* State = static_cast<SDK::AReadyOrNotGameState*>(World->GameState);
        if (State->IsA(SDK::ACoopGS::StaticClass())) Result.Mode = static_cast<int>(static_cast<SDK::ACoopGS*>(State)->Mode);
        if (State->MissionObjectives.Num() < 0 || State->MissionObjectives.Num() > 256) return Result;
        for (auto* Objective : State->MissionObjectives) {
            if (!Objective) continue;
            MissionModel::Rule Rule;
            Rule.Status = static_cast<int>(Objective->ObjectiveStatus);
            Rule.Hidden = Objective->bHiddenObjective;
            Rule.LockedMode = static_cast<int>(Objective->LockedToMode);
            SDK::FName Tag;
            if (Objective->IsA(SDK::ANeutralizeSuspectByTag::StaticClass())) {
                auto* Goal = static_cast<SDK::ANeutralizeSuspectByTag*>(Objective);
                Rule.Action = Goal->bRequireArrest ? MissionModel::Kind::Arrest : MissionModel::Kind::Neutralize;
                Tag = Goal->SuspectTag;
                if (Goal->suspect) Rule.Actors.push_back(reinterpret_cast<uint64_t>(Goal->suspect));
            } else if (Objective->IsA(SDK::ARescueCivilianByTag::StaticClass())) {
                auto* Goal = static_cast<SDK::ARescueCivilianByTag*>(Objective);
                Rule.Action = MissionModel::Kind::Rescue;
                Tag = Goal->CivilianTag;
                if (Goal->civilian) Rule.Actors.push_back(reinterpret_cast<uint64_t>(Goal->civilian));
            } else if (Objective->IsA(SDK::APickupEvidenceByTag::StaticClass())) {
                Rule.Action = MissionModel::Kind::Collect;
                Tag = static_cast<SDK::APickupEvidenceByTag*>(Objective)->EvidenceTag;
            } else if (Objective->IsA(SDK::AReportReportableByTag::StaticClass())) {
                Rule.Action = MissionModel::Kind::Report;
                Tag = static_cast<SDK::AReportReportableByTag*>(Objective)->ReportTag;
            } else if (Objective->IsA(SDK::ADefuseBombThreats::StaticClass())) {
                Rule.Action = MissionModel::Kind::Defuse;
            } else if (Objective->IsA(SDK::AInteractWithDoorObjective::StaticClass())) {
                auto* Goal = static_cast<SDK::AInteractWithDoorObjective*>(Objective);
                Rule.Action = MissionModel::Kind::Interact;
                Tag = Goal->DoorTag;
                if (Goal->Doors.Num() >= 0 && Goal->Doors.Num() < 1024)
                    for (auto* Door : Goal->Doors) if (Door) Rule.Actors.push_back(reinterpret_cast<uint64_t>(Door));
            }
            if (!MissionModel::Active(Rule, Result.Mode)) continue;
            if ((Rule.Action == MissionModel::Kind::Defuse && !Settings.bBombTargets)
                || ((Rule.Action == MissionModel::Kind::Rescue || Rule.Action == MissionModel::Kind::Arrest || Rule.Action == MissionModel::Kind::Neutralize) && !Settings.bNamedTargets)
                || ((Rule.Action == MissionModel::Kind::Collect || Rule.Action == MissionModel::Kind::Report || Rule.Action == MissionModel::Kind::Interact) && !Settings.bObjectiveObjects)) continue;
            Rule.Tag = Tag.IsNone() ? 0 : NameKey(Tag);
            auto Found = Texts.find(Objective);
            if (Found == Texts.end() || Found->second.Index != Objective->Index) {
                TextCache Value{Objective->Index, ReadText(Objective->ObjectiveName), ReadText(Objective->ObjectiveDescription),
                    Tag.IsNone() ? L"" : Wide(Tag.ToString())};
                Found = Texts.insert_or_assign(Objective, std::move(Value)).first;
            }
            Rule.Title = Found->second.Title;
            Rule.Description = Found->second.Description;
            Rule.TagText = Found->second.Tag;
            Result.Rules.push_back(std::move(Rule));
        }
        return Result;
    }

    std::wstring ObjectName(SDK::AActor* Actor) {
        if (Actor->IsA(SDK::AReportableActor::StaticClass()))
            return ReadText(static_cast<SDK::AReportableActor*>(Actor)->ReportableName);
        if (Actor->IsA(SDK::AEvidenceActor::StaticClass()))
            return ReadText(static_cast<SDK::AEvidenceActor*>(Actor)->EvidenceName);
        return L"";
    }

    bool Apply(CachedEntity& Entity, const Snapshot& Mission, MissionModel::Category Category) {
        if (!Entity.Actor || Mission.Rules.empty()) return false;
        MissionModel::Candidate Candidate;
        Candidate.Actor = reinterpret_cast<uint64_t>(Entity.Actor);
        Candidate.Type = Category;
        if (Entity.Actor->Tags.Num() >= 0 && Entity.Actor->Tags.Num() < 1024)
            for (const auto& Tag : Entity.Actor->Tags) Candidate.Tags.push_back(NameKey(Tag));
        for (const auto& Rule : Mission.Rules) {
            if (!MissionModel::Matches(Rule, Mission.Mode, Candidate)) continue;
            auto Name = ObjectName(Entity.Actor);
            const auto Caption = MissionModel::Caption(Rule, Name);
            if (Entity.MissionCaption.empty()) {
                Entity.MissionCaption = Caption;
                Entity.MissionKind = Rule.Action;
            } else if (Entity.MissionCaption.find(Caption) == std::wstring::npos)
                Entity.MissionCaption += L" / " + Caption;
            // Name-based categorisation is allowed only after a real mission match.
            if (Category != MissionModel::Category::Person && MissionModel::ComputerName(Name + L" " + Rule.Title))
                Entity.Type = EEntityType::Computer;
            const uint64_t Key = (static_cast<uint64_t>(static_cast<uint32_t>(Entity.Actor->Index)) << 8) | static_cast<unsigned>(Rule.Action);
            if (LoggedTargets.insert(Key).second)
                Logger::Log("[+] Mission target: %s | actor=%s", Utf8(Caption).c_str(), Entity.Actor->GetName().c_str());
        }
        return !Entity.MissionCaption.empty();
    }
}
