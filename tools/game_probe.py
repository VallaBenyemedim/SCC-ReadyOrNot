"""Read-only compatibility probe for the running Ready or Not Steam game."""
import argparse
import ctypes as c
from functools import lru_cache
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tmp/python-deps"))
import pefile

K32 = c.WinDLL("kernel32", use_last_error=True)
K32.OpenProcess.argtypes = [c.c_uint32, c.c_bool, c.c_uint32]
K32.OpenProcess.restype = c.c_void_p
K32.ReadProcessMemory.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
K32.CloseHandle.argtypes = [c.c_void_p]


class Game:
    def __init__(self):
        cmd = "$p=Get-Process -Name 'ReadyOrNotSteam-Win64-Shipping'; [pscustomobject]@{id=$p.Id;base=$p.MainModule.BaseAddress.ToInt64();path=$p.Path}|ConvertTo-Json -Compress"
        info = json.loads(subprocess.check_output(["powershell", "-NoProfile", "-Command", cmd], text=True))
        self.pid, self.base, self.path = info["id"], info["base"], info["path"]
        self.handle = K32.OpenProcess(0x410, False, self.pid)
        if not self.handle:
            raise c.WinError(c.get_last_error())
        self.pe = pefile.PE(self.path, fast_load=True)
        self.off = offsets()

    def read(self, address, size):
        out = c.create_string_buffer(size)
        count = c.c_size_t()
        if not K32.ReadProcessMemory(self.handle, address, out, size, c.byref(count)) or count.value != size:
            raise ValueError(f"Unreadable game address: 0x{address:X}")
        return out.raw

    def ptr(self, address):
        return struct.unpack("<Q", self.read(address, 8))[0]

    def section(self, name):
        sec = next(s for s in self.pe.sections if s.Name.rstrip(b"\0") == name.encode())
        return sec.VirtualAddress, self.read(self.base + sec.VirtualAddress, sec.Misc_VirtualSize)

    @lru_cache(maxsize=None)
    def name(self, index):
        pool = self.base + self.off["GNames"]
        block = self.ptr(pool + 16 + 8 * (index >> 16))
        address = block + 2 * (index & 0xFFFF)
        header = struct.unpack("<H", self.read(address, 2))[0]
        length, wide = header >> 6, header & 1
        assert 0 < length < 1024, "Invalid FName entry"
        return self.read(address + 2, length * (2 if wide else 1)).decode("utf-16-le" if wide else "utf-8")

    def object_name(self, address):
        return self.name(struct.unpack("<I", self.read(address + 24, 4))[0])

    def properties(self, cls):
        result = {}
        field = self.ptr(cls + 0x50)
        seen = set()
        while field and field not in seen:
            seen.add(field)
            data = self.read(field, 0x58)
            name_index = struct.unpack_from("<I", data, 0x20)[0]
            result[self.name(name_index)] = struct.unpack_from("<i", data, 0x44)[0]
            field = struct.unpack_from("<Q", data, 0x18)[0]
        return result

    def close(self):
        K32.CloseHandle(self.handle)


def offsets():
    source = (ROOT / "RoN-Internal/SDK/Basic.hpp").read_text()
    return {name: int(value, 16) for name, value in re.findall(r"constexpr int32\s+(\w+)\s*=\s*(0x[\dA-Fa-f]+)", source)}


def check(game):
    off = offsets()
    objects = game.read(game.base + off["GObjects"], 32)
    chunks, _, maximum, count, max_chunks, num_chunks = struct.unpack("<QQiiii", objects)
    assert chunks > 0x10000 and 1000 < count <= maximum <= 16_777_216, f"Invalid GObjects: pointer=0x{chunks:X}, count={count}, maximum={maximum}"
    assert 0 < num_chunks <= max_chunks <= 1024, "Invalid object chunks"
    first = game.ptr(game.ptr(chunks))
    vtable = game.ptr(first)
    event = game.ptr(vtable + off["ProcessEventIdx"] * 8) - game.base
    assert event == off["ProcessEvent"], f"ProcessEvent mismatch: actual=0x{event:X}, SDK=0x{off['ProcessEvent']:X}"
    world = game.ptr(game.base + off["GWorld"])
    assert world > 0x10000, f"Invalid GWorld: 0x{world:X}"
    game.read(world, 0x200)
    assert game.name(0) == "None", "GNames mismatch"
    world_class = game.ptr(world + 16)
    assert game.object_name(world_class) == "World", "World class mismatch"
    reflected = game.properties(world_class)
    assert reflected["PersistentLevel"] == 0x30, reflected
    assert reflected["OwningGameInstance"] == 0x1B8, reflected
    level = game.ptr(world + 0x30)
    assert game.object_name(game.ptr(level + 16)) == "Level", "PersistentLevel layout mismatch"
    gi = game.ptr(world + reflected["OwningGameInstance"])
    assert "GameInstance" in game.object_name(game.ptr(gi + 16)), "GameInstance layout mismatch"
    pdata_rva, pdata = game.section(".pdata")
    starts = {struct.unpack_from("<I", pdata, i)[0] for i in range(0, len(pdata)-11, 12)}
    assert off["AppendString"] in starts, "AppendString is not a function entry"
    print(json.dumps({"compatible": True, "pid": game.pid, "objects": count, "world": game.object_name(world), "processEvent": hex(event), "worldProperties": len(reflected)}))


