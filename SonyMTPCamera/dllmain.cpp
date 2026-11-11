// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "Logger.h"
#include "Registry.h"
#include "dll.h"

HINSTANCE dllInstance;
Logger logger;
Registry registry(L"Software\\Retro.kiwi\\SonyMTPCamera.dll");

// DllMain runs under the loader lock, so it must do nothing that can wait on another thread, load
// a library or touch the registry. All real set-up happens in Init() on first use of the API.
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        dllInstance = hModule;
        break;

    case DLL_PROCESS_DETACH:
        // Deliberately no cleanup. On process exit (lpReserved non-null) the worker threads are
        // already dead and the OS reclaims everything. On FreeLibrary, tearing down would join the
        // worker threads, which cannot exit while we hold the loader lock (deadlock). Hosts should
        // call the exported ShutdownLibrary() before FreeLibrary.
        (void)lpReserved;
        break;
    }

    return TRUE;
}
