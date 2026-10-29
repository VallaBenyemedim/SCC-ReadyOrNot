#include "GraphicsOverlay.hpp"
#include "MenuUI.hpp"
#include "OverlayScene.hpp"
#include "Logger.hpp"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_dx12.h"
#include "MinHook.h"
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <commctrl.h>
#include <wrl/client.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
using Microsoft::WRL::ComPtr;

namespace GraphicsOverlay {
    namespace {
        HWND GameWindow = nullptr;
        InputCallback GameInput = nullptr;
        UINT ControlMessage = 0;
        constexpr UINT_PTR SubclassId = 0x524F4E43;
        std::atomic<bool> Installed{false}, Stopping{false}, Initialized{false};
        std::atomic<bool> ResetInput{false};
        std::atomic<uint64_t> Frames{0};
        std::atomic<int> Calls{0};
        std::atomic<HHOOK> InstallHook{nullptr};
        DWORD WindowThread = 0;
        std::mutex RenderMutex, QueueMutex, InputMutex;
        ComPtr<ID3D12CommandQueue> CandidateQueue;
        struct Message { UINT Id; WPARAM W; LPARAM L; };
        std::vector<Message> Messages;
        struct CallScope { CallScope(){++Calls;} ~CallScope(){--Calls;} };
        using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
        using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
        using ExecuteFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList* const*);
        PresentFn Present12=nullptr, Present11=nullptr;
        ResizeFn Resize12=nullptr, Resize11=nullptr;
        ExecuteFn Execute=nullptr;
        std::vector<void*> HookTargets;

        struct Frame {
            ComPtr<ID3D12CommandAllocator> Allocator;
            ComPtr<ID3D12Resource> Buffer;
            D3D12_CPU_DESCRIPTOR_HANDLE Rtv{};
            uint64_t FenceValue = 0;
        };
        struct Renderer {
            ImGuiContext* Context = nullptr;
            ComPtr<IDXGISwapChain> Swap;
            bool DX12 = false;
            bool Backend = false;
            ComPtr<ID3D12Device> Device12;
            ComPtr<ID3D12CommandQueue> Queue;
            ComPtr<ID3D12GraphicsCommandList> List;
            ComPtr<ID3D12DescriptorHeap> RtvHeap, SrvHeap;
            ComPtr<ID3D12Fence> Fence;
            HANDLE FenceEvent = nullptr;
            uint64_t NextFence = 0;
            UINT DescriptorSize = 0;
            std::vector<UINT> FreeDescriptors;
            std::vector<Frame> Frames;
            ComPtr<ID3D11Device> Device11;
            ComPtr<ID3D11DeviceContext> Context11;
            ComPtr<ID3D11RenderTargetView> Rtv11;
        } R;

