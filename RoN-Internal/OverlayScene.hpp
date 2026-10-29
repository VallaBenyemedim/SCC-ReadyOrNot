#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace OverlayScene {
    struct Point { float X=0,Y=0; };
    struct Color { float R=1,G=1,B=1,A=1; };
    struct Stroke { Point A,B; Color Tint; float Thickness=1; bool Box=false; };
    struct Label {
        std::string Text;
        Point Anchor;
        Color Accent;
        float Scale=1,Opacity=0.88f,Background=0.28f;
        bool Above=false;
        int Priority=0;
    };
    struct Frame {
        float Width=0,Height=0;
        bool Declutter=true;
        std::vector<Stroke> Strokes;
        std::vector<Label> Labels;
    };
    struct Stats { size_t Strokes=0,Labels=0,Suppressed=0; };
    void Submit(Frame Value);
    void Clear();
    Stats Draw(bool Dimmed=false);
    Stats LastStats();
}
