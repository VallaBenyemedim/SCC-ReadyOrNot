#pragma once
#include "OverlayScene.hpp"
#include "Config.hpp"

// Deterministic visual fixture. Positions represent a pose, not a live game screenshot.
inline OverlayScene::Frame ESPSampleScene() {
    using namespace OverlayScene;
    const auto C=Config::Snapshot().ESP;
    Frame F;F.Width=1280;F.Height=800;F.Declutter=C.bDeclutterLabels;
    auto Person=[&](float X,Color Tint,const char* LabelText,bool Target) {
        F.Strokes.push_back({{X-50,225},{X+50,525},Target?Color{1,.82f,.15f,.75f}:Tint,C.BoxThickness,true});
        const Point Pose[]={{X,240},{X,265},{X,300},{X,342},{X,385},
            {X-29,278},{X-42,335},{X-49,374},{X+29,278},{X+42,335},{X+49,374},
            {X-19,439},{X-22,505},{X-35,515},{X+19,439},{X+22,505},{X+35,515}};
        const int Links[][2]={{0,1},{1,2},{2,3},{3,4},{1,5},{5,6},{6,7},{1,8},{8,9},{9,10},{4,11},{11,12},{12,13},{4,14},{14,15},{15,16}};
        auto Bones=Tint;Bones.A=C.SkeletonOpacity;
        for(const auto& L:Links) F.Strokes.push_back({Pose[L[0]],Pose[L[1]],Bones,C.SkeletonThickness,false});
        F.Labels.push_back({LabelText,{X,525},Tint,C.TextScale,C.TextOpacity,C.LabelBackground,false,40});
        if(Target) F.Labels.push_back({"TUTUKLA: Jack Adams",{X,225},{1,.82f,.15f,1},C.TextScale,C.TextOpacity,C.LabelBackground,true,100});
    };
    Person(385,{1,.25f,.22f,.85f},"Düşman | 24 m",true);
    Person(725,{.3f,.85f,.6f,.85f},"Sivil | 18 m",false);
    F.Strokes.push_back({{990,500},{1027,521},{.95f,.83f,.3f,.65f},1,true});
    F.Labels.push_back({"Tabanca | 7 m",{1008,525},{1,.85f,.3f,1},C.TextScale,C.TextOpacity,C.LabelBackground,false,20});
    F.Labels.push_back({"overlap-hidden",{385,525},{1,1,1,1},C.TextScale,C.TextOpacity,C.LabelBackground,false,0});
    return F;
}
