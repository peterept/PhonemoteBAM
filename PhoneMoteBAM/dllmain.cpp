#include "pch.h"

#include "BAM.h"
#include "PhonemoteBAM.h"

const int PLUGIN_ID_NUM = 0x0222;

HMODULE hModule;
BOOL(WINAPI* Real_SwapBuffers)(HDC) = SwapBuffers;

std::unique_ptr<PhonemoteBAM> phonemoteBAM(nullptr);

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ::hModule = hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

BOOL WINAPI Routed_SwapBuffers(HDC hDC)
{
    phonemoteBAM->onSwapBuffers(hDC);
    return Real_SwapBuffers(hDC);
}

void calibrateCallback(int) {
    phonemoteBAM->calibrate();
}

extern "C" {

    BAMEXPORT int BAM_load(HMODULE bam_module)
    {
        phonemoteBAM = std::make_unique<PhonemoteBAM>(hModule, bam_module, PLUGIN_ID_NUM);
        phonemoteBAM->createMenu(calibrateCallback);
        return S_OK;
    }

    BAMEXPORT void BAM_unload()
    {
    }

    // called when the table is loaded
    BAMEXPORT void BAM_PluginStart()
    {
        phonemoteBAM->OnPluginStart();
    }

    BAMEXPORT void BAM_PluginStop()
    {
        phonemoteBAM->OnPluginStop();
    }

    BAMEXPORT void BAM_AttachDetours()
    {
        BAM::detours::Attach(&(PVOID&)Real_SwapBuffers, Routed_SwapBuffers);
    }

    BAMEXPORT void BAM_DetachDetours()
    {
        BAM::detours::Detach(&(PVOID&)Real_SwapBuffers, Routed_SwapBuffers);
    }
}