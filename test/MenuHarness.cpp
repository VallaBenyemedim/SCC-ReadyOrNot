#include "GraphicsOverlay.hpp"
#include "MenuUI.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "ESPSampleScene.hpp"
#include "GameplayUI.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
static bool InputCaptured=false;
static int LegacyF11=0;
static void Check(HRESULT H,const char* What) { if (FAILED(H)) throw std::runtime_error(What); }
static void Require(bool Test,const char* What) { if (!Test) throw std::runtime_error(What); }
static LRESULT CALLBACK WndProc(HWND W,UINT M,WPARAM A,LPARAM B) {
    if (M==WM_KEYDOWN && A==VK_F11) ++LegacyF11;
    return DefWindowProcW(W,M,A,B);
}
static void Pump() { MSG M; while (PeekMessageW(&M,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&M); DispatchMessageW(&M); } }

int wmain(int Count,wchar_t** Args) {
    Logger::Init(false);
    try {
        const auto ConfigPath=Config::GetConfigPath();
        WritePrivateProfileStringA("Keys","UnloadCheat","122",ConfigPath.c_str());
        auto Custom=Config::Settings{};
        Custom.ESP.bSkeleton=false; Custom.ESP.bDistance=false;
        Custom.ESP.bSuspects=false; Custom.ESP.bCivilians=false; Custom.ESP.bAllies=false;
        Custom.ESP.bNamedTargets=false; Custom.ESP.bBombTargets=false; Custom.ESP.bObjectiveObjects=false;
        Custom.ESP.bTraps=false; Custom.ESP.bEvidence=false; Custom.ESP.bDroppedWeapon=false; Custom.ESP.bReportable=false;
        Custom.ESP.BoxThickness=3.5f; Custom.ESP.SkeletonThickness=2.5f; Custom.ESP.TextScale=1.25f;
        Custom.ESP.TextOpacity=0.625f;Custom.ESP.LabelBackground=0.375f;Custom.ESP.SkeletonOpacity=0.5f;
        Custom.ESP.bDeclutterLabels=false;
        Custom.ESP.ColBomb={0.25f,0.5f,0.75f,0.5f}; Custom.ESP.ColCivilian={0.5f,0.25f,0.75f,1};
        Custom.Misc.bForceOfficial=false;
        auto& G=Custom.Gameplay;
        G.bNoRecoil=G.bNoReload=G.bPlayerHealth=G.bNoInjuries=G.bTeamHealth=true;
        G.bMaxSpeed=G.bCrouchSpeed=G.bSlowWalkSpeed=G.bWalkSpeed=true;
        G.bForceSurrender=G.bAlwaysSurrender=G.bDropWeapons=G.bUnlockDoors=G.bTeleport=true;
        G.RecoilScale=.25f;G.MaxSpeed=1200;G.CrouchMultiplier=2;G.SlowWalkMultiplier=2.5f;G.WalkMultiplier=3;
        G.NPCRange=75;G.TeleportDistance=3.5f;G.NPCFilter=2;
        Config::Set(Custom); Config::Save(); Config::Set(Config::Settings{}); Config::Load();
        Require(Config::Snapshot()==Custom,"all settings and colors persist across Save/Load");
        char RemovedKeys[64]{};
        GetPrivateProfileStringA("Keys",nullptr,"",RemovedKeys,sizeof(RemovedKeys),ConfigPath.c_str());
        Require(RemovedKeys[0]==0,"legacy feature keybinds removed from saved profile");
        Config::Set(Config::Settings{}); Config::Save();
        Check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM init");
        WNDCLASSW C{}; C.lpfnWndProc=WndProc; C.hInstance=GetModuleHandleW(nullptr); C.lpszClassName=L"SCCReadyOrNotMenuHarness";
        RegisterClassW(&C);
        HWND Window=CreateWindowExW(0,C.lpszClassName,L"SCC — Ready or Not Menu Harness",WS_POPUP,0,0,1280,800,nullptr,nullptr,C.hInstance,nullptr);
        Require(Window!=nullptr,"test window");
        ComPtr<IDXGIFactory4> Factory; Check(CreateDXGIFactory1(IID_PPV_ARGS(&Factory)),"factory");
        ComPtr<ID3D12Device> Device; Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&Device)),"device");
        D3D12_COMMAND_QUEUE_DESC Q{}; Q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> Queue; Check(Device->CreateCommandQueue(&Q,IID_PPV_ARGS(&Queue)),"queue");
        DXGI_SWAP_CHAIN_DESC1 Desc{}; Desc.Width=1280; Desc.Height=800; Desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        Desc.SampleDesc.Count=1; Desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; Desc.BufferCount=2; Desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> Swap1; Check(Factory->CreateSwapChainForHwnd(Queue.Get(),Window,&Desc,nullptr,nullptr,&Swap1),"swapchain");
        ComPtr<IDXGISwapChain3> Swap; Check(Swap1.As(&Swap),"swapchain3"); Swap1.Reset();
        D3D12_DESCRIPTOR_HEAP_DESC HD{}; HD.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV; HD.NumDescriptors=2;
        ComPtr<ID3D12DescriptorHeap> Heap; Check(Device->CreateDescriptorHeap(&HD,IID_PPV_ARGS(&Heap)),"RTV heap");
        const UINT RtvSize=Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        std::vector<ComPtr<ID3D12Resource>> Buffers(2);
        auto GetBuffers=[&] { auto H=Heap->GetCPUDescriptorHandleForHeapStart(); for(int I=0;I<2;++I) { Check(Swap->GetBuffer(I,IID_PPV_ARGS(&Buffers[I])),"buffer"); Device->CreateRenderTargetView(Buffers[I].Get(),nullptr,H); H.ptr+=RtvSize; } };
        GetBuffers();
        ComPtr<ID3D12CommandAllocator> Allocator; Check(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&Allocator)),"allocator");
        ComPtr<ID3D12GraphicsCommandList> List; Check(Device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,Allocator.Get(),nullptr,IID_PPV_ARGS(&List)),"command list"); List->Close();
        ComPtr<ID3D12Fence> Fence; Check(Device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&Fence)),"fence");
        HANDLE Event=CreateEventW(nullptr,FALSE,FALSE,nullptr); UINT64 Serial=0;
        auto Wait=[&] { ++Serial; Check(Queue->Signal(Fence.Get(),Serial),"signal"); if (Fence->GetCompletedValue()<Serial) { Fence->SetEventOnCompletion(Serial,Event); WaitForSingleObject(Event,INFINITE); } };
        auto Barrier=[&](ID3D12Resource* Buffer,D3D12_RESOURCE_STATES Before,D3D12_RESOURCE_STATES After) {
            D3D12_RESOURCE_BARRIER B{}; B.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; B.Transition.pResource=Buffer;
            B.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; B.Transition.StateBefore=Before; B.Transition.StateAfter=After; List->ResourceBarrier(1,&B);
        };
        auto Submit=[&] { Check(List->Close(),"close"); ID3D12CommandList* Lists[]={List.Get()}; Queue->ExecuteCommandLists(1,Lists); Wait(); };
        auto Render=[&] {
            const UINT Index=Swap->GetCurrentBackBufferIndex(); Allocator->Reset(); List->Reset(Allocator.Get(),nullptr);
            Barrier(Buffers[Index].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
            auto H=Heap->GetCPUDescriptorHandleForHeapStart(); H.ptr+=Index*RtvSize;
            const float Clear[]={0.035f,0.055f,0.07f,1}; List->ClearRenderTargetView(H,Clear,0,nullptr);
            Barrier(Buffers[Index].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT); Submit();
            Check(Swap->Present(0,0),"present"); Wait(); return Index;
        };
        auto Screenshot=[&](UINT Index,const wchar_t* Path) {
            auto D=Buffers[Index]->GetDesc(); D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layout{}; UINT64 Bytes=0;
            Device->GetCopyableFootprints(&D,0,1,0,&Layout,nullptr,nullptr,&Bytes);
            D3D12_HEAP_PROPERTIES HP{}; HP.Type=D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC BD{}; BD.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; BD.Width=Bytes; BD.Height=1; BD.DepthOrArraySize=1;
            BD.MipLevels=1; BD.SampleDesc.Count=1; BD.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ComPtr<ID3D12Resource> Readback; Check(Device->CreateCommittedResource(&HP,D3D12_HEAP_FLAG_NONE,&BD,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&Readback)),"readback buffer");
            Allocator->Reset(); List->Reset(Allocator.Get(),nullptr);
            Barrier(Buffers[Index].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION To{}; To.pResource=Readback.Get(); To.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; To.PlacedFootprint=Layout;
            D3D12_TEXTURE_COPY_LOCATION From{}; From.pResource=Buffers[Index].Get(); From.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            List->CopyTextureRegion(&To,0,0,0,&From,nullptr);
            Barrier(Buffers[Index].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT); Submit();
            void* Pixels=nullptr; D3D12_RANGE Range{0,static_cast<SIZE_T>(Bytes)}; Check(Readback->Map(0,&Range,&Pixels),"map readback");
            ComPtr<IWICImagingFactory> Wic; Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&Wic)),"WIC factory");
            ComPtr<IWICStream> Stream; Wic->CreateStream(&Stream); Check(Stream->InitializeFromFilename(Path,GENERIC_WRITE),"PNG output");
            ComPtr<IWICBitmapEncoder> Encoder; Wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&Encoder); Encoder->Initialize(Stream.Get(),WICBitmapEncoderNoCache);
            ComPtr<IWICBitmapFrameEncode> Frame; Encoder->CreateNewFrame(&Frame,nullptr); Frame->Initialize(nullptr);
            Frame->SetSize(static_cast<UINT>(D.Width),D.Height); WICPixelFormatGUID Format=GUID_WICPixelFormat32bppBGRA; Check(Frame->SetPixelFormat(&Format),"PNG pixel format");
            ComPtr<IWICBitmap> Bitmap;
            Check(Wic->CreateBitmapFromMemory(static_cast<UINT>(D.Width),D.Height,GUID_WICPixelFormat32bppRGBA,
                Layout.Footprint.RowPitch,static_cast<UINT>(Bytes),static_cast<BYTE*>(Pixels),&Bitmap),"RGBA bitmap");
            ComPtr<IWICFormatConverter> Converter; Wic->CreateFormatConverter(&Converter);
            Check(Converter->Initialize(Bitmap.Get(),Format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom),"PNG format conversion");
            Check(Frame->WriteSource(Converter.Get(),nullptr),"PNG pixels");
            Check(Frame->Commit(),"PNG frame"); Check(Encoder->Commit(),"PNG commit"); D3D12_RANGE Empty{0,0}; Readback->Unmap(0,&Empty);
        };
        MenuUI::SetTelemetry({37,4,true}); MenuUI::SetOpen(true);
        GameplayUI::State Game;Game.Ready=Game.Authority=true;Game.Health=Game.MaxHealth=200;Game.Allies=4;Game.Suspects=13;Game.Doors=91;Game.Weapon="Primary_M4_C";
        Game.People={{(1ULL<<32)|1,"Düşman #1",12,1},{(2ULL<<32)|1,"Maria Lopez",19,2},{(3ULL<<32)|1,"SWAT #3",4,3}};GameplayUI::Publish(std::move(Game));
        const bool ScenePreview=Count>2 && _wtoi(Args[2])==9;
        if (Count>2) MenuUI::SetPage(_wtoi(Args[2]));
        Require(GraphicsOverlay::Initialize(Window,[](bool Open){InputCaptured=Open;}),"overlay initialize");
        UINT LastIndex=0;
        for (int I=0;I<90;++I) {
            Pump();
            if(I==20 && Count>3) {
                auto& Context=*ImGui::GetCurrentContext();
                for(auto* W:Context.Windows) if(std::strstr(W->Name,"RoN Control/content_")) ImGui::SetScrollY(W,static_cast<float>(_wtoi(Args[3])));
            }
            if (ScenePreview) {OverlayScene::Submit(ESPSampleScene()); if(I==26) MenuUI::SetOpen(false);}
            if (I==28 && Count>1) {Screenshot(LastIndex,Args[1]);if(ScenePreview) MenuUI::SetOpen(true);}
            if (I==30) {
                Wait(); for (auto& B:Buffers) B.Reset();
                Check(Swap->ResizeBuffers(2,1024,720,DXGI_FORMAT_R8G8B8A8_UNORM,0),"resize");
                SetWindowPos(Window,nullptr,0,0,1024,720,SWP_NOZORDER|SWP_NOACTIVATE); GetBuffers();
            }
            if (I==44) SendMessageW(Window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(10,10));
            if (I==45) { SendMessageW(Window,WM_KEYDOWN,VK_INSERT,0); Pump(); Require(!MenuUI::IsOpen(),"Insert closes menu"); SendMessageW(Window,WM_LBUTTONUP,0,MAKELPARAM(10,10)); }
            if (I==55) { SendMessageW(Window,WM_KEYDOWN,VK_INSERT,0); Pump(); Require(MenuUI::IsOpen(),"Insert opens menu"); }
            LastIndex=Render();
            if (I==55) Require(!ImGui::GetIO().MouseDown[0],"mouse release preserved across menu toggle");
        }
        Require(GraphicsOverlay::Ready() && GraphicsOverlay::RenderedFrames()>20,"menu frames rendered");
        MenuUI::SetOpen(false); Pump(); OverlayScene::Submit(ESPSampleScene());Render();
        const auto SceneStats=OverlayScene::LastStats();
        Require(SceneStats.Strokes>30 && SceneStats.Labels==4 && SceneStats.Suppressed==1,"ESP draws while menu hidden; overlapping normal label suppressed, mission name preserved");
        MenuUI::SetOpen(false); Pump(); SendMessageW(Window,WM_KEYDOWN,VK_F11,0);
        Require(!MenuUI::UnloadRequested() && LegacyF11==1,"F11 must remain a game key, not an unload shortcut");
        MenuUI::RequestUnload(); Pump(); Require(MenuUI::UnloadRequested(),"unload request");
        Require(GraphicsOverlay::Shutdown(),"overlay shutdown"); Require(!InputCaptured,"input restored");
        Require(OverlayScene::LastStats().Labels==0,"ESP scene cleared on unload");
        auto Frames=GraphicsOverlay::RenderedFrames(); Render();
        Require(GraphicsOverlay::RenderedFrames()==Frames,"all rendering hooks removed");
        printf("PASS: settings roundtrip, antialiased ESP and quiet labels, overlap priority, DX12 resize, Insert toggle, disabled F11 and complete cleanup (%llu frames)\n",Frames);
        Wait(); CloseHandle(Event); DestroyWindow(Window); UnregisterClassW(C.lpszClassName,C.hInstance);
        Logger::Cleanup(); return 0;
    } catch (const std::exception& E) { fprintf(stderr,"FAIL: %s\n",E.what()); GraphicsOverlay::Shutdown(); Logger::Cleanup(); return 1; }
}
