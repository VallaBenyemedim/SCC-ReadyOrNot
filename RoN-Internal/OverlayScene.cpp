#include "OverlayScene.hpp"
#include "imgui.h"
#include "Logger.hpp"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <cmath>

namespace OverlayScene {
    namespace {
        Frame Latest;
        std::chrono::steady_clock::time_point Updated;
        std::mutex Mutex;
        Stats Last;
        ImU32 Tint(Color C,float Alpha=1) {
            return ImGui::ColorConvertFloat4ToU32({C.R,C.G,C.B,std::clamp(C.A*Alpha,0.0f,1.0f)});
        }
    }
    void Submit(Frame Value) {
        std::lock_guard<std::mutex> Lock(Mutex);
        Latest=std::move(Value); Updated=std::chrono::steady_clock::now();
    }
    void Clear() {std::lock_guard<std::mutex> Lock(Mutex);Latest={};Last={};Updated=std::chrono::steady_clock::now();}
    Stats LastStats() { std::lock_guard<std::mutex> Lock(Mutex); return Last; }
    Stats Draw(bool Dimmed) {
        Frame Current;
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            Last={};
            if (std::chrono::steady_clock::now()-Updated>std::chrono::milliseconds(300)) return {};
            Current=Latest;
        }
        if (Current.Width<1 || Current.Height<1) return {};
        const auto Display=ImGui::GetIO().DisplaySize;
        const float SX=Display.x/Current.Width,SY=Display.y/Current.Height;
        const float Alpha=Dimmed?0.28f:1.0f;
        auto P=[&](Point V) { return ImVec2{V.X*SX,V.Y*SY}; };
        auto* Draw=ImGui::GetBackgroundDrawList();
        Stats Result;
        for (const auto& Line:Current.Strokes) {
            const auto A=P(Line.A),B=P(Line.B);
            if (!std::isfinite(A.x+A.y+B.x+B.y)) continue;
            if (Line.Box) Draw->AddRect(A,B,Tint(Line.Tint,Alpha),2.0f,0,Line.Thickness);
            else Draw->AddLine(A,B,Tint(Line.Tint,Alpha),Line.Thickness);
            ++Result.Strokes;
        }
        std::stable_sort(Current.Labels.begin(),Current.Labels.end(),[](const Label& A,const Label& B){return A.Priority>B.Priority;});
        struct Rect { ImVec2 A,B; };
        std::vector<Rect> Placed;
        auto* Font=ImGui::GetIO().FontDefault;
        for (const auto& Tag:Current.Labels) {
            if (Tag.Text.empty()) continue;
            const float FontSize=13.0f*Tag.Scale;
            const auto Size=Font->CalcTextSizeA(FontSize,10000,0,Tag.Text.c_str());
            auto Anchor=P(Tag.Anchor);
            if (!std::isfinite(Anchor.x+Anchor.y) || Size.x>Display.x-12) continue;
            ImVec2 Pos{std::clamp(Anchor.x-Size.x*0.5f,9.0f,std::max(9.0f,Display.x-Size.x-9)),
                std::clamp(Anchor.y+(Tag.Above?-Size.y-6:5),4.0f,std::max(4.0f,Display.y-Size.y-5))};
            Rect Bounds{{Pos.x-7,Pos.y-3},{Pos.x+Size.x+6,Pos.y+Size.y+3}};
            bool Overlap=false;
            for (const auto& R:Placed) if (Bounds.A.x<R.B.x+3 && Bounds.B.x>R.A.x-3 && Bounds.A.y<R.B.y+2 && Bounds.B.y>R.A.y-2) {Overlap=true;break;}
            if (Current.Declutter && Overlap && Tag.Priority<100) {++Result.Suppressed;continue;}
            Placed.push_back(Bounds);
            const float Opacity=std::clamp(Tag.Opacity*Tag.Accent.A*Alpha,0.0f,1.0f);
            if (Tag.Background>0) Draw->AddRectFilled(Bounds.A,Bounds.B,Tint({0.025f,0.035f,0.045f,Tag.Background},Alpha),3);
            Draw->AddLine({Bounds.A.x+1,Bounds.A.y+3},{Bounds.A.x+1,Bounds.B.y-3},Tint(Tag.Accent,Opacity),1.5f);
            Draw->AddText(Font,FontSize,{Pos.x+0.75f,Pos.y+0.75f},Tint({0,0,0,Opacity*0.7f}),Tag.Text.c_str());
            // Neutral text preserves type color in a small accent, avoiding neon nameplates.
            Draw->AddText(Font,FontSize,Pos,Tint({0.86f,0.89f,0.91f,Opacity}),Tag.Text.c_str());
            ++Result.Labels;
        }
        if (Result.Strokes || Result.Labels) {
            static bool Logged=false;
            if (!Logged) {Logger::Log("[ESP] Antialiased ImGui scene active: %zu strokes, %zu quiet labels",Result.Strokes,Result.Labels);Logged=true;}
        }
        {std::lock_guard<std::mutex> Lock(Mutex);Last=Result;}
        return Result;
    }
}
