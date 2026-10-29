#pragma once
#include <Windows.h>
#include <cstdint>

namespace GraphicsOverlay {
    using InputCallback = void(*)(bool Open);
    bool Initialize(HWND Window, InputCallback Callback = nullptr);
    bool Shutdown();
    bool Ready();
    uint64_t RenderedFrames();
}
