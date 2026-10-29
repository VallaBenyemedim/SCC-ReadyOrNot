#include "Renderer.hpp"
#include "Config.hpp"
#include "OverlayScene.hpp"
#include <algorithm>
#include <cmath>

namespace Renderer {
    namespace {
        thread_local OverlayScene::Frame Draft;
        thread_local Config::ESPSettings Style;
        OverlayScene::Point Point(SDK::FVector2D P) {return {static_cast<float>(P.X),static_cast<float>(P.Y)};}
        OverlayScene::Color Tint(SDK::FLinearColor C) {return {C.R,C.G,C.B,C.A};}
    }
    void BeginFrame(SDK::UCanvas* Canvas) {
        Draft={}; Style=Config::Snapshot().ESP;
        if (Canvas) {Draft.Width=Canvas->ClipX;Draft.Height=Canvas->ClipY;}
        Draft.Declutter=Style.bDeclutterLabels;
    }
    void EndFrame() {OverlayScene::Submit(std::move(Draft));}
    bool WorldToScreen(SDK::APlayerController* PC,SDK::FVector WorldPos,SDK::FVector2D& ScreenPos) {
        return PC && PC->ProjectWorldLocationToScreen(WorldPos,&ScreenPos,false)
            && std::isfinite(ScreenPos.X) && std::isfinite(ScreenPos.Y);
    }
    void DrawBox(SDK::UCanvas* Canvas,SDK::FVector2D Pos,SDK::FVector2D Size,float Thickness,SDK::FLinearColor Color) {
        if (!Canvas) return;
        Draft.Strokes.push_back({Point(Pos),Point({Pos.X+Size.X,Pos.Y+Size.Y}),Tint(Color),Thickness,true});
    }
    void DrawLine(SDK::UCanvas* Canvas,SDK::FVector2D Start,SDK::FVector2D End,float Thickness,SDK::FLinearColor Color) {
        if (!Canvas) return;
        Draft.Strokes.push_back({Point(Start),Point(End),Tint(Color),Thickness,false});
    }
    bool DrawString(SDK::UCanvas* Canvas,const std::wstring& Text,SDK::FVector2D Pos,SDK::FLinearColor Color,float Scale,bool Above,int Priority) {
        if (!Canvas || Text.empty() || Canvas->ClipX<1 || Canvas->ClipY<1) return false;
        const int Length=WideCharToMultiByte(CP_UTF8,0,Text.data(),static_cast<int>(Text.size()),nullptr,0,nullptr,nullptr);
        if (Length<=0) return false;
        std::string Utf8(Length,'\0');
        WideCharToMultiByte(CP_UTF8,0,Text.data(),static_cast<int>(Text.size()),Utf8.data(),Length,nullptr,nullptr);
        Draft.Labels.push_back({std::move(Utf8),Point(Pos),Tint(Color),Scale,Style.TextOpacity,Style.LabelBackground,Above,Priority});
        return true;
    }
}
