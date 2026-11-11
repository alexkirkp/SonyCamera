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
        // lpReserved is non-null when the process is exiting. The worker threads have already been
        // killed by then, so waiting for them would hang; the OS reclaims everything anyway.
        if (lpReserved == nullptr)
        {
            // FreeLibrary: tear down only if something was ever initialised
            Shutdown();
        }
        break;
    }

    return TRUE;
}