def discover(game):
    rva, data = game.section(".data")
    print(f".data RVA=0x{rva:X}, size=0x{len(data):X}")
    for i in range(0, len(data) - 32, 8):
        chunks, _, maximum, count, max_chunks, num_chunks = struct.unpack_from("<QQiiii", data, i)
        if not (0x10000 < chunks < 0x7FFFFFFF0000 and 1000 < count <= maximum <= 16_777_216 and maximum % 65536 == 0 and 0 < num_chunks <= max_chunks <= 1024):
            continue
        try:
            first = game.ptr(game.ptr(chunks))
            obj = game.read(first, 40)
            vt, flags, index, cls, name, number, outer = struct.unpack("<QIiQIIQ", obj)
            event = game.ptr(vt + 0x4D * 8)
            if game.base < vt < game.base + game.pe.OPTIONAL_HEADER.SizeOfImage and index == 0:
                print(json.dumps({"GObjects": hex(rva+i), "count": count, "first": hex(first), "firstName": name, "ProcessEvent": hex(event-game.base)}))
        except ValueError:
            pass
    for i in range(0, len(data) - 24, 8):
        block, cursor, first = struct.unpack_from("<IIQ", data, i+8)
        if not (0 <= block < 8192 and 0 < cursor < 131072 and 0x10000 < first < 0x7FFFFFFF0000):
            continue
        try:
            entry = game.read(first, 32)
            if entry[2:6] == b"None":
                print(json.dumps({"GNames": hex(rva+i), "block": block, "cursor": cursor, "entry": entry.hex()}))
        except ValueError:
            pass


