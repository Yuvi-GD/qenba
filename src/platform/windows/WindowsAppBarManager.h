#pragma once

#ifdef _WIN32
#include <windows.h>

namespace Qenba {
namespace Platform {

struct AppBarState {
    int currentAppBarWidth = 0;
    RECT normalWindowRect = {0};
    DWORD normalWindowStyle = 0;
};

class WindowsAppBarManager {
public:
    static void RegisterAppBar(HWND hwnd, int width, AppBarState& state);
    static void UnregisterAppBar(HWND hwnd, AppBarState& state);
    static void SetAppBarWidth(HWND hwnd, int width, AppBarState& state);
};

} // namespace Platform
} // namespace Qenba
#endif
