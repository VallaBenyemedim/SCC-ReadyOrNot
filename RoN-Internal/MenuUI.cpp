#include "MenuUI.hpp"
#include "Config.hpp"
#include "GameplayUI.hpp"
#include "imgui.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>

namespace MenuUI {
    namespace {
        std::atomic<bool> Open{true}, Unload{false};
        std::atomic<unsigned> Commands{0};
        std::atomic<void(*)(bool)> VisibilityNotifier{nullptr};
        std::mutex StateMutex;
        Telemetry Live;
        std::string Notice;
        std::chrono::steady_clock::time_point NoticeTime;
        int Page = 0;
        float Scale = 1.0f;
        ImFont* Regular = nullptr;
        ImFont* Semibold = nullptr;
        ImGuiStyle BaseStyle;
        constexpr ImVec4 Accent{0.79f,0.69f,0.46f,1.0f};
        constexpr ImVec4 Muted{0.51f,0.52f,0.54f,1.0f};
        constexpr ImVec4 Text{0.91f,0.91f,0.93f,1.0f};
        constexpr ImVec4 Card{0.045f,0.045f,0.050f,1.0f};
        constexpr ImVec4 Border{0.12f,0.12f,0.13f,1.0f};
        float S(float Value) { return Value * Scale; }
        ImU32 U(ImVec4 Color) { return ImGui::ColorConvertFloat4ToU32(Color); }
        void TextAt(ImVec2 Pos, const char* Value, float Size, ImVec4 Color = Text, bool Bold = false) {
            ImGui::GetWindowDrawList()->AddText(Bold ? Semibold : Regular, S(Size), Pos, U(Color), Value);
        }
        void Heading(const char* Title, const char* Description) {
            ImGui::PushFont(Semibold, 28); ImGui::TextUnformatted(Title); ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, Muted); ImGui::TextWrapped("%s", Description); ImGui::PopStyleColor();
            ImGui::Dummy({0,S(14)});
        }
        void BeginCard(const char* Id, const char* Title, float Height, float Width = 0) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Card);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{S(16),S(10)});
            ImGui::BeginChild(Id, {Width ? S(Width) : 0,S(Height)}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PushFont(Semibold,17); ImGui::TextUnformatted(Title); ImGui::PopFont();
        }
        void EndCard() { ImGui::EndChild(); ImGui::PopStyleVar(); ImGui::PopStyleColor(); }
        bool Switch(const char* Label, const char* Detail, bool& Value, float Height = 59) {
            ImGui::PushID(Label);
            auto Pos = ImGui::GetCursorScreenPos();
            float Width = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("switch", {Width,S(Height)});
            bool Changed = ImGui::IsItemClicked();
            if (Changed) Value = !Value;
            auto* Draw = ImGui::GetWindowDrawList();
            if (ImGui::IsItemHovered()) Draw->AddRectFilled(Pos,{Pos.x+Width,Pos.y+S(Height-2)},IM_COL32(255,255,255,5),S(7));
            TextAt({Pos.x+S(2),Pos.y+S(5)},Label,16,Text,true);
            TextAt({Pos.x+S(2),Pos.y+S(29)},Detail,12.5f,Muted);
            float& T = *ImGui::GetStateStorage()->GetFloatRef(ImGui::GetID("animation"),Value ? 1.0f : 0.0f);
            T += ((Value ? 1.0f : 0.0f)-T) * std::min(1.0f,ImGui::GetIO().DeltaTime*15);
            ImVec2 A{Pos.x+Width-S(45),Pos.y+S(14)};
            auto Bg = ImVec4(0.15f+(Accent.x-0.15f)*T,0.15f+(Accent.y-0.15f)*T,0.17f+(Accent.z-0.17f)*T,1);
            Draw->AddRectFilled(A,{A.x+S(42),A.y+S(24)},U(Bg),S(12));
            Draw->AddCircleFilled({A.x+S(12+18*T),A.y+S(12)},S(8),Value ? IM_COL32(28,24,16,255) : IM_COL32(210,211,216,255));
            ImGui::PopID();
            return Changed;
        }
        void Preview(const Config::ESPSettings& Settings, float Width, float Height) {
            auto Pos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("esp-preview",{Width,Height});
            auto* Draw = ImGui::GetWindowDrawList();
            Draw->AddRectFilled(Pos,{Pos.x+Width,Pos.y+Height},IM_COL32(13,20,25,255),S(10));
            for (float X = S(22); X < Width; X += S(28)) Draw->AddLine({Pos.x+X,Pos.y},{Pos.x+X,Pos.y+Height},IM_COL32(255,255,255,5));
            for (float Y = S(20); Y < Height; Y += S(28)) Draw->AddLine({Pos.x,Pos.y+Y},{Pos.x+Width,Pos.y+Y},IM_COL32(255,255,255,5));
            TextAt({Pos.x+S(14),Pos.y+S(10)},"CANLI STİL ÖNİZLEMESİ",11.5f,Muted,true);
            float CX=Pos.x+Width*0.5f, CY=Pos.y+Height*0.51f;
            const float BodyScale=std::min(1.0f,(Height-S(40*Settings.TextScale))/S(160));
            auto B=[&](float Value) { return S(Value)*BodyScale; };
            const ImU32 Person = U({Settings.ColCivilian.R,Settings.ColCivilian.G,Settings.ColCivilian.B,1});
            if (Settings.bEnabled) {
                if (Settings.bBox) Draw->AddRect({CX-B(34),CY-B(46)},{CX+B(34),CY+B(61)},Person,S(2),0,S(Settings.BoxThickness));
                if (Settings.bSkeleton) {
                    const auto Bones=U({Settings.ColCivilian.R,Settings.ColCivilian.G,Settings.ColCivilian.B,Settings.ColCivilian.A*Settings.SkeletonOpacity});
                    Draw->AddCircle({CX,CY-B(32)},B(9),Bones,20,S(Settings.SkeletonThickness));
                    for (const auto& Line : {ImVec4(0,-23,0,17),ImVec4(-24,-9,24,-9),ImVec4(-24,-9,-30,18),ImVec4(24,-9,30,18),ImVec4(0,17,-17,51),ImVec4(0,17,17,51)})
                        Draw->AddLine({CX+B(Line.x),CY+B(Line.y)},{CX+B(Line.z),CY+B(Line.w)},Bones,S(Settings.SkeletonThickness));
                }
                if (Settings.bNameTags) {
                    auto Tag=[&](const char* Value,float Y,Config::Color AccentColor) {
                        const auto Size=Regular->CalcTextSizeA(S(13*Settings.TextScale),10000,0,Value);
                        const ImVec2 At{CX-Size.x*0.5f,Y};
                        Draw->AddRectFilled({At.x-S(7),At.y-S(3)},{At.x+Size.x+S(6),At.y+Size.y+S(3)},U({.025f,.035f,.045f,Settings.LabelBackground}),S(3));
                        Draw->AddLine({At.x-S(6),At.y},{At.x-S(6),At.y+Size.y},U({AccentColor.R,AccentColor.G,AccentColor.B,Settings.TextOpacity}),S(1.5f));
                        Draw->AddText(Regular,S(13*Settings.TextScale),At,U({.86f,.89f,.91f,Settings.TextOpacity}),Value);
                    };
                    Tag(Settings.bDistance ? "Sivil | 24 m" : "Sivil",CY+B(69),Settings.ColCivilian);
                    if (Settings.bObjectives) Tag("KURTAR: Görev hedefi",CY-B(72),Settings.ColObjective);
                }
            } else TextAt({CX-S(45),CY},"ESP kapalı",18,Muted,true);
        }
        void Slider(const char* Label,float& Value,float Min,float Max,const char* Format) {
            ImGui::TextUnformatted(Label); ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat((std::string("##")+Label).c_str(),&Value,Min,Max,Format);
            ImGui::Dummy({0,S(6)});
        }
        void ColorRow(const char* Label,Config::Color& Color) {
            ImGui::PushID(Label);
            auto Pos=ImGui::GetCursorScreenPos();
            ImGui::Dummy({0,S(42)});
            TextAt({Pos.x,Pos.y+S(9)},Label,15.5f,Text,true);
            ImGui::SetCursorScreenPos({Pos.x+ImGui::GetContentRegionAvail().x-S(42),Pos.y+S(5)});
            ImGui::ColorEdit4("##color",&Color.R,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_AlphaPreviewHalf);
            ImGui::SetCursorScreenPos({Pos.x,Pos.y+S(42)});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{ImGui::GetStyle().ItemSpacing.x,0});
            ImGui::Dummy({0,0});
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
    }

    bool IsOpen() { return Open.load(); }
    void SetOpen(bool Value) {
        if (Open.exchange(Value) != Value) if (auto Callback=VisibilityNotifier.load()) Callback(Value);
    }
    void SetVisibilityNotifier(void(*Callback)(bool)) { VisibilityNotifier.store(Callback); }
    bool UnloadRequested() { return Unload.load(); }
    void RequestUnload() { SetOpen(false); Unload.store(true); }
    unsigned TakeCommands() { return Commands.exchange(0); }
    void SetTelemetry(const Telemetry& Value) { std::lock_guard<std::mutex> Lock(StateMutex); Live=Value; }
    void Notify(const std::string& Message) { std::lock_guard<std::mutex> Lock(StateMutex); Notice=Message; NoticeTime=std::chrono::steady_clock::now(); }
    void SetPage(int Value) { Page=std::clamp(Value,0,8); }

    void InitializeStyle() {
        auto& IO=ImGui::GetIO(); IO.IniFilename=nullptr; IO.LogFilename=nullptr;
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        char Windows[260]{}; GetWindowsDirectoryA(Windows,260);
        auto FontPath=std::string(Windows)+"\\Fonts\\segoeui.ttf";
        auto BoldPath=std::string(Windows)+"\\Fonts\\seguisb.ttf";
        Regular=IO.Fonts->AddFontFromFileTTF(FontPath.c_str(),18);
        if (!Regular) Regular=IO.Fonts->AddFontDefaultVector();
        Semibold=IO.Fonts->AddFontFromFileTTF(BoldPath.c_str(),18);
        if (!Semibold) Semibold=Regular;
        IO.FontDefault=Regular;
        ImGui::StyleColorsDark();
        auto& Style=ImGui::GetStyle();
        Style.WindowPadding={0,0}; Style.ChildRounding=12; Style.WindowRounding=16;
        Style.FramePadding={12,9}; Style.FrameRounding=8; Style.PopupRounding=10;
        Style.GrabRounding=5; Style.ScrollbarRounding=8; Style.ScrollbarSize=7;
        Style.ItemSpacing={12,8}; Style.WindowBorderSize=1; Style.ChildBorderSize=1;
        Style.Colors[ImGuiCol_WindowBg]={0.018f,0.018f,0.021f,0.99f};
        Style.Colors[ImGuiCol_Text]=Text; Style.Colors[ImGuiCol_TextDisabled]=Muted;
        Style.Colors[ImGuiCol_Border]=Border; Style.Colors[ImGuiCol_Separator]=Border;
        Style.Colors[ImGuiCol_FrameBg]={0.09f,0.09f,0.10f,1};
        Style.Colors[ImGuiCol_FrameBgHovered]={0.14f,0.14f,0.15f,1};
        Style.Colors[ImGuiCol_FrameBgActive]={0.19f,0.18f,0.15f,1};
        Style.Colors[ImGuiCol_SliderGrab]=Accent; Style.Colors[ImGuiCol_SliderGrabActive]=Accent;
        Style.Colors[ImGuiCol_CheckMark]=Accent;
        Style.Colors[ImGuiCol_Button]={0.10f,0.10f,0.115f,1};
        Style.Colors[ImGuiCol_ButtonHovered]={0.17f,0.16f,0.135f,1};
        Style.Colors[ImGuiCol_ButtonActive]={0.23f,0.21f,0.16f,1};
        Style.Colors[ImGuiCol_Header]={0.14f,0.13f,0.105f,1};
        Style.Colors[ImGuiCol_HeaderHovered]={0.20f,0.18f,0.13f,1};
        BaseStyle=Style;
    }

    void Draw() {
        if (!IsOpen()) return;
        const auto& IO=ImGui::GetIO();
        Scale=std::clamp(std::min((IO.DisplaySize.x-40)/1000.0f,(IO.DisplaySize.y-40)/670.0f),0.6f,1.25f);
        ImGui::GetStyle()=BaseStyle; ImGui::GetStyle().ScaleAllSizes(Scale);
        ImGui::GetStyle().FontScaleMain=Scale;
        ImGui::SetNextWindowSize({S(1000),S(670)},ImGuiCond_Always);
        ImGui::SetNextWindowPos({(IO.DisplaySize.x-S(1000))*0.5f,(IO.DisplaySize.y-S(670))*0.5f},ImGuiCond_Appearing);
        if (!ImGui::Begin("RoN Control",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) { ImGui::End(); return; }
        auto Settings=Config::Snapshot();
        auto Initial=Settings;
        Telemetry TelemetryValue; std::string Message;
        { std::lock_guard<std::mutex> Lock(StateMutex); TelemetryValue=Live;
          if (std::chrono::steady_clock::now()-NoticeTime<std::chrono::seconds(5)) Message=Notice; }
        auto P=ImGui::GetWindowPos(); auto* Draw=ImGui::GetWindowDrawList();
        Draw->AddRectFilled(P,{P.x+S(208),P.y+S(670)},IM_COL32(8,8,10,255),S(16),ImDrawFlags_RoundCornersLeft);
        Draw->AddLine({P.x+S(208),P.y+S(22)},{P.x+S(208),P.y+S(648)},U(Border));
        Draw->AddRectFilled({P.x+S(23),P.y+S(29)},{P.x+S(59),P.y+S(65)},U(Accent),S(10));
        TextAt({P.x+S(32),P.y+S(32)},"S",24,{0.07f,0.06f,0.04f,1},true);
        TextAt({P.x+S(72),P.y+S(30)},"SCC",23,Text,true);
        TextAt({P.x+S(73),P.y+S(57)},"READY OR NOT",10.5f,Muted,true);
        TextAt({P.x+S(25),P.y+S(108)},"KONTROL MERKEZİ",10.5f,Muted,true);
        const char* Labels[]={"Genel bakış","Oyuncu ve takım","Silahlar","Hareket","NPC ve dünya","Görseller","Görevler","Renkler","Sistem"};
        const char* Details[]={"Durum ve hızlı ayarlar","Sağlık ve yaralanma","Sekme ve şarjör","Hız ve çarpanlar","Teslim, kapı ve ışınlanma","ESP katmanları","Özel görev hedefleri","Renk ve görünüm","Kayıt ve oturum"};
        for (int i=0;i<9;++i) {
            ImGui::SetCursorPos({S(15),S(132+i*49)}); ImGui::PushID(i);
            ImGui::InvisibleButton("navigation",{S(178),S(45)});
            bool Selected=i==Page;
            if (ImGui::IsItemClicked()) Page=i;
            auto A=ImGui::GetItemRectMin();
            if (Selected || ImGui::IsItemHovered()) Draw->AddRectFilled(A,{A.x+S(178),A.y+S(45)},Selected ? IM_COL32(34,31,23,255) : IM_COL32(22,22,25,255),S(7));
            if (Selected) Draw->AddRectFilled({A.x,A.y+S(10)},{A.x+S(2),A.y+S(35)},U(Accent),S(2));
            char Number[4]; sprintf_s(Number,"%02d",i+1);
            TextAt({A.x+S(13),A.y+S(13)},Number,11,Selected?Accent:Muted,true);
            TextAt({A.x+S(39),A.y+S(4)},Labels[i],14.5f,Selected?Accent:Text,true);
            TextAt({A.x+S(39),A.y+S(25)},Details[i],9.5f,Muted);
            ImGui::PopID();
        }
        Draw->AddCircleFilled({P.x+S(28),P.y+S(598)},S(3),TelemetryValue.Connected?U(Accent):U(Muted));
        TextAt({P.x+S(40),P.y+S(587)},TelemetryValue.Connected?"Oyuna bağlı":"Bağlantı bekleniyor",12.5f,Text,true);
        TextAt({P.x+S(24),P.y+S(624)},"INS  ·  Menüyü aç / kapat",12,Muted);
        TextAt({P.x+S(237),P.y+S(25)},"READY OR NOT",10.5f,Muted,true);
        TextAt({P.x+S(237),P.y+S(44)},Labels[Page],25,Text,true);
        if(Page>=1 && Page<=4) {
            const auto Game=GameplayUI::Snapshot();
            if(Game.Ready && !Game.Authority) TextAt({P.x+S(237),P.y+S(75)},"Bu oturumda oyun kontrolü için ev sahibi yetkisi gerekiyor.",11,Accent);
        }
        ImGui::SetCursorPos({S(946),S(27)});
        if (ImGui::Button("X",{S(30),S(30)})) SetOpen(false);
        Draw->AddLine({P.x+S(232),P.y+S(88)},{P.x+S(976),P.y+S(88)},U(Border));
        ImGui::SetCursorPos({S(232),S(108)});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{S(16),S(14)});
        ImGui::BeginChild("content",{S(744),S(481)},ImGuiChildFlags_None);

        if (Page==0) {
            BeginCard("master","GÖRSEL DESTEK",114);
            Switch("ESP", "Tüm görsel işaretlerin ana kontrolü",Settings.ESP.bEnabled,64); EndCard();
            ImGui::Dummy({0,S(5)});
            BeginCard("summary","SAHADAKİ DURUM",86);
            ImGui::TextColored(Accent,"%d nesne",TelemetryValue.Entities); ImGui::SameLine(S(150));
            ImGui::TextColored(Accent,"%d görev hedefi",TelemetryValue.Targets); ImGui::SameLine(S(340));
            ImGui::TextDisabled("%.0f FPS",IO.Framerate); EndCard();
            ImGui::Dummy({0,S(5)});
            Preview(Settings.ESP,ImGui::GetContentRegionAvail().x,S(231));
        } else if (Page==1) {
            const auto Live=GameplayUI::Snapshot();
            BeginCard("health","OYUNCU",240);
            if(Live.Ready) ImGui::TextDisabled("Sağlık %.0f / %.0f",Live.Health,Live.MaxHealth);else ImGui::TextDisabled("Görev oyuncusu bekleniyor.");
            Switch("Sınırsız sağlık","Hasarı engeller; etkinleştirilince sağlığı doldurur",Settings.Gameplay.bPlayerHealth);
            Switch("Yaralanmayı kapat","Uzuv yaralarını temizler; yeni uzuv hasarını önler",Settings.Gameplay.bNoInjuries);EndCard();
            ImGui::Dummy({0,S(7)});BeginCard("team-health","TAKIM",129);
            Switch("Sınırsız sağlık · takım","Canlı SWAT takım arkadaşlarını korur",Settings.Gameplay.bTeamHealth);EndCard();
            ImGui::TextDisabled("%d canlı takım arkadaşı",Live.Allies);
        } else if(Page==2) {
            const auto Live=GameplayUI::Snapshot();
            BeginCard("recoil","SİLAH KONTROLÜ",212);
            Switch("Sekmeyi kapat","Geri tepme gücünü seçtiğin orana indir",Settings.Gameplay.bNoRecoil);
            ImGui::BeginDisabled(!Settings.Gameplay.bNoRecoil);
            Slider("Kalan sekme oranı",Settings.Gameplay.RecoilScale,0,1,"%.2f x");ImGui::EndDisabled();EndCard();
            ImGui::Dummy({0,S(7)});BeginCard("reload","ŞARJÖR",129);
            Switch("Şarjör değişimi gerekmesin","Mevcut şarjörün mermisini dolu tutar",Settings.Gameplay.bNoReload);EndCard();
            ImGui::TextDisabled("Kuşanılan silah");ImGui::TextWrapped("%s",Live.Weapon.empty()?"Silah bekleniyor":Live.Weapon.c_str());
        } else if(Page==3) {
            auto SpeedCard=[&](const char* Id,const char* Title,const char* Label,const char* Detail,bool& On,float& Value,float Min,float Max,const char* Format) {
                BeginCard(Id,Title,219);Switch(Label,Detail,On);
                ImGui::BeginDisabled(!On);Slider("Değer",Value,Min,Max,Format);ImGui::EndDisabled();EndCard();
            };
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{S(4),S(6)});
            ImGui::BeginTable("movement-grid",2,ImGuiTableFlags_SizingStretchSame);
            ImGui::TableNextColumn();SpeedCard("max-speed","HIZ SINIRI","Maksimum hareket hızı","Üst hızı ayarla",Settings.Gameplay.bMaxSpeed,Settings.Gameplay.MaxSpeed,100,3000,"%.0f cm/sn");
            ImGui::TableNextColumn();SpeedCard("crouch-speed","EĞİLME","Eğilme hızı çarpanı","Eğilerek hareket hızı",Settings.Gameplay.bCrouchSpeed,Settings.Gameplay.CrouchMultiplier,.25f,5,"%.2f x");
            ImGui::TableNextColumn();SpeedCard("slow-speed","YAVAŞ YÜRÜYÜŞ","Yavaş yürüyüş çarpanı","Yavaş hareket hızını çarp",Settings.Gameplay.bSlowWalkSpeed,Settings.Gameplay.SlowWalkMultiplier,.25f,5,"%.2f x");
            ImGui::TableNextColumn();SpeedCard("walk-speed","YÜRÜYÜŞ","Yürüme hızı çarpanı","Normal yürüyüş hızını çarp",Settings.Gameplay.bWalkSpeed,Settings.Gameplay.WalkMultiplier,.25f,5,"%.2f x");
            ImGui::EndTable();ImGui::PopStyleVar();
        } else if(Page==4) {
            const auto Live=GameplayUI::Snapshot();
            BeginCard("npc-actions","DÜŞMAN KONTROLÜ",285);
            Switch("Teslim olmaya zorla","Etki alanındaki her düşmana bir kez uygula",Settings.Gameplay.bForceSurrender);
            Switch("Düşmanlar her zaman teslim olur","Teslim olmayan veya vazgeçen düşmanları tekrar zorla",Settings.Gameplay.bAlwaysSurrender);
            Switch("Düşmanlar silahını bıraksın","Etki alanındaki düşmanların silahlarını bıraktır",Settings.Gameplay.bDropWeapons);EndCard();
            ImGui::Dummy({0,S(7)});BeginCard("npc-range","ETKİ ALANI",143);
            Slider("NPC mesafesi · 0 = tüm harita",Settings.Gameplay.NPCRange,0,250,"%.0f m");EndCard();
            ImGui::Dummy({0,S(7)});BeginCard("doors","KAPILAR",128);
            Switch("Tüm kapıların kilidini aç","Etkin olduğu sürece haritadaki kilitleri açık tut",Settings.Gameplay.bUnlockDoors);EndCard();
            ImGui::Dummy({0,S(7)});BeginCard("teleport","NPC IŞINLANMA",347);
            Switch("Işınlanmayı etkinleştir","Seçilen NPC için iki yönlü kontrol",Settings.Gameplay.bTeleport);
            const char* Filters[]={"Tüm NPC'ler","Düşmanlar","Siviller","Takım"};
            ImGui::SetNextItemWidth(-1);ImGui::Combo("##npc-filter",&Settings.Gameplay.NPCFilter,Filters,4);
            static uint64_t Selected=0;
            std::vector<const GameplayUI::NPC*> Candidates;
            for(const auto& N:Live.People) if(Settings.Gameplay.NPCFilter==0 || N.Kind==Settings.Gameplay.NPCFilter) Candidates.push_back(&N);
            auto Chosen=std::find_if(Candidates.begin(),Candidates.end(),[&](const GameplayUI::NPC* N){return N->Id==Selected;});
            if(Chosen==Candidates.end()) Selected=Candidates.empty()?0:Candidates.front()->Id;
            const auto Current=std::find_if(Candidates.begin(),Candidates.end(),[&](const GameplayUI::NPC* N){return N->Id==Selected;});
            auto Caption=[](const GameplayUI::NPC& N) {return N.Name+" · "+std::to_string(static_cast<int>(N.Distance))+" m";};
            const auto Preview=Current==Candidates.end()?std::string("NPC bulunamadı"):Caption(**Current);
            ImGui::BeginDisabled(!Settings.Gameplay.bTeleport || !Live.Ready || !Live.Authority);
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##npc-target",Preview.c_str())) {
                for(const auto* N:Candidates) {ImGui::PushID(static_cast<int>(N->Id>>32));if(ImGui::Selectable(Caption(*N).c_str(),N->Id==Selected)) Selected=N->Id;ImGui::PopID();}
                ImGui::EndCombo();
            }
            Slider("Hedefle aradaki mesafe",Settings.Gameplay.TeleportDistance,1,6,"%.1f m");
            ImGui::BeginDisabled(Selected==0);
            if(ImGui::Button("NPC'ye git",{S(190),S(37)})) GameplayUI::Queue({GameplayUI::Action::TeleportToNPC,Selected});
            ImGui::SameLine();if(ImGui::Button("NPC'yi yanıma getir",{S(220),S(37)})) GameplayUI::Queue({GameplayUI::Action::BringNPC,Selected});
            ImGui::EndDisabled();ImGui::EndDisabled();EndCard();
            ImGui::TextWrapped("Teslim ve silah bırakmayı kapatmak yeni zorlamaları durdurur; görevde gerçekleşen işlemler kalır.");
            ImGui::TextDisabled("%d düşman · %d kapı",Live.Suspects,Live.Doors);
        } else if (Page==5) {
            ImGui::BeginTable("visual-grid",2,ImGuiTableFlags_SizingStretchSame);
            ImGui::TableNextColumn(); BeginCard("layers","ÇİZİM KATMANLARI",295);
            Switch("Kutular","Karakter ve nesne çerçeveleri",Settings.ESP.bBox);
            Switch("İskelet","Gerçek kemik bağlantıları",Settings.ESP.bSkeleton);
            Switch("Tür etiketleri","Sivil, düşman, silah ve durum",Settings.ESP.bNameTags);
            Switch("Mesafe","Hedefe olan uzaklık, metre",Settings.ESP.bDistance); EndCard();
            ImGui::TableNextColumn(); BeginCard("stroke","ÇİZİM AYARLARI",295);
            Slider("Kutu kalınlığı",Settings.ESP.BoxThickness,0.5f,6,"%.1f px");
            Slider("İskelet kalınlığı",Settings.ESP.SkeletonThickness,0.5f,6,"%.1f px");
            Slider("Yazı boyutu",Settings.ESP.TextScale,0.6f,2,"%.2f x"); EndCard();
            ImGui::EndTable(); ImGui::Dummy({0,S(7)});
            BeginCard("readability","OKUNURLUK",338);
            Slider("Yazı opaklığı",Settings.ESP.TextOpacity,0.25f,1,"%.2f");
            Slider("Etiket arka planı",Settings.ESP.LabelBackground,0,0.85f,"%.2f");
            Slider("İskelet opaklığı",Settings.ESP.SkeletonOpacity,0.2f,1,"%.2f");
            Switch("Etiket çakışmalarını azalt","Görev isimleri korunur; üst üste gelen normal etiketler sadeleşir",Settings.ESP.bDeclutterLabels); EndCard();
            ImGui::Dummy({0,S(7)});
            BeginCard("people","KİŞİLER · Görev hedefleri ayrıca yönetilir",229);
            Switch("Düşmanlar","Normal şüpheli işaretleri",Settings.ESP.bSuspects);
            Switch("Siviller","Normal sivil işaretleri",Settings.ESP.bCivilians);
            Switch("Müttefikler","Takım ve dost karakterler",Settings.ESP.bAllies); EndCard();
            ImGui::Dummy({0,S(7)});
            BeginCard("objects","NESNELER",294);
            Switch("Tuzaklar","Kapı ve çevre tuzakları",Settings.ESP.bTraps);
            Switch("Kanıtlar","Toplanabilir normal kanıtlar",Settings.ESP.bEvidence);
            Switch("Silahlar","Tabancalar ve uzun namlulu kanıt silahları",Settings.ESP.bDroppedWeapon);
            Switch("Bildirilecek nesneler","Görevle bağlantılı rapor noktaları",Settings.ESP.bReportable); EndCard();
        } else if (Page==6) {
            BeginCard("mission-master","GÖREV TAKİBİ",118);
            Switch("Özel görev hedefleri","Aktif görevin gerçek hedeflerini vurgula",Settings.ESP.bObjectives,67); EndCard();
            ImGui::Dummy({0,S(7)});
            ImGui::BeginDisabled(!Settings.ESP.bObjectives);
            BeginCard("mission-types","HEDEF TÜRLERİ",231);
            Switch("İsimli kişiler","Tutukla, kurtar ve etkisiz hale getir",Settings.ESP.bNamedTargets);
            Switch("Bombalar","Aktif bomba ve varsa kalan süre",Settings.ESP.bBombTargets);
            Switch("Yer ve görev nesneleri","Bilgisayar, kanıt, ofis ve etkileşim noktaları",Settings.ESP.bObjectiveObjects); EndCard();
            ImGui::EndDisabled(); ImGui::Dummy({0,S(10)});
            ImGui::PushStyleColor(ImGuiCol_Text,Muted);
            ImGui::TextWrapped("Sarı başlıklar göreve özeldir. Harita ve mod değişince hedefler yenilenir; tamamlanan görevlerin özel işareti kalkar.");
            ImGui::PopStyleColor();
        } else if (Page==7) {
            ImGui::BeginTable("color-grid",2,ImGuiTableFlags_SizingStretchSame);
            ImGui::TableNextColumn(); BeginCard("character-colors","KİŞİ RENKLERİ",300);
            ColorRow("Düşman",Settings.ESP.ColSuspect); ColorRow("Sivil",Settings.ESP.ColCivilian);
            ColorRow("Müttefik",Settings.ESP.ColAlly); ColorRow("Kelepçeli",Settings.ESP.ColArrested); EndCard();
            ImGui::TableNextColumn(); BeginCard("object-colors","NESNE VE HEDEF RENKLERİ",300);
            ColorRow("Görev hedefi",Settings.ESP.ColObjective); ColorRow("Silah",Settings.ESP.ColDroppedWeapon);
            ColorRow("Kanıt",Settings.ESP.ColEvidence); ColorRow("Tuzak",Settings.ESP.ColTrap);
            ColorRow("Bildirilecek nesne",Settings.ESP.ColReportable); ColorRow("Bomba",Settings.ESP.ColBomb); EndCard();
            ImGui::EndTable(); ImGui::Dummy({0,S(7)});
            Preview(Settings.ESP,ImGui::GetContentRegionAvail().x,S(150));
        } else {
            BeginCard("misc","OTURUM",114);
            Switch("Resmî puan yaması","Puan yamasını bu oturumda uygula",Settings.Misc.bForceOfficial,65); EndCard();
            ImGui::Dummy({0,S(2)}); BeginCard("profile","AYAR PROFİLİ",114);
            ImGui::TextDisabled("Ayarlarını sakla veya son kayda geri dön.");
            if (ImGui::Button("Kaydet",{S(126),S(37)})) Commands.fetch_or(Save);
            ImGui::SameLine(); if (ImGui::Button("Kaydı yükle",{S(126),S(37)})) Commands.fetch_or(Load);
            ImGui::SameLine(); if (ImGui::Button("Varsayılana dön",{S(162),S(37)})) Commands.fetch_or(Defaults); EndCard();
            ImGui::Dummy({0,S(2)}); BeginCard("diagnostics","TANILAMA",84);
            if (ImGui::Button("Nesne listesini kayda yaz",{S(245),S(36)})) Commands.fetch_or(Dump); EndCard();
            ImGui::Dummy({0,S(2)}); BeginCard("detach","OTURUMDAN KALDIR",112);
            ImGui::TextDisabled("Oyunu açık tutar; menüyü ve ESP'yi tamamen kaldırır.");
            ImGui::PushStyleColor(ImGuiCol_Button,{0.32f,0.115f,0.13f,1});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.48f,0.15f,0.17f,1});
            if (ImGui::Button("Hileyi tamamen kaldır",{S(246),S(35)})) RequestUnload();
            ImGui::PopStyleColor(2); EndCard();
        }
        ImGui::EndChild(); ImGui::PopStyleVar();
        // All edits cross a single lock-protected boundary; worker/game threads read snapshots.
        if (Initial != Settings) Config::Set(Settings);
        Draw->AddLine({P.x+S(232),P.y+S(610)},{P.x+S(976),P.y+S(610)},U(Border));
        TextAt({P.x+S(237),P.y+S(630)},Message.empty()?"Değişiklikler anında uygulanır.":Message.c_str(),12.5f,Message.empty()?Muted:Accent);
        ImGui::SetCursorPos({S(756),S(622)});
        if (ImGui::Button("Menüyü gizle",{S(122),S(33)})) SetOpen(false);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,Accent); ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.89f,0.79f,0.55f,1});
        ImGui::PushStyleColor(ImGuiCol_Text,{0.07f,0.06f,0.04f,1});
        if (ImGui::Button("Kaydet",{S(84),S(33)})) Commands.fetch_or(Save);
        ImGui::PopStyleColor(3);
        ImGui::End();
    }
}