def verify_layout(game):
    wanted = {
        "UWorld": ["PersistentLevel", "OwningGameInstance", "GameState", "AuthorityGameMode", "Levels"],
        "UGameInstance": ["LocalPlayers"], "UPlayer": ["PlayerController"],
        "APlayerController": ["MyHUD", "bShowMouseCursor"], "AController": ["Pawn"], "AHUD": ["Canvas"],
        "UWidgetBlueprintLibrary": [],
        "ACharacter": ["Mesh"], "UEngine": ["SmallFont"], "UCanvas": ["ClipX", "ClipY"],
        "USkinnedMeshComponent": [], "USceneComponent": [],
        "ABaseItem": ["bInInventory", "PreviousOwner", "ItemMesh", "EvidenceComponent", "ItemClass"],
        "UEvidenceComponent": ["bEvidenceExtracted", "bMarkAsEvidenceWhenNoOwner", "EvidenceState"],
        "ATrapActor": ["TrapStatus"], "AReportableActor": ["bHasBeenReported", "bReportableEnabled", "ReportableName"],
        "AReadyOrNotPlayerController": ["bClientModded"], "AScoringManager": ["bIsOfficialScoring"],
        "ACommanderGM": ["CommanderProfile"], "ALobbyGM": ["CommanderProfile"],
        "UCommanderProfile": ["bIsModded"], "UReadyOrNotGameInstance": ["ModdedLevelDataAssets"],
        "AReadyOrNotGameState": ["AllItems", "AllReportableActors", "AllEvidenceActors", "MissionObjectives"],
        "AActor": ["Tags", "Owner"], "ACoopGS": ["Mode"],
        "AObjective": ["ObjectiveName", "ObjectiveDescription", "ObjectiveStatus", "LockedToMode", "bHiddenObjective"],
        "ANeutralizeSuspectByTag": ["suspect", "SuspectTag", "bRequireArrest"],
        "ARescueCivilianByTag": ["civilian", "CivilianTag"],
        "APickupEvidenceByTag": ["EvidenceTag"], "AReportReportableByTag": ["ReportTag"],
        "ADefuseBombThreats": [], "ABombActor": ["BombState", "TimeUntilExplodes"],
        "AEvidenceActor": ["EvidenceName"], "AInteractWithDoorObjective": ["Doors", "DoorTag"], "ADoor": [],
        "AReadyOrNotCharacter": ["CharacterHealth", "InventoryComp", "bGodMode", "bSurrendered", "SpeechCharacterName"],
        "APlayerCharacter": ["BleedComponent", "RunSpeed", "SpeedModifier_Crouch", "SlowDownSpeedMultiplier", "WalkSpeedMultiplier", "MaxRunSpeedPercent", "MaxCrouchRunSpeedPercent", "PendingRecoil", "MeshspaceRecoilMovementMultiplier", "bWalking"],
        "UResourceComponent": ["Resource", "MaxResource", "bUnlimited"],
        "UCharacterHealthComponent": ["RightLeg", "LeftLeg", "RightArm", "LeftArm", "Head"],
        "ABaseWeapon": ["ADSRecoilMultiplier", "FirstShotRecoil", "VelocityRecoilMultiplier", "RecoilMultiplierPitch", "RecoilMultiplierYaw", "RecoilFireStrength", "RecoilFireStrengthFirst", "RecoilAngleStrength", "RecoilBuildupADSModifier"],
        "ABaseMagazineWeapon": ["bInfiniteAmmo", "Magazines", "MagIndex", "AmmoMax"],
        "UInventoryComponent": [],
        "ACyberneticCharacter": ["bIsFakeSurrender"], "ACyberneticController": ["MoraleComponent"],
        "UMoraleComponent": [], "UBleedComponent": [],
    }
    wanted["AActor"] += ["bCanBeDamaged"]
    wanted["APawn"] = ["Controller"]
    wanted["ADoor"] = ["bLocked", "bAlwaysLocked"]
    expected = {}
    for path in (ROOT / "RoN-Internal/SDK").glob("*_classes.hpp"):
        text = path.read_text()
        for match in re.finditer(r"^class(?:\s+(?:alignas|SDK_ALIGN)\([^)]*\))?\s+(\w+)[^{\n]*\n\{(.*?)^\};", text, re.M | re.S):
            cls, body = match.groups()
            if cls in wanted:
                props = dict((n, int(o, 16)) for n, o in re.findall(r"\b(\w+)\s*(?::\s*\d+)?;\s*// (0x[\dA-Fa-f]+)", body))
                expected[cls[1:]] = {p: props[p] for p in wanted[cls]}
    missing = {cls[1:] for cls in wanted} - set(expected)
    assert not missing, f"Missing SDK classes: {missing}"
    # OfficialScore uses this verified offset instead of the older generated ACoopGM base.
    expected["CommanderGM"]["CommanderProfile"] = game.off["CommanderGMProfile"]
    expected["Magazine"]={"Ammo":0,"AmmoType":2}
    expected["LimbHealthData"]={"LimbDamageMultiplier":0x18}
    pool = game.base + game.off["GNames"]
    last_block, cursor = struct.unpack("<II", game.read(pool + 8, 8))
    ids = {}
    for block in range(last_block + 1):
        data = game.read(game.ptr(pool + 16 + 8*block), cursor if block == last_block else 131072)
        for cls in expected:
            pattern = cls.encode()
            start = 2
            while (pos := data.find(pattern, start)) >= 0:
                start = pos + 1
                header = struct.unpack_from("<H", data, pos - 2)[0]
                if pos % 2 == 0 and header >> 6 == len(cls) and header & 1 == 0:
                    ids[(block << 16) | ((pos-2)//2)] = cls
    objects = game.read(game.base + game.off["GObjects"], 32)
    chunks = struct.unpack_from("<Q", objects)[0]
    count = struct.unpack_from("<i", objects, 20)[0]
    found = {}
    for chunk_index in range((count+65535)//65536):
        amount = min(65536, count-chunk_index*65536)
        chunk = game.read(game.ptr(chunks+8*chunk_index), amount*24)
        for i in range(amount):
            obj = struct.unpack_from("<Q", chunk, i*24)[0]
            if not obj:
                continue
            try:
                head = game.read(obj, 32)
                idx = struct.unpack_from("<I", head, 24)[0]
                if idx in ids and game.object_name(struct.unpack_from("<Q", head, 16)[0]) in ["Class", "ScriptStruct"]:
                    found[ids[idx]] = obj
            except ValueError:
                continue
        if len(found) == len(expected):
            break
    assert set(found) == set(expected), f"Missing live classes: {set(expected)-set(found)}"
    checked = 0
    mismatches = []
    for cls, props in expected.items():
        actual = game.properties(found[cls])
        for prop, offset in props.items():
            if actual.get(prop) != offset:
                mismatches.append(f"{cls}.{prop}: SDK={hex(offset)}, live={hex(actual[prop]) if prop in actual else None}")
            checked += 1
    assert not mismatches, "; ".join(mismatches)
    print(json.dumps({"layoutCompatible": True, "classes": len(found), "properties": checked}))
    required_functions = {
        "SkinnedMeshComponent": ["GetBoneName", "GetParentBone", "GetNumBones", "GetSkinnedAsset"],
        "SceneComponent": ["GetSocketLocation"],
        "Canvas": ["K2_TextSize", "K2_DrawText"],
        "Controller": ["SetIgnoreLookInput", "SetIgnoreMoveInput"],
        "Actor": ["K2_TeleportTo", "K2_GetActorLocation", "K2_GetActorRotation", "GetActorForwardVector", "HasAuthority"],
        "ReadyOrNotCharacter": ["IsDeadOrUnconscious", "IsArrested"],
        "CyberneticCharacter": ["Surrender", "GetIsSurrenderAnimPlaying"],
        "InventoryComponent": ["ThrowAllWeapons", "GetEquippedItem"],
        "Door": ["SetLocked"],
        "ResourceComponent": ["SetCurrentResourceToMax"],
        "CharacterHealthComponent": ["ResetAllLimbHealth", "ResetAllLimbTickets"],
        "BleedComponent": ["StopBleeding", "IsBleeding"],
    }
    assertions = (ROOT / "RoN-Internal/SDK/Assertions.inl").read_text()
    sizes = {name: int(size, 16) for name, size in re.findall(r"static_assert\(sizeof\((\w+)\) == (0x[\dA-Fa-f]+)", assertions)}
    function_count = 0
    for cls, names in required_functions.items():
        child = game.ptr(found[cls] + 0x48)
        live = {}
        visited = set()
        while child and child not in visited:
            visited.add(child)
            live[game.object_name(child)] = struct.unpack("<i", game.read(child + 0x58, 4))[0]
            child = game.ptr(child + 0x28)
        for name in names:
            expected_size = sizes.get(f"{cls}_{name}", 0) # no-parameter functions have no generated parameter struct
            assert live.get(name) == expected_size, f"{cls}.{name}: SDK params={expected_size}, live={live.get(name)}"
            function_count += 1
    print(json.dumps({"boneAndTextFunctionsCompatible": True, "functions": function_count}))
    expected_input = {
        "SetInputMode_GameAndUIEx": {"PlayerController": 0, "InWidgetToFocus": 8, "InMouseLockMode": 16, "bHideCursorDuringCapture": 17, "bFlushInput": 18},
        "SetInputMode_GameOnly": {"PlayerController": 0, "bFlushInput": 8},
    }
    child = game.ptr(found["WidgetBlueprintLibrary"] + 0x48)
    input_functions = {}
    while child:
        name = game.object_name(child)
        if name in expected_input:
            fields = game.properties(child)
            for param, offset in expected_input[name].items():
                assert fields.get(param) == offset, f"{name}.{param}: live={fields.get(param)}, expected={offset}"
            input_functions[name] = fields
        child = game.ptr(child + 0x28)
    assert len(input_functions) == 2, "Missing game/UI input functions"
    print(json.dumps({"gameInputFunctionsCompatible": True, "functions": 2}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--discover", action="store_true")
    parser.add_argument("--layout", action="store_true")
    args = parser.parse_args()
    game = Game()
    try:
        if args.discover:
            discover(game)
        else:
            check(game)
            if args.layout:
                verify_layout(game)
    finally:
        game.close()