        void Check(HRESULT Result, const char* Label) {
            if (FAILED(Result)) throw std::runtime_error(Label);
        }
        void WaitFence(uint64_t Value) {
            if (!Value || !R.Fence || R.Fence->GetCompletedValue() >= Value) return;
            Check(R.Fence->SetEventOnCompletion(Value,R.FenceEvent),"GPU fence event");
            WaitForSingleObject(R.FenceEvent,INFINITE);
        }
        void WaitGpu() {
            if (!R.Queue || !R.Fence) return;
            const auto Value=++R.NextFence;
            Check(R.Queue->Signal(R.Fence.Get(),Value),"GPU fence signal");
            WaitFence(Value);
        }
        bool IsTarget(IDXGISwapChain* Swap) {
            DXGI_SWAP_CHAIN_DESC Desc{};
            return SUCCEEDED(Swap->GetDesc(&Desc)) && Desc.OutputWindow==GameWindow;
        }
        void VisibilityChanged(bool) {
            ResetInput.store(true);
            if (GameWindow && Installed) PostMessageW(GameWindow,ControlMessage,1,0);
        }
        LRESULT CALLBACK WindowProc(HWND Window,UINT Msg,WPARAM W,LPARAM L,UINT_PTR,DWORD_PTR) {
            CallScope Scope;
            if (Msg==ControlMessage) {
                if (W==1 && GameInput) GameInput(MenuUI::IsOpen());
                if (W==2) {
                    if (GameInput) GameInput(false);
                    RemoveWindowSubclass(Window,WindowProc,SubclassId);
                    Installed.store(false);
                }
                return 0;
            }
            if (Msg==WM_NCDESTROY) {
                if (GameInput) GameInput(false);
                RemoveWindowSubclass(Window,WindowProc,SubclassId);
                Installed.store(false);
                return DefSubclassProc(Window,Msg,W,L);
            }
            if ((Msg==WM_KEYDOWN || Msg==WM_SYSKEYDOWN) && W==VK_INSERT) {
                if (!(L & (1LL<<30))) {
                    MenuUI::SetOpen(!MenuUI::IsOpen());
                    Logger::Log("[UI] Insert: menu %s",MenuUI::IsOpen()?"OPEN":"CLOSED");
                }
                return 0;
            }
            if ((Msg==WM_KEYUP || Msg==WM_SYSKEYUP) && W==VK_INSERT) return 0;
            if (MenuUI::IsOpen() && !Stopping) {
                if (Msg==WM_SETCURSOR && LOWORD(L)==HTCLIENT) { SetCursor(nullptr); return TRUE; }
                const bool Mouse=(Msg>=WM_MOUSEFIRST && Msg<=WM_MOUSELAST);
                const bool Keyboard=(Msg>=WM_KEYFIRST && Msg<=WM_KEYLAST);
                const bool Focus=Msg==WM_SETFOCUS || Msg==WM_KILLFOCUS;
                if (Mouse || Keyboard || Focus) {
                    std::lock_guard<std::mutex> Lock(InputMutex);
                    if (Messages.size()<2048) Messages.push_back({Msg,W,L});
                }
                if (Msg==WM_INPUT) return DefWindowProcW(Window,Msg,W,L);
                if (Mouse || Keyboard) return 0;
            }
            return DefSubclassProc(Window,Msg,W,L);
        }
        bool InstallSubclass() {
            if (!SetWindowSubclass(GameWindow,WindowProc,SubclassId,0)) return false;
            Installed.store(true);
            PostMessageW(GameWindow,ControlMessage,1,0);
            return true;
        }
        LRESULT CALLBACK InstallProc(int Code,WPARAM W,LPARAM L) {
            CallScope Scope;
            if (Code>=0) {
                auto* Msg=reinterpret_cast<CWPSTRUCT*>(L);
                if (Msg->hwnd==GameWindow && Msg->message==ControlMessage && Msg->wParam==0) {
                    InstallSubclass();
                    if (auto Hook=InstallHook.exchange(nullptr)) UnhookWindowsHookEx(Hook);
                }
            }
            return CallNextHookEx(nullptr,Code,W,L);
        }
        void ConsumeInput() {
            std::vector<Message> Pending;
            { std::lock_guard<std::mutex> Lock(InputMutex); Pending.swap(Messages); }
            for (const auto& Msg:Pending) ImGui_ImplWin32_WndProcHandler(GameWindow,Msg.Id,Msg.W,Msg.L);
        }
        void DestroyRenderer() {
            if (!R.Context) { if (R.FenceEvent) CloseHandle(R.FenceEvent); R=Renderer{}; return; }
            ImGui::SetCurrentContext(R.Context);
            if (R.DX12) {
                if (SUCCEEDED(R.Device12->GetDeviceRemovedReason())) WaitGpu();
                if (R.Backend) ImGui_ImplDX12_Shutdown();
            } else if (R.Backend) ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext(R.Context);
            if (R.FenceEvent) CloseHandle(R.FenceEvent);
            R=Renderer{};
            Initialized.store(false);
        }
        bool CreateRenderer(IDXGISwapChain* Swap) {
            DXGI_SWAP_CHAIN_DESC Desc{}; Check(Swap->GetDesc(&Desc),"Swapchain description");
            R.Swap=Swap;
            R.DX12=SUCCEEDED(Swap->GetDevice(IID_PPV_ARGS(&R.Device12)));
            if (R.DX12) {
                { std::lock_guard<std::mutex> Lock(QueueMutex); R.Queue=CandidateQueue; }
                if (!R.Queue) { R.Swap.Reset(); R.Device12.Reset(); return false; }
                ComPtr<ID3D12Device> QueueDevice; R.Queue->GetDevice(IID_PPV_ARGS(&QueueDevice));
                if (QueueDevice.Get()!=R.Device12.Get()) { R.Queue.Reset(); return false; }
                D3D12_DESCRIPTOR_HEAP_DESC Heap{}; Heap.NumDescriptors=Desc.BufferCount; Heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                Check(R.Device12->CreateDescriptorHeap(&Heap,IID_PPV_ARGS(&R.RtvHeap)),"RTV descriptor heap");
                Heap.NumDescriptors=128; Heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; Heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                Check(R.Device12->CreateDescriptorHeap(&Heap,IID_PPV_ARGS(&R.SrvHeap)),"SRV descriptor heap");
                R.DescriptorSize=R.Device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                for (UINT i=128;i>0;--i) R.FreeDescriptors.push_back(i-1);
                R.Frames.resize(Desc.BufferCount);
                const auto RtvSize=R.Device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                auto Rtv=R.RtvHeap->GetCPUDescriptorHandleForHeapStart();
                for (UINT i=0;i<Desc.BufferCount;++i) {
                    auto& Frame=R.Frames[i]; Frame.Rtv=Rtv; Rtv.ptr+=RtvSize;
                    Check(R.Device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&Frame.Allocator)),"Command allocator");
                    Check(Swap->GetBuffer(i,IID_PPV_ARGS(&Frame.Buffer)),"Back buffer");
                    R.Device12->CreateRenderTargetView(Frame.Buffer.Get(),nullptr,Frame.Rtv);
                }
                Check(R.Device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,R.Frames[0].Allocator.Get(),nullptr,IID_PPV_ARGS(&R.List)),"UI command list");
                Check(R.List->Close(),"Close UI command list");
                Check(R.Device12->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&R.Fence)),"UI fence");
                R.FenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
                if (!R.FenceEvent) throw std::runtime_error("UI fence event");
            } else {
                Check(Swap->GetDevice(IID_PPV_ARGS(&R.Device11)),"D3D device");
                R.Device11->GetImmediateContext(&R.Context11);
                ComPtr<ID3D11Texture2D> Buffer; Check(Swap->GetBuffer(0,IID_PPV_ARGS(&Buffer)),"DX11 back buffer");
                Check(R.Device11->CreateRenderTargetView(Buffer.Get(),nullptr,&R.Rtv11),"DX11 RTV");
            }
            IMGUI_CHECKVERSION(); R.Context=ImGui::CreateContext(); ImGui::SetCurrentContext(R.Context);
            MenuUI::InitializeStyle();
            if (!ImGui_ImplWin32_Init(GameWindow)) throw std::runtime_error("ImGui Win32 init");
            if (R.DX12) {
                ImGui_ImplDX12_InitInfo Init{}; Init.Device=R.Device12.Get(); Init.CommandQueue=R.Queue.Get();
                Init.NumFramesInFlight=static_cast<int>(Desc.BufferCount); Init.RTVFormat=Desc.BufferDesc.Format;
                Init.DSVFormat=DXGI_FORMAT_UNKNOWN; Init.SrvDescriptorHeap=R.SrvHeap.Get(); Init.UserData=&R;
                Init.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE* Cpu,D3D12_GPU_DESCRIPTOR_HANDLE* Gpu) {
                    if (R.FreeDescriptors.empty()) throw std::runtime_error("ImGui SRV heap exhausted");
                    const auto Index=R.FreeDescriptors.back(); R.FreeDescriptors.pop_back();
                    *Cpu=R.SrvHeap->GetCPUDescriptorHandleForHeapStart(); *Gpu=R.SrvHeap->GetGPUDescriptorHandleForHeapStart();
                    Cpu->ptr+=static_cast<SIZE_T>(Index)*R.DescriptorSize; Gpu->ptr+=static_cast<UINT64>(Index)*R.DescriptorSize;
                };
                Init.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE Cpu,D3D12_GPU_DESCRIPTOR_HANDLE) {
                    const auto Base=R.SrvHeap->GetCPUDescriptorHandleForHeapStart().ptr;
                    R.FreeDescriptors.push_back(static_cast<UINT>((Cpu.ptr-Base)/R.DescriptorSize));
                };
                R.Backend=ImGui_ImplDX12_Init(&Init);
            } else R.Backend=ImGui_ImplDX11_Init(R.Device11.Get(),R.Context11.Get());
            if (!R.Backend) throw std::runtime_error("ImGui renderer init");
            Initialized.store(true);
            Logger::Log("[UI] Dear ImGui ready: %s, %u back buffers",R.DX12?"DX12":"DX11",Desc.BufferCount);
            return true;
        }
        void Render(IDXGISwapChain* Swap) {
            if (Stopping || !IsTarget(Swap)) return;
            std::lock_guard<std::mutex> Lock(RenderMutex);
            if (R.Swap && R.Swap.Get()!=Swap) DestroyRenderer();
            if (!R.Context && !CreateRenderer(Swap)) return;
            ImGui::SetCurrentContext(R.Context);
            if (ResetInput.exchange(false)) { ImGui::GetIO().ClearInputKeys(); ImGui::GetIO().ClearInputMouse(); }
            ConsumeInput();
            if (R.DX12) ImGui_ImplDX12_NewFrame(); else ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame(); ImGui::GetIO().MouseDrawCursor=MenuUI::IsOpen();
            ImGui::NewFrame(); OverlayScene::Draw(MenuUI::IsOpen()); MenuUI::Draw(); ImGui::Render();
            if (R.DX12) {
                ComPtr<IDXGISwapChain3> Swap3; Check(Swap->QueryInterface(IID_PPV_ARGS(&Swap3)),"DX12 swapchain");
                auto& Frame=R.Frames[Swap3->GetCurrentBackBufferIndex()]; WaitFence(Frame.FenceValue);
                Check(Frame.Allocator->Reset(),"Reset UI allocator"); Check(R.List->Reset(Frame.Allocator.Get(),nullptr),"Reset UI command list");
                D3D12_RESOURCE_BARRIER Barrier{}; Barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                Barrier.Transition.pResource=Frame.Buffer.Get(); Barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                Barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT; Barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
                R.List->ResourceBarrier(1,&Barrier); R.List->OMSetRenderTargets(1,&Frame.Rtv,FALSE,nullptr);
                ID3D12DescriptorHeap* Heaps[]={R.SrvHeap.Get()}; R.List->SetDescriptorHeaps(1,Heaps);
                ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),R.List.Get());
                std::swap(Barrier.Transition.StateBefore,Barrier.Transition.StateAfter); R.List->ResourceBarrier(1,&Barrier);
                Check(R.List->Close(),"Close UI draw list"); ID3D12CommandList* Lists[]={R.List.Get()};
                Execute(R.Queue.Get(),1,Lists);
                Frame.FenceValue=++R.NextFence; Check(R.Queue->Signal(R.Fence.Get(),Frame.FenceValue),"Signal UI frame");
            } else {
                ComPtr<ID3D11RenderTargetView> Previous; ComPtr<ID3D11DepthStencilView> Depth;
                R.Context11->OMGetRenderTargets(1,&Previous,&Depth);
                auto* Rtv=R.Rtv11.Get(); R.Context11->OMSetRenderTargets(1,&Rtv,nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                auto* Old=Previous.Get(); R.Context11->OMSetRenderTargets(1,&Old,Depth.Get());
            }
            if (++Frames==1) Logger::Log("[UI] Menu rendering active; INSERT is the only feature shortcut");
        }
        void SafeRender(IDXGISwapChain* Swap) {
            try { Render(Swap); }
            catch (const std::exception& Error) {
                static int Errors=0;
                if (++Errors<5) Logger::Log("[UI] Render error: %s",Error.what());
                MenuUI::SetOpen(false);
                try { std::lock_guard<std::mutex> Lock(RenderMutex); DestroyRenderer(); } catch (...) {}
            }
        }
        HRESULT STDMETHODCALLTYPE HookPresent12(IDXGISwapChain* S,UINT I,UINT F) { CallScope Scope; SafeRender(S); return Present12(S,I,F); }
        HRESULT STDMETHODCALLTYPE HookPresent11(IDXGISwapChain* S,UINT I,UINT F) { CallScope Scope; SafeRender(S); return Present11(S,I,F); }
        void BeforeResize(IDXGISwapChain* Swap) {
            if (Stopping || !IsTarget(Swap)) return;
            std::lock_guard<std::mutex> Lock(RenderMutex);
            if (R.Context) { Logger::Log("[UI] Rebuilding renderer after swapchain resize"); DestroyRenderer(); }
        }
        HRESULT STDMETHODCALLTYPE HookResize12(IDXGISwapChain* S,UINT C,UINT W,UINT H,DXGI_FORMAT F,UINT Flags) {
            CallScope Scope; BeforeResize(S); return Resize12(S,C,W,H,F,Flags);
        }
        HRESULT STDMETHODCALLTYPE HookResize11(IDXGISwapChain* S,UINT C,UINT W,UINT H,DXGI_FORMAT F,UINT Flags) {
            CallScope Scope; BeforeResize(S); return Resize11(S,C,W,H,F,Flags);
        }
        void STDMETHODCALLTYPE HookExecute(ID3D12CommandQueue* Queue,UINT Count,ID3D12CommandList* const* Lists) {
            CallScope Scope;
            if (!Stopping && Queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT) {
                std::lock_guard<std::mutex> Lock(QueueMutex); CandidateQueue=Queue;
            }
            Execute(Queue,Count,Lists);
        }
        void AddHook(void* Target,void* Detour,void** Original) {
            if (std::find(HookTargets.begin(),HookTargets.end(),Target)!=HookTargets.end()) return;
            if (MH_CreateHook(Target,Detour,Original)!=MH_OK) throw std::runtime_error("Create graphics hook");
            HookTargets.push_back(Target);
        }
        LRESULT CALLBACK DummyProc(HWND W,UINT M,WPARAM A,LPARAM B) { return DefWindowProcW(W,M,A,B); }
        void DiscoverHooks() {
            HINSTANCE Module=GetModuleHandleW(nullptr);
            WNDCLASSW Class{}; Class.lpfnWndProc=DummyProc; Class.hInstance=Module; Class.lpszClassName=L"RoNImGuiProbe";
            RegisterClassW(&Class);
            HWND Dummy=CreateWindowExW(0,Class.lpszClassName,L"",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,Module,nullptr);
            if (!Dummy) throw std::runtime_error("Graphics probe window");
            try {
                ComPtr<IDXGIFactory4> Factory; Check(CreateDXGIFactory1(IID_PPV_ARGS(&Factory)),"DXGI factory");
                ComPtr<ID3D12Device> Device; Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&Device)),"DX12 probe device");
                D3D12_COMMAND_QUEUE_DESC Q{}; Q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
                ComPtr<ID3D12CommandQueue> Queue; Check(Device->CreateCommandQueue(&Q,IID_PPV_ARGS(&Queue)),"DX12 probe queue");
                DXGI_SWAP_CHAIN_DESC1 Desc{}; Desc.Width=32; Desc.Height=32; Desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
                Desc.SampleDesc.Count=1; Desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; Desc.BufferCount=2; Desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
                ComPtr<IDXGISwapChain1> Swap;
                Check(Factory->CreateSwapChainForHwnd(Queue.Get(),Dummy,&Desc,nullptr,nullptr,&Swap),"DX12 probe swapchain");
                auto** VTable=*reinterpret_cast<void***>(Swap.Get());
                AddHook(VTable[8],reinterpret_cast<void*>(HookPresent12),reinterpret_cast<void**>(&Present12));
                AddHook(VTable[13],reinterpret_cast<void*>(HookResize12),reinterpret_cast<void**>(&Resize12));
                auto** QTable=*reinterpret_cast<void***>(Queue.Get());
                AddHook(QTable[10],reinterpret_cast<void*>(HookExecute),reinterpret_cast<void**>(&Execute));
                Swap.Reset();
                DXGI_SWAP_CHAIN_DESC Desc11{}; Desc11.BufferDesc.Width=32; Desc11.BufferDesc.Height=32;
                Desc11.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; Desc11.SampleDesc.Count=1;
                Desc11.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; Desc11.BufferCount=2; Desc11.OutputWindow=Dummy;
                Desc11.Windowed=TRUE; Desc11.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
                ComPtr<IDXGISwapChain> Swap11; ComPtr<ID3D11Device> Device11;
                if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&Desc11,&Swap11,&Device11,nullptr,nullptr))) {
                    auto** Table=*reinterpret_cast<void***>(Swap11.Get());
                    AddHook(Table[8],reinterpret_cast<void*>(HookPresent11),reinterpret_cast<void**>(&Present11));
                    AddHook(Table[13],reinterpret_cast<void*>(HookResize11),reinterpret_cast<void**>(&Resize11));
                }
            } catch (...) { DestroyWindow(Dummy); UnregisterClassW(Class.lpszClassName,Module); throw; }
            DestroyWindow(Dummy); UnregisterClassW(Class.lpszClassName,Module);
        }
    }

    bool Initialize(HWND Window,InputCallback Callback) {
        GameWindow=Window; GameInput=Callback; Stopping.store(false); Frames.store(0);
        ControlMessage=RegisterWindowMessageW(L"SCC.ReadyOrNot.ImGui.Control.v5");
        if (!IsWindow(Window) || !ControlMessage) return false;
        auto Status=MH_Initialize();
        if (Status!=MH_OK && Status!=MH_ERROR_ALREADY_INITIALIZED) return false;
        try {
            DiscoverHooks();
            for (auto Target:HookTargets) if (MH_EnableHook(Target)!=MH_OK) throw std::runtime_error("Enable graphics hook");
            WindowThread=GetWindowThreadProcessId(Window,nullptr);
            if (WindowThread==GetCurrentThreadId()) InstallSubclass();
            else {
                HMODULE Module=nullptr;
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&InstallProc),&Module);
                InstallHook.store(SetWindowsHookExW(WH_CALLWNDPROC,InstallProc,Module,WindowThread));
                if (!InstallHook) throw std::runtime_error("Game window input hook");
                DWORD_PTR Result=0;
                SendMessageTimeoutW(Window,ControlMessage,0,0,SMTO_ABORTIFHUNG,2000,&Result);
                for (int i=0;i<200 && !Installed;++i) Sleep(10);
            }
            if (!Installed) throw std::runtime_error("Game window subclass");
            MenuUI::SetVisibilityNotifier(VisibilityChanged);
            Logger::Log("[UI] Graphics hooks installed; menu starts open");
            return true;
        } catch (const std::exception& Error) {
            Logger::Log("[UI] Initialization failed: %s",Error.what());
            Shutdown(); return false;
        }
    }
    bool Shutdown() {
        MenuUI::SetOpen(false);
        if (Installed) {
            if (WindowThread==GetCurrentThreadId()) {
                if (GameInput) GameInput(false);
                RemoveWindowSubclass(GameWindow,WindowProc,SubclassId); Installed=false;
            } else {
                PostMessageW(GameWindow,ControlMessage,2,0);
                for (int i=0;i<500 && Installed;++i) Sleep(10);
                if (Installed) { Logger::Log("[UI] Waiting for game input release before unloading"); return false; }
            }
        }
        MenuUI::SetVisibilityNotifier(nullptr);
        if (auto Hook=InstallHook.exchange(nullptr)) UnhookWindowsHookEx(Hook);
        Stopping.store(true);
        for (auto Target:HookTargets) MH_DisableHook(Target);
        while (Calls.load()) Sleep(1);
        { std::lock_guard<std::mutex> Lock(RenderMutex); DestroyRenderer(); }
        for (auto Target:HookTargets) MH_RemoveHook(Target);
        HookTargets.clear(); MH_Uninitialize();
        { std::lock_guard<std::mutex> Lock(QueueMutex); CandidateQueue.Reset(); }
        { std::lock_guard<std::mutex> Lock(InputMutex); Messages.clear(); }
        OverlayScene::Clear();
        Logger::Log("[UI] ImGui, graphics hooks and input callback removed");
        return true;
    }
    bool Ready() { return Initialized.load(); }
    uint64_t RenderedFrames() { return Frames.load(); }
}
