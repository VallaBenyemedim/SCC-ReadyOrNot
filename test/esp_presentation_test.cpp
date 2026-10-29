#include "../RoN-Internal/ESPPresentation.hpp"
#include "../RoN-Internal/MissionModel.hpp"
#include "fixtures/readyornot_bones.hpp"
#include <cassert>
#include <iostream>
#include <set>

int main() {
    using namespace ESPPresentation;
    std::set<std::wstring> Categories;
    for (auto Type : {EEntityType::Suspect, EEntityType::Civilian, EEntityType::Ally,
                      EEntityType::DroppedWeapon, EEntityType::Evidence, EEntityType::Trap, EEntityType::Reportable}) {
        auto Text = Label(Type, EEntityStatus::Active, 24);
        assert(Text.find(L"24 m") != std::wstring::npos);
        Categories.insert(TypeLabel(Type));
    }
    assert(Categories.size() == 7);
    assert(Label(EEntityType::Civilian, EEntityStatus::Arrested).starts_with(L"Sivil | Kelep\u00e7eli"));
    assert(Label(EEntityType::Suspect, EEntityStatus::Dead).starts_with(L"D\u00fc\u015fman | Etkisiz"));
    assert(Label(EEntityType::DroppedWeapon, EEntityStatus::Active) == L"Silah");
    assert(Label(EEntityType::DroppedWeapon,EEntityStatus::Active,7,true)==L"Tabanca | 7 m");
    assert(IsPerson(EEntityType::Civilian) && !IsPerson(EEntityType::DroppedWeapon));

    // A twist/helper bone between real joints must not break the visible arm.
    std::vector<std::string> Names = {"root", "pelvis", "spine_01", "neck_01", "head", "upperarm_l",
                                      "upperarm_twist_01_l", "lowerarm_l", "hand_l", "index_01_l", "ik_hand_l"};
    auto Links = BoneLinks(Names, {-1, 0, 1, 2, 3, 2, 5, 6, 7, 8, 0});
    const std::vector<std::pair<int,int>> Expected = {{2,1}, {3,2}, {4,3}, {5,2}, {7,5}, {8,7}};
    assert(Links == Expected);
    assert(IsBodyBone("Bip01 L UpperArm"));
    assert(IsBodyBone("Bip01 R Forearm"));
    assert(!IsBodyBone("head_end") && !IsBodyBone("VB hand_l") && !IsBodyBone("weapon_hand_socket"));
    // Missing/cyclic parents must terminate without adding a fabricated connection.
    assert(BoneLinks({"head", "helper_a", "helper_b"}, {1, 2, 1}).empty());
    assert(BoneLinks({"head"}, {5}).empty());
    assert(BoneLinks({"head"}, {}).empty());
    const auto LiveLinks=BoneLinks(BoneFixture::Names,BoneFixture::Parents);
    assert(LiveLinks.size()>=15 && LiveLinks.size()<=22 && "Skeleton must contain only the main anatomical chains");
    for (auto [Child,Parent]:LiveLinks) {
        for (auto Index:{Child,Parent}) {
            const auto& Name=BoneFixture::Names[Index];
            assert(Name.find("hlp")==std::string::npos && Name.find("extra")==std::string::npos
                && Name.find("equipment")==std::string::npos && "Helper/equipment bones create visible spikes");
        }
    }

    using namespace MissionModel;
    Rule Jack;
    Jack.Action = Kind::Arrest;
    Jack.Tag = 10;
    Jack.Actors = {100};
    Jack.Title = L"Arrest FISA Agent Adams";
    Jack.Description = L"Apprehend and secure the prime suspect, FISA agent, Jack Adams.";
    Jack.TagText = L"IsJackAdams";
    Candidate Person{100, Category::Person, {10}, true};
    assert(Matches(Jack, 4, Person));
    assert(Caption(Jack) == L"TUTUKLA: Jack Adams");
    Person.Actor = 101;
    assert(!Matches(Jack, 4, Person)); // A repeated tag must not override a precise actor reference.
    Rule Maria = Jack;
    Maria.Actors = {101};
    Maria.Title = L"Arrest Maria Lopez";
    Maria.Description = L"Apprehend and secure postal worker, Maria Lopez.";
    Maria.TagText = L"IsMariaLopez";
    assert(Matches(Maria, 4, Person) && !Matches(Jack, 4, Person));
    assert(Caption(Maria) == L"TUTUKLA: Maria Lopez");
    Rule Eugene;
    Eugene.Action = Kind::Rescue;
    Eugene.Tag = 30;
    Eugene.TagText = L"IsEugeneGomez";
    Eugene.Title = L"Find Eugene Gomez";
    assert(Matches(Eugene, 4, {102, Category::Person, {30}, true}));
    assert(Caption(Eugene) == L"KURTAR: Eugene Gomez");
    Eugene.Status = 1;
    assert(!Matches(Eugene, 4, {102, Category::Person, {30}, true}));
    Eugene.Status = 2;
    assert(!Active(Eugene, 4));
    Eugene.Status = 0; Eugene.Hidden = true;
    assert(!Active(Eugene, 4));
    Rule Bomb;
    Bomb.Action = Kind::Defuse; Bomb.LockedMode = 1;
    assert(Matches(Bomb, 1, {200, Category::Bomb, {}, true}));
    assert(!Matches(Bomb, 4, {200, Category::Bomb, {}, true}));
    assert(!Matches(Bomb, 1, {200, Category::Bomb, {}, false}));
    Rule Office;
    Office.Action = Kind::Report; Office.Tag = 40;
    assert(Matches(Office, 4, {300, Category::Reportable, {40}, true}));
    assert(Caption(Office, L"FISA Office") == L"BUL / B\u0130LD\u0130R: FISA Office");
    assert(!Matches(Office, 4, {301, Category::Reportable, {41}, true}));
    Rule PC;
    PC.Action = Kind::Collect; PC.Tag = 50;
    assert(ComputerName(L"Retrieve the laptop"));
    assert(Matches(PC, 4, {400, Category::Evidence, {50}, true}));
    assert(!Matches(PC, 4, {401, Category::Evidence, {}, true}));
    assert(!Matches(PC, 4, {402, Category::Person, {50}, true}));
    assert(TypeLabel(EEntityType::Bomb) == std::wstring(L"Bomba"));
    assert(TypeLabel(EEntityType::Computer) == std::wstring(L"Bilgisayar"));
    std::cout << "PASS: Turkish labels, skeleton chains, named targets, multiple people, office/PC matching, mode changes and completed/disabled targets\n";
}
