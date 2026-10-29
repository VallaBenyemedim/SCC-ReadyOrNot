#pragma once
#include <Windows.h>
#include <cstdio>
#include <mutex>
#include <string>
#include <filesystem>
#include <share.h>

namespace Logger
{
    // Shared state for all translation units
    struct State {
        bool bAllocated = false;
        FILE* LogFile = nullptr;
        std::mutex LogMutex;
    };

    // Meyers Singleton: Guarantees one instance across the DLL
    inline State& GetState() {
        static State s;
        return s;
    }

    inline void Init(bool EnableConsole = true)
    {
        State& s = GetState();
        std::lock_guard<std::mutex> lock(s.LogMutex);

        if (!s.LogFile) {
            HMODULE Module = nullptr;
            wchar_t Path[32768]{};
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&GetState), &Module);
            if (GetModuleFileNameW(Module, Path, 32768)) {
                auto LogPath = std::filesystem::path(Path).replace_extension(L".log");
                s.LogFile = _wfsopen(LogPath.c_str(), L"w", _SH_DENYNO);
            }
        }
        if (EnableConsole && !s.bAllocated && AllocConsole()) {
            FILE* Dummy;
            freopen_s(&Dummy, "CONOUT$", "w", stdout);
            s.bAllocated = true;
            SetConsoleTitleW(L"SCC — Ready or Not");
        }
    }

    inline void Cleanup()
    {
        State& s = GetState();
        std::lock_guard<std::mutex> lock(s.LogMutex);

        if (s.bAllocated)
        {
            FreeConsole();
            s.bAllocated = false;
        }
        if (s.LogFile) {
            fclose(s.LogFile);
            s.LogFile = nullptr;
        }
    }

    // Variadic Log
    template <typename... Args>
    inline void Log(const char* format, Args... args)
    {
        State& s = GetState();
        std::lock_guard<std::mutex> lock(s.LogMutex);
        if (s.bAllocated) {
            printf(format, args...);
            printf("\n");
        }
        if (s.LogFile) {
            fprintf(s.LogFile, format, args...);
            fprintf(s.LogFile, "\n");
            fflush(s.LogFile);
        }
    }

    // String overload
    inline void Log(const std::string& msg)
    {
        Log("%s", msg.c_str());
    }
    
    // Char* overload (ambiguity resolution)
    inline void Log(const char* msg)
    {
         Log("%s", msg);
    }
}
