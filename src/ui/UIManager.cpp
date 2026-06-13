#include "UIManager.h"
#include "app/ConfigManager.h"
#include <iostream>

#ifdef _WIN32
#include <windowsx.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// Window subclass procedure for native resizing and dragging
static LRESULT CALLBACK QenbaWindowSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_NCHITTEST: {
            LRESULT hit = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            if (hit == HTCLIENT) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &pt);

                RECT rect;
                GetClientRect(hWnd, &rect);
                
                // Sensitivity for resize borders (8 pixels)
                const int border = 8;
                bool left = pt.x < border;
                bool right = pt.x > rect.right - border;
                bool top = pt.y < border;
                bool bottom = pt.y > rect.bottom - border;

                if (top && left) return HTTOPLEFT;
                if (top && right) return HTTOPRIGHT;
                if (bottom && left) return HTBOTTOMLEFT;
                if (bottom && right) return HTBOTTOMRIGHT;
                if (top) return HTTOP;
                if (bottom) return HTBOTTOM;
                if (left) return HTLEFT;
                if (right) return HTRIGHT;

                // Caption area for dragging (top 38px, but exclude the window controls on the right)
                // Window controls are approximately 176px wide.
                if (pt.y < 38 && pt.x < rect.right - 180) {
                    DWORD style = GetWindowLong(hWnd, GWL_STYLE);
                    // In AppBar mode (WS_POPUP without WS_CAPTION), dragging is disabled
                    if ((style & WS_CAPTION) == 0) {
                        return HTCLIENT;
                    }
                    return HTCAPTION;
                }
            }
            return hit;
        }
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) {
                DWORD style = GetWindowLong(hWnd, GWL_STYLE);
                // In AppBar mode (WS_POPUP without WS_CAPTION), eat the entire NC frame
                if ((style & WS_CAPTION) == 0) {
                    return 0;
                }
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            DWORD style = GetWindowLong(hWnd, GWL_STYLE);
            if ((style & WS_CAPTION) == 0) {
                // AppBar mode: enforce minimum width so panel cannot shrink to 0
                MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
                mmi->ptMinTrackSize.x = 440; // minimum 440px wide
                return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            // Recapture OS keyboard focus from WebView2 child HWND.
            // After the user clicks inside the WebView, OS focus moves to the WebView2 HWND.
            // When they click back on the Slint UI (address bar, tabs, etc.),
            // Slint gives the TextInput Slint-level focus, but OS keystrokes still go
            // to the WebView. SetFocus() brings OS keyboard focus back to the main window
            // so the Slint TextInput can actually receive keystrokes.
            SetFocus(hWnd);
            // Fall through to let Slint handle the click normally
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, QenbaWindowSubclass, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
#endif

namespace Qenba {

UIManager::UIManager() : m_appWindow(AppWindow::create()) {
    auto tabs_model = std::make_shared<slint::VectorModel<TabData>>();
    TabData initial;
    initial.title = "New Tab";
    initial.url = "https://duckduckgo.com";
    initial.icon = "B";
    tabs_model->push_back(initial);
    m_appWindow->set_tabs(tabs_model);
}

UIManager::~UIManager() {}

void UIManager::init() {
    auto app = m_appWindow;

    // --- WINDOW CONTROLS (use stored HWND, set via setMainHwnd) ---
    app->on_close_window([this]() {
#ifdef _WIN32
        PostQuitMessage(0);
#endif
        exit(0);
    });

    app->on_minimize_window([this]() {
#ifdef _WIN32
        if (m_hwnd) ShowWindow(m_hwnd, SW_MINIMIZE);
#endif
    });

    app->on_maximize_window([this]() {
#ifdef _WIN32
        if (m_hwnd) {
            WINDOWPLACEMENT wp;
            wp.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(m_hwnd, &wp);
            if (wp.showCmd == SW_SHOWMAXIMIZED) ShowWindow(m_hwnd, SW_RESTORE);
            else ShowWindow(m_hwnd, SW_MAXIMIZE);
        }
#endif
    });

    app->on_window_move([this](float, float) {
        // Now handled by WM_NCHITTEST in subclass for native behavior
    });

    app->on_window_resize([this](float, float) {
        // Handled by native WM_EXITSIZEMOVE or continuous sync
    });

    // --- NAVIGATION ---
    app->on_load_url([this](slint::SharedString url) {
        if (m_urlLoadCallback) m_urlLoadCallback(std::string(url.data()));
    });
    app->on_go_back([this]()    { if (m_backCallback)    m_backCallback(); });
    app->on_go_forward([this]() { if (m_forwardCallback) m_forwardCallback(); });
    app->on_go_home([this]()    { if (m_homeCallback)    m_homeCallback(); });
    app->on_show_profile([this](){ if (m_profileCallback) m_profileCallback(); });
    app->on_refresh([this]() { if (m_refreshCallback) m_refreshCallback(); });
    app->on_sync_geometry([this](float x, float y, float w, float h) {
        if (m_syncGeometryCallback) m_syncGeometryCallback(x, y, w, h);
    });

    // --- SIDEBAR ---
    app->on_sidebar_settings([this]() { if (m_sidebarSettingsCallback) m_sidebarSettingsCallback(); });
    app->on_sidebar_add_current_tab([this]() {
        if (m_sidebarAddCurrentTabCallback) m_sidebarAddCurrentTabCallback();
    });
    app->on_sidebar_add_app_menu([this]() {
        if (m_sidebarAddAppMenuCallback) m_sidebarAddAppMenuCallback();
    });
    app->on_notify_adding_app([this](bool adding) {
        if (m_notifyAddingAppCallback) m_notifyAddingAppCallback(adding);
    });
    app->on_sidebar_submit_add_app([this](slint::SharedString url) {
        if (m_sidebarSubmitAddAppCallback) m_sidebarSubmitAddAppCallback(std::string(url.data()));
    });
    
    app->on_sidebar_navigate([this](slint::SharedString url) {
        m_appWindow->set_sidebar_open(true);
        if (m_sidebarNavigateCallback) m_sidebarNavigateCallback(std::string(url.data()));
    });
    app->on_sidebar_context_menu([this](slint::SharedString url) {
        if (m_sidebarContextMenuCallback) m_sidebarContextMenuCallback(std::string(url.data()));
    });
    app->on_sidebar_back([this]()    { if (m_sidebarBackCallback)    m_sidebarBackCallback(); });
    app->on_sidebar_refresh([this]() { if (m_sidebarRefreshCallback) m_sidebarRefreshCallback(); });
    app->on_sidebar_close([this]() {
        m_appWindow->set_sidebar_open(false);
        if (m_sidebarCloseCallback) m_sidebarCloseCallback(); // destroys WebView HWND
    });
    app->on_sidebar_hide([this]() {
        if (m_sidebarHideCallback) m_sidebarHideCallback(); // hides WebView HWND
    });
    app->on_copy_sidebar_url([this](slint::SharedString url) {
        if (m_copyToClipboardCallback) m_copyToClipboardCallback(std::string(url.data()));
    });
    app->on_toggle_appbar_mode([this]() {
        if (m_toggleAppbarModeCallback) m_toggleAppbarModeCallback();
    });
    app->on_toggle_ai_chat([this]() {
        if (m_toggleAIChatCallback) m_toggleAIChatCallback();
    });
    // sidebar-toggle-overlay removed — pinned/split is always the mode now
    app->on_sync_sidebar_geometry([this](float x, float y, float w, float h) {
        if (m_sidebarSyncGeometryCallback) m_sidebarSyncGeometryCallback(x, y, w, h);
    });
    app->on_sync_appbar_width([this](float width) {
        if (m_syncAppbarWidthCallback) m_syncAppbarWidthCallback(width);
    });

    // --- TABS ---
    app->on_tab_clicked([this](int index) {
        m_appWindow->set_active_tab(index);
        if (m_tabClickedCallback) m_tabClickedCallback(index);
    });

    app->on_new_tab([this]() {
        auto current = m_appWindow->get_tabs();
        auto model = std::make_shared<slint::VectorModel<TabData>>();
        for (int i = 0; i < current->row_count(); ++i)
            model->push_back(*current->row_data(i));
        TabData nt; nt.title = "New Tab"; nt.url = "https://duckduckgo.com"; nt.icon = "N";
        model->push_back(nt);
        m_appWindow->set_tabs(model);
        m_appWindow->set_active_tab(model->row_count() - 1);
        m_appWindow->set_url(slint::SharedString(""));
        if (m_newTabCallback) m_newTabCallback();
    });

    app->on_close_tab([this](int index) {
        auto current = m_appWindow->get_tabs();
        if (current->row_count() <= 1) return;
        auto model = std::make_shared<slint::VectorModel<TabData>>();
        for (int i = 0; i < current->row_count(); ++i)
            if (i != index) model->push_back(*current->row_data(i));
        int active = m_appWindow->get_active_tab();
        if (active >= index && active > 0) m_appWindow->set_active_tab(active - 1);
        m_appWindow->set_tabs(model);
        if (m_closeTabCallback) m_closeTabCallback(index);
    });
}

void UIManager::show() { m_appWindow->show(); }
void UIManager::run()  { slint::run_event_loop(); }

void UIManager::setMainHwnd(HWND hwnd) { 
    m_hwnd = hwnd; 
#ifdef _WIN32
    if (m_hwnd) {
        SetWindowSubclass(m_hwnd, QenbaWindowSubclass, 0, 0);
    }
#endif
}

void UIManager::setUrlLoadCallback(std::function<void(const std::string&)> cb)            { m_urlLoadCallback = cb; }
void UIManager::setSyncGeometryCallback(std::function<void(float,float,float,float)> cb)  { m_syncGeometryCallback = cb; }
void UIManager::setBackCallback(std::function<void()> cb)      { m_backCallback = cb; }
void UIManager::setForwardCallback(std::function<void()> cb)   { m_forwardCallback = cb; }
void UIManager::setHomeCallback(std::function<void()> cb)      { m_homeCallback = cb; }
void UIManager::setProfileCallback(std::function<void()> cb)   { m_profileCallback = cb; }
void UIManager::setTabClickedCallback(std::function<void(int)> cb) { m_tabClickedCallback = cb; }
void UIManager::setCloseTabCallback(std::function<void(int)> cb){ m_closeTabCallback = cb; }
void UIManager::setRefreshCallback(std::function<void()> cb)   { m_refreshCallback = cb; }
void UIManager::setNewTabCallback(std::function<void()> cb)    { m_newTabCallback = cb; }

void UIManager::setSidebarSyncGeometryCallback(std::function<void(float, float, float, float)> cb) { m_sidebarSyncGeometryCallback = std::move(cb); }
void UIManager::setSyncAppbarWidthCallback(std::function<void(float)> cb) { m_syncAppbarWidthCallback = std::move(cb); }
void UIManager::setSidebarSettingsCallback(std::function<void()> cb) { m_sidebarSettingsCallback = std::move(cb); }
void UIManager::setSidebarAddCurrentTabCallback(std::function<void()> cb) { m_sidebarAddCurrentTabCallback = std::move(cb); }
void UIManager::setSidebarAddAppMenuCallback(std::function<void()> cb) { m_sidebarAddAppMenuCallback = std::move(cb); }
void UIManager::setNotifyAddingAppCallback(std::function<void(bool)> cb) { m_notifyAddingAppCallback = std::move(cb); }
void UIManager::setSidebarSubmitAddAppCallback(std::function<void(const std::string&)> cb) { m_sidebarSubmitAddAppCallback = cb; }
void UIManager::setSidebarNavigateCallback(std::function<void(const std::string&)> cb)            { m_sidebarNavigateCallback = cb; }
void UIManager::setSidebarContextMenuCallback(std::function<void(const std::string&)> cb)         { m_sidebarContextMenuCallback = cb; }
void UIManager::setSidebarBackCallback(std::function<void()> cb)                                  { m_sidebarBackCallback = cb; }
void UIManager::setSidebarRefreshCallback(std::function<void()> cb)                               { m_sidebarRefreshCallback = cb; }
void UIManager::setSidebarCloseCallback(std::function<void()> cb)                                 { m_sidebarCloseCallback = cb; }
void UIManager::setSidebarHideCallback(std::function<void()> cb)                                  { m_sidebarHideCallback = cb; }
void UIManager::setToggleAppbarModeCallback(std::function<void()> cb)                             { m_toggleAppbarModeCallback = cb; }
void UIManager::setCopyToClipboardCallback(std::function<void(const std::string&)> cb)            { m_copyToClipboardCallback = cb; }
void UIManager::setToggleAIChatCallback(std::function<void()> cb)                                 { m_toggleAIChatCallback = cb; }

void UIManager::updateActiveTabState(int tabIndex, const std::string& title,
    const std::string& url, bool canBack, bool canForward,
    const slint::Image& faviconImage)
{
    auto app = m_appWindow;
    slint::invoke_from_event_loop([app, tabIndex, title, url, canBack, canForward, faviconImage]() {
        auto current = app->get_tabs();
        if (tabIndex < 0 || tabIndex >= current->row_count()) return;

        auto tab = *current->row_data(tabIndex);
        
        if (!title.empty()) tab.title = title.c_str();
        tab.url = url.c_str();
        
        // Update favicon only if provided. This guarantees the icon NEVER disappears or flickers.
        if (faviconImage.size().width > 0) {
            tab.favicon = faviconImage;
        }

        // Derive icon letter from domain (fallback when no favicon EVER existed)
        std::string d = url;
        auto ps = d.find("://");
        if (ps != std::string::npos) d = d.substr(ps + 3);
        if (d.size() >= 4 && d.substr(0,4) == "www.") d = d.substr(4);
        if (!d.empty() && std::isalnum((unsigned char)d[0]))
            tab.icon = std::string(1, (char)std::toupper((unsigned char)d[0])).c_str();
        else
            tab.icon = "B";

        auto model = std::make_shared<slint::VectorModel<TabData>>();
        for (int i = 0; i < current->row_count(); ++i)
            model->push_back(i == tabIndex ? tab : *current->row_data(i));
        app->set_tabs(model);

        // Build clean display URL for the address bar
        std::string displayUrl = url;
        if (displayUrl.find("config/home.html") != std::string::npos) {
            displayUrl = ""; // Home page shows blank address bar
        } else if (displayUrl.find("config/settings.html") != std::string::npos) {
            displayUrl = "qenba://settings";
        } else if (displayUrl.find("config/add-app") != std::string::npos ||
                   displayUrl.find("add-app.html") != std::string::npos) {
            displayUrl = "";
        } else if (displayUrl.starts_with("file:///")) {
            displayUrl = "";
        }

        // Sync main address bar if this is the active tab
        if (tabIndex == app->get_active_tab()) {
            if (!app->get_addr_focused()) {
                // Only update if value actually changed — avoid resetting TextInput state
                std::string currentUrl(app->get_url().data());
                if (currentUrl != displayUrl) {
                    std::cerr << "[URL] Setting addr bar: \"" << displayUrl << "\" (was: \"" << currentUrl << "\", editing=" << app->get_addr_focused() << ")" << std::endl;
                    app->set_url(slint::SharedString(displayUrl.c_str()));
                }
            }
            app->set_is_secure(url.starts_with("https://") || url.starts_with("qenba://"));
            app->set_can_go_back(canBack);
            app->set_can_go_forward(canForward);
        }
    });
}

void UIManager::updateSidebarFavicon(const slint::Image& img) {
    auto app = m_appWindow;
    slint::invoke_from_event_loop([app, img]() {
        app->set_sidebar_favicon(img);
    });
}

} // namespace Qenba
