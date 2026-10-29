#include "../RoN-Internal/WeaponESP.hpp"
#include <cassert>
#include <iostream>

int main() {
    // Captured live in RoN_Dealer_BarricadedSuspects_Core: Secondary_Makarov_C,
    // AllItems=true, bInInventory=0, PreviousOwner=null, Owner=null,
    // EvidenceComponent.bMarkAsEvidenceWhenNoOwner=1, bEvidenceExtracted=0, ItemMesh exists.
    WeaponESP::State Makarov{true,false,true,false,false,false,false,true};
    assert(WeaponESP::ShouldShow(Makarov) && "Unowned Makarov evidence must be visible");
    WeaponESP::State Rifle{true,false,true,false,false,true,false,true};
    assert(WeaponESP::ShouldShow(Rifle));
    auto Inventory=Makarov; Inventory.InInventory=true; assert(!WeaponESP::ShouldShow(Inventory));
    auto Collected=Makarov; Collected.Collected=true; assert(!WeaponESP::ShouldShow(Collected));
    auto Ally=Makarov; Ally.PreviousAlly=true; assert(!WeaponESP::ShouldShow(Ally));
    auto Held=Makarov; Held.HasCurrentOwner=true; assert(!WeaponESP::ShouldShow(Held));
    auto NotWeapon=Makarov; NotWeapon.IsWeapon=false; assert(!WeaponESP::ShouldShow(NotWeapon));
    auto NoMesh=Makarov; NoMesh.HasMesh=false; assert(!WeaponESP::ShouldShow(NoMesh));
    auto Decoration=Makarov; Decoration.MarkUnownedEvidence=false; assert(!WeaponESP::ShouldShow(Decoration));
    std::cout << "PASS: unowned handgun and suspect rifle included; held, collected, ally and non-weapon items excluded\n";
}
