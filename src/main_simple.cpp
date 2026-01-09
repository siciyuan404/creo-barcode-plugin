/**
 * @file main_simple.cpp
 * @brief Simplified Creo Barcode Plugin entry point for testing
 * 
 * This is a minimal version to test if the DLL loads correctly in Creo.
 */

#include <windows.h>

// Pro/TOOLKIT entry points - must be exported
#ifdef _WIN32
extern "C" __declspec(dllexport) int user_initialize()
#else
extern "C" int user_initialize()
#endif
{
    // Minimal initialization - just return success
    MessageBoxA(NULL, "Creo Barcode Plugin loaded successfully!", "Plugin Info", MB_OK | MB_ICONINFORMATION);
    return 0;
}

#ifdef _WIN32
extern "C" __declspec(dllexport) void user_terminate()
#else
extern "C" void user_terminate()
#endif
{
    // Minimal cleanup - nothing to do
}

// DLL entry point (optional but can help with debugging)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
