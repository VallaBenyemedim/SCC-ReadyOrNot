#pragma once

namespace WeaponESP {
    struct State {
        bool IsWeapon=false, InInventory=false, HasMesh=false, Collected=false;
        bool HasCurrentOwner=false, PreviousSuspect=false, PreviousAlly=false, MarkUnownedEvidence=false;
    };
    inline bool ShouldShow(const State& Item) {
        return Item.IsWeapon && !Item.InInventory && Item.HasMesh && !Item.Collected
            && !Item.HasCurrentOwner && !Item.PreviousAlly
            && (Item.PreviousSuspect || Item.MarkUnownedEvidence);
    }
}
