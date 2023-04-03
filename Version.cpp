#include "pch.h"

#include "Version.h"

Version::Version(HMODULE hModule) {
    HRSRC res = FindResource(hModule, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
    DWORD dwSize = ::SizeofResource(hModule, res);
    HGLOBAL resData = LoadResource(hModule, res);
    LPVOID lpData = LockResource(resData);
    lpResCopy = (LPVOID)LocalAlloc(LMEM_FIXED, dwSize);
    CopyMemory(lpResCopy, lpData, dwSize);
}

Version::~Version() {
    if (lpResCopy) {
        LocalFree(lpResCopy);
    }
}

std::string Version::productVersionString() {
    if (lpResCopy) {
        VS_FIXEDFILEINFO *lpFfi;
        UINT size;
        if (VerQueryValue(lpResCopy, L"\\", (LPVOID *)&lpFfi, &size)) {
            char str[10 * 4];
            snprintf(str, sizeof(str), "%d.%d.%d.%d", HIWORD(lpFfi->dwProductVersionMS),
                     LOWORD(lpFfi->dwProductVersionMS), HIWORD(lpFfi->dwProductVersionLS),
                     LOWORD(lpFfi->dwProductVersionLS));
            return std::string(str);
        }
    }

    return std::string();
}
