#include "Platform.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <iostream>
#endif

namespace Qenba {
namespace Platform {

void setClipboardText(const std::string& text) {
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    size_t len = text.length() + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    
    CloseClipboard();
#else
    // Stub for non-Windows platforms.
    // Real cross-platform implementation would go here (e.g., using X11 or Wayland on Linux, NSPasteboard on macOS).
    std::cerr << "Platform::setClipboardText called but not implemented for this OS. Text: " << text << std::endl;
#endif
}

void initDarkTheme() {
#ifdef _WIN32
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUxtheme) {
        using fnSetPreferredAppMode = int(WINAPI*)(int);
        auto SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
        if (SetPreferredAppMode) {
            SetPreferredAppMode(2); // 2 = AllowDark
        }
        
        using fnFlushMenuThemes = void(WINAPI*)();
        auto FlushMenuThemesFn = (fnFlushMenuThemes)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136));
        if (FlushMenuThemesFn) {
            FlushMenuThemesFn();
        }
        
        FreeLibrary(hUxtheme);
    }
#endif
}

} // namespace Platform
} // namespace Qenba
