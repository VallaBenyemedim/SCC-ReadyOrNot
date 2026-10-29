#pragma once
#include "SDK/Engine_classes.hpp"
#include <string>

// Windows GDI Macro cleanup
#ifdef DrawText
#undef DrawText
#endif

namespace Renderer
{
    void BeginFrame(SDK::UCanvas* Canvas);
    void EndFrame();
    bool WorldToScreen(SDK::APlayerController* PC, SDK::FVector WorldPos, SDK::FVector2D& ScreenPos);
    void DrawBox(SDK::UCanvas* Canvas, SDK::FVector2D TopLeft, SDK::FVector2D Size, float Thickness, SDK::FLinearColor Color);
    bool DrawString(SDK::UCanvas* Canvas, const std::wstring& Text, SDK::FVector2D Pos, SDK::FLinearColor Color, float Scale = 1.0f, bool Above = false, int Priority = 0);
    void DrawLine(SDK::UCanvas* Canvas, SDK::FVector2D Start, SDK::FVector2D End, float Thickness, SDK::FLinearColor Color);
}
