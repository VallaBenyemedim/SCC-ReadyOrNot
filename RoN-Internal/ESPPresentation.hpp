#pragma once
#include "EntityTypes.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace ESPPresentation {
    inline bool IsPerson(EEntityType Type) {
        return Type == EEntityType::Suspect || Type == EEntityType::Civilian || Type == EEntityType::Ally;
    }

    inline const wchar_t* TypeLabel(EEntityType Type) {
        switch (Type) {
        case EEntityType::Suspect: return L"D\u00fc\u015fman";
        case EEntityType::Civilian: return L"Sivil";
        case EEntityType::Ally: return L"M\u00fcttefik";
        case EEntityType::Trap: return L"Tuzak";
        case EEntityType::Evidence: return L"Kan\u0131t";
        case EEntityType::DroppedWeapon: return L"Silah";
        case EEntityType::Reportable: return L"Bildirilecek Nesne";
        case EEntityType::Bomb: return L"Bomba";
        case EEntityType::Computer: return L"Bilgisayar";
        case EEntityType::MissionObject: return L"G\u00f6rev Noktas\u0131";
        default: return L"Bilinmeyen";
        }
    }

    inline std::wstring Label(EEntityType Type, EEntityStatus Status, int DistanceMeters = -1, bool Handgun = false) {
        std::wstring Text = Type==EEntityType::DroppedWeapon && Handgun?L"Tabanca":TypeLabel(Type);
        if (Status == EEntityStatus::Arrested) Text += L" | Kelep\u00e7eli";
        else if (Status == EEntityStatus::Dead) Text += L" | Etkisiz";
        else if (Status == EEntityStatus::TrapDisabled) Text += L" | Devre D\u0131\u015f\u0131";
        else if (Status == EEntityStatus::TrapLive) Text += L" | Aktif";
        if (DistanceMeters >= 0) Text += L" | " + std::to_wstring(DistanceMeters) + L" m";
        return Text;
    }

    inline bool IsBodyBone(const std::string& Name) {
        std::string Key;
        for (unsigned char Ch : Name)
            if (std::isalnum(Ch)) Key += static_cast<char>(std::tolower(Ch));
        constexpr const char* Excluded[] = {
            "twist", "roll", "weapon", "attach", "socket", "cloth", "finger", "thumb",
            "index", "middle", "ring", "pinky", "face", "brow", "lid", "jaw", "teeth",
            "tongue", "nose", "eye", "hair", "breast", "helper", "toe", "end",
            "hlp", "extra", "equipment", "accessory", "corrective", "bulge"
        };
        if (Key.starts_with("ik") || Key.starts_with("vb")) return false;
        for (const auto* Word : Excluded)
            if (Key.find(Word) != std::string::npos) return false;
        constexpr const char* Included[] = {
            "pelvis", "hips", "spine", "neck", "head", "clavicle", "collar",
            "upperarm", "lowerarm", "forearm", "hand", "thigh", "upperleg", "calf", "shin", "lowerleg", "foot"
        };
        for (const auto* Word : Included)
            if (Key.find(Word) != std::string::npos) return true;
        return false;
    }

    // Connect each body joint to its nearest body ancestor, skipping helper/twist bones.
    inline std::vector<std::pair<int, int>> BoneLinks(const std::vector<std::string>& Names,
                                                    const std::vector<int>& Parents) {
        std::vector<std::pair<int, int>> Links;
        if (Names.size() != Parents.size()) return Links;
        for (int Child = 0; Child < static_cast<int>(Names.size()); ++Child) {
            if (!IsBodyBone(Names[Child])) continue;
            int Parent = Parents[Child];
            std::vector<bool> Visited(Names.size(), false);
            Visited[Child] = true;
            while (Parent >= 0 && Parent < static_cast<int>(Names.size()) && !Visited[Parent]) {
                Visited[Parent] = true;
                if (IsBodyBone(Names[Parent])) {
                    Links.emplace_back(Child, Parent);
                    break;
                }
                Parent = Parents[Parent];
            }
        }
        return Links;
    }
}
