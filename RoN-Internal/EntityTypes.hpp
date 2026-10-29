#pragma once

enum class EEntityType {
    Unknown, Suspect, Civilian, Ally, Trap, Evidence, DroppedWeapon, Reportable,
    Bomb, Computer, MissionObject
};

enum class EEntityStatus {
    Active, Arrested, Dead, TrapLive, TrapDisabled
};
