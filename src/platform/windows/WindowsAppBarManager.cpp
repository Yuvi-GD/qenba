#ifdef _WIN32
#include "WindowsAppBarManager.h"
#include <shellapi.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace Qenba {
namespace Platform {

void WindowsAppBarManager::RegisterAppBar(HWND hwnd, int width, AppBarState& state) {
    // Save normal window state
    GetWindowRect(hwnd, &state.normalWindowRect);
    state.normalWindowStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

    // Un-decorate the window and make it a ToolWindow so it acts like a native sidebar
    // WS_EX_TOOLWINDOW removes it from Alt+Tab and taskbar.
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_THICKFRAME);
    DWORD exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, (exStyle & ~WS_EX_APPWINDOW) | WS_EX_TOOLWINDOW);

    // Flush style change so WM_NCCALCSIZE fires before any paint
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

    // Tell DWM to stop rendering the NC frame entirely — eliminates the white-border flash
    // at compositor level, before any ShowWindow causes a repaint.
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));
    MARGINS margins = {0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Force Windows to update the taskbar/alt-tab visibility
    ShowWindow(hwnd, SW_HIDE);
    ShowWindow(hwnd, SW_SHOW);

    // Register AppBar
    APPBARDATA abd = {0};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hwnd;
    abd.uCallbackMessage = WM_USER + 101;
    SHAppBarMessage(ABM_NEW, &abd);

    state.currentAppBarWidth = width;

    // Apply initial position
    UINT dpi = GetDpiForWindow(hwnd);
    int physicalWidth = width;
    if (dpi != 0) physicalWidth = (width * dpi) / 96;

    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(hMon, &mi);

    abd.uEdge = ABE_RIGHT;
    abd.rc = mi.rcMonitor;
    abd.rc.left = abd.rc.right - physicalWidth;

    SHAppBarMessage(ABM_QUERYPOS, &abd);
    abd.rc.left = abd.rc.right - physicalWidth;
    SHAppBarMessage(ABM_SETPOS, &abd);

    SetWindowPos(hwnd, HWND_TOPMOST, abd.rc.left, abd.rc.top,
                 abd.rc.right - abd.rc.left, abd.rc.bottom - abd.rc.top, 
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

void WindowsAppBarManager::UnregisterAppBar(HWND hwnd, AppBarState& state) {
    // Unregister AppBar
    APPBARDATA abd = {0};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = hwnd;
    SHAppBarMessage(ABM_REMOVE, &abd);

    // Restore DWM NC rendering first (before restoring styles)
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_USEWINDOWSTYLE;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));

    // Restore normal window state
    DWORD exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, (exStyle & ~WS_EX_TOOLWINDOW) | WS_EX_APPWINDOW);
    SetWindowLongPtr(hwnd, GWL_STYLE, state.normalWindowStyle);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

    ShowWindow(hwnd, SW_HIDE);
    ShowWindow(hwnd, SW_SHOW);

    SetWindowPos(hwnd, HWND_NOTOPMOST,
                 state.normalWindowRect.left, state.normalWindowRect.top,
                 state.normalWindowRect.right - state.normalWindowRect.left,
                 state.normalWindowRect.bottom - state.normalWindowRect.top,
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

void WindowsAppBarManager::SetAppBarWidth(HWND hwnd, int width, AppBarState& state) {
    UINT dpi = GetDpiForWindow(hwnd);
    int physicalWidth = width;
    if (dpi != 0) physicalWidth = (width * dpi) / 96;

    if (state.currentAppBarWidth != physicalWidth) {
        state.currentAppBarWidth = physicalWidth;

        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hMon, &mi);

        APPBARDATA abd = {0};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = hwnd;
        abd.uEdge = ABE_RIGHT;
        abd.rc = mi.rcMonitor;
        abd.rc.left = abd.rc.right - physicalWidth;

        SHAppBarMessage(ABM_QUERYPOS, &abd);
        abd.rc.left = abd.rc.right - physicalWidth;
        SHAppBarMessage(ABM_SETPOS, &abd);

        RECT currentRect;
        GetWindowRect(hwnd, &currentRect);
        if (currentRect.right - currentRect.left != physicalWidth) {
            SetWindowPos(hwnd, HWND_TOPMOST, abd.rc.left, abd.rc.top,
                         abd.rc.right - abd.rc.left, abd.rc.bottom - abd.rc.top, 
                         SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

} // namespace Platform
} // namespace Qenba
#endif
