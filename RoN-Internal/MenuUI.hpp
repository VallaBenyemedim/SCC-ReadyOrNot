#pragma once
#include <cstdint>
#include <string>

namespace MenuUI {
    enum Command : unsigned { Save = 1, Load = 2, Defaults = 4, Dump = 8 };
    struct Telemetry { int Entities = 0, Targets = 0; bool Connected = false; };
    void InitializeStyle();
    void Draw();
    bool IsOpen();
    void SetOpen(bool Open);
    void SetVisibilityNotifier(void(*Callback)(bool));
    bool UnloadRequested();
    void RequestUnload();
    unsigned TakeCommands();
    void SetTelemetry(const Telemetry& Value);
    void Notify(const std::string& Message);
    void SetPage(int Page);
}
