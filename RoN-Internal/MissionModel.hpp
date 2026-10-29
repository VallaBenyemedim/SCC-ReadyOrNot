#pragma once
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <string>
#include <vector>

namespace MissionModel {
    enum class Kind { None, Rescue, Arrest, Neutralize, Defuse, Collect, Report, Interact };
    enum class Category { Other, Person, Bomb, Evidence, Reportable, Door };
    struct Rule {
        Kind Action = Kind::None;
        int Status = 0;
        int LockedMode = 0;
        bool Hidden = false;
        uint64_t Tag = 0;
        std::vector<uint64_t> Actors;
        std::wstring Title;
        std::wstring Description;
        std::wstring TagText;
    };
    struct Candidate {
        uint64_t Actor = 0;
        Category Type = Category::Other;
        std::vector<uint64_t> Tags;
        bool Actionable = true;
    };

    inline bool Active(const Rule& Goal, int Mode) {
        return Goal.Action != Kind::None && Goal.Status == 0 && !Goal.Hidden
            && (Goal.LockedMode == 0 || Goal.LockedMode == Mode);
    }

    inline bool Matches(const Rule& Goal, int Mode, const Candidate& Target) {
        if (!Active(Goal, Mode) || !Target.Actionable || !Target.Actor) return false;
        const auto CategoryMatches = [&] {
            switch (Goal.Action) {
            case Kind::Rescue: case Kind::Arrest: case Kind::Neutralize: return Target.Type == Category::Person;
            case Kind::Defuse: return Target.Type == Category::Bomb;
            case Kind::Collect: return Target.Type == Category::Evidence;
            case Kind::Report: return Target.Type == Category::Reportable;
            case Kind::Interact: return Target.Type == Category::Door;
            default: return false;
            }
        };
        if (!CategoryMatches()) return false;
        if (!Goal.Actors.empty())
            return std::find(Goal.Actors.begin(), Goal.Actors.end(), Target.Actor) != Goal.Actors.end();
        if (Goal.Action == Kind::Defuse) return true;
        return Goal.Tag && std::find(Target.Tags.begin(), Target.Tags.end(), Goal.Tag) != Target.Tags.end();
    }

    inline std::wstring Normalize(const std::wstring& Text) {
        std::wstring Out;
        for (wchar_t Ch : Text) if (std::iswalnum(Ch)) Out += static_cast<wchar_t>(std::towlower(Ch));
        return Out;
    }

    inline std::wstring PersonName(const Rule& Goal) {
        std::wstring Tag = Goal.TagText;
        if (Tag.size() > 2 && Tag.starts_with(L"Is") && (std::iswupper(Tag[2]) || Tag[2] == L'_')) Tag.erase(0, 2);
        std::wstring Name;
        for (size_t i = 0; i < Tag.size(); ++i) {
            const wchar_t Ch = Tag[i];
            if (Ch == L'_') { if (!Name.empty() && Name.back() != L' ') Name += L' '; continue; }
            if (i && std::iswupper(Ch) && (std::iswlower(Tag[i-1])
                || (std::iswupper(Tag[i-1]) && i+1 < Tag.size() && std::iswlower(Tag[i+1]))))
                if (!Name.empty() && Name.back() != L' ') Name += L' ';
            Name += Ch;
        }
        // A tag-derived name is used only when the mission's own text confirms it.
        const auto Key = Normalize(Name);
        if (Key.size() >= 4 && (Normalize(Goal.Title).find(Key) != std::wstring::npos
            || Normalize(Goal.Description).find(Key) != std::wstring::npos)) return Name;
        return Goal.Title;
    }

    inline const wchar_t* ActionLabel(Kind Action) {
        switch (Action) {
        case Kind::Rescue: return L"KURTAR";
        case Kind::Arrest: return L"TUTUKLA";
        case Kind::Neutralize: return L"ETK\u0130S\u0130Z HALE GET\u0130R";
        case Kind::Defuse: return L"BOMBAYI \u0130MHA ET";
        case Kind::Collect: return L"G\u00d6REV KANITI";
        case Kind::Report: return L"BUL / B\u0130LD\u0130R";
        case Kind::Interact: return L"G\u00d6REV NOKTASI";
        default: return L"HEDEF";
        }
    }

    inline std::wstring Caption(const Rule& Goal, const std::wstring& ObjectName = L"") {
        std::wstring Name;
        if (Goal.Action == Kind::Rescue || Goal.Action == Kind::Arrest || Goal.Action == Kind::Neutralize)
            Name = PersonName(Goal);
        else Name = !ObjectName.empty() ? ObjectName : Goal.Title;
        for (auto& Ch : Name) if (Ch == L'\n' || Ch == L'\r' || Ch == L'\t') Ch = L' ';
        if (Name.size() > 80) Name = Name.substr(0, 77) + L"...";
        return std::wstring(ActionLabel(Goal.Action)) + (Name.empty() ? L"" : L": " + Name);
    }

    inline bool ComputerName(const std::wstring& Text) {
        const auto Key = Normalize(Text);
        for (const auto* Word : {L"computer", L"laptop", L"notebook", L"terminal", L"workstation", L"bilgisayar"})
            if (Key.find(Word) != std::wstring::npos) return true;
        return false;
    }
}
