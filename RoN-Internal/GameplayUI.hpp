#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace GameplayUI {
    enum class Action {TeleportToNPC,BringNPC};
    struct Request {Action Type;uint64_t NPC=0;};
    struct NPC {uint64_t Id=0;std::string Name;float Distance=0;int Kind=0;};
    struct State {
        bool Ready=false,Authority=false;
        float Health=0,MaxHealth=0;
        int Allies=0,Suspects=0,Doors=0,ActivePatches=0;
        std::string Weapon;
        std::vector<NPC> People;
    };
    void Publish(State Value);
    State Snapshot();
    bool Queue(Request Value);
    std::vector<Request> Take();
    void Clear();
}
