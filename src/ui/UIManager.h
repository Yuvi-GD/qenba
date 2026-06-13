#pragma once

#include <string>
#include <functional>
#include <memory>
#include "app.h" // the Slint generated header, contains AppWindow and slint::Image
#include <windows.h>

namespace Qenba {

class UIManager {
public:
    UIManager();
    ~UIManager();

    void init();
    void show();
    void run();
    void setMainHwnd(HWND hwnd);

    // Callbacks for browser navigation
    void setUrlLoadCallback(std::function<void(const std::string&)> cb);
    void setSyncGeometryCallback(std::function<void(float, float, float, float)> cb);
    void setBackCallback(std::function<void()> cb);
    void setForwardCallback(std::function<void()> cb);
    void setHomeCallback(std::function<void()> cb);
    void setProfileCallback(std::function<void()> cb);
    void setRefreshCallback(std::function<void()> cb);
    
    // Callbacks for tabs
    void setTabClickedCallback(std::function<void(int)> cb);
    void setCloseTabCallback(std::function<void(int)> cb);
    void setNewTabCallback(std::function<void()> cb);

    // Callbacks for sidebar
    void setSidebarSyncGeometryCallback(std::function<void(float, float, float, float)> cb);
    void setSyncAppbarWidthCallback(std::function<void(float)> cb);
    void setSidebarSettingsCallback(std::function<void()> cb);
    void setSidebarAddCurrentTabCallback(std::function<void()> cb);
    void setSidebarAddAppMenuCallback(std::function<void()> cb);
    void setNotifyAddingAppCallback(std::function<void(bool)> cb);
    void setSidebarSubmitAddAppCallback(std::function<void(const std::string&)> cb);
    void setSidebarNavigateCallback(std::function<void(const std::string&)> cb);
    void setSidebarContextMenuCallback(std::function<void(const std::string&)> cb);
    void setSidebarBackCallback(std::function<void()> cb);
    void setSidebarRefreshCallback(std::function<void()> cb);
    void setSidebarCloseCallback(std::function<void()> cb);
    void setSidebarHideCallback(std::function<void()> cb);
    void setToggleAppbarModeCallback(std::function<void()> cb);
    void setCopyToClipboardCallback(std::function<void(const std::string&)> cb);
    void setToggleAIChatCallback(std::function<void()> cb);

    // UI Updates
    void updateActiveTabState(int tabIndex, const std::string& title,
                              const std::string& url, bool canBack, bool canForward,
                              const slint::Image& faviconImage);
    void updateSidebarFavicon(const slint::Image& img);

    // Getters
    slint::ComponentHandle<AppWindow>& getAppWindow() { return m_appWindow; }

private:
    slint::ComponentHandle<AppWindow> m_appWindow;
    HWND m_hwnd = nullptr;

    // Callbacks
    std::function<void(const std::string&)> m_urlLoadCallback;
    std::function<void(float, float, float, float)> m_syncGeometryCallback;
    std::function<void()> m_backCallback;
    std::function<void()> m_forwardCallback;
    std::function<void()> m_homeCallback;
    std::function<void()> m_profileCallback;
    std::function<void()> m_refreshCallback;
    std::function<void(int)> m_tabClickedCallback;
    std::function<void(int)> m_closeTabCallback;
    std::function<void()> m_newTabCallback;

    std::function<void(float, float, float, float)> m_sidebarSyncGeometryCallback;
    std::function<void(float)> m_syncAppbarWidthCallback;
    std::function<void()> m_sidebarSettingsCallback;
    std::function<void()> m_sidebarAddCurrentTabCallback;
    std::function<void()> m_sidebarAddAppMenuCallback;
    std::function<void(bool)> m_notifyAddingAppCallback;
    std::function<void(const std::string&)> m_sidebarSubmitAddAppCallback;
    std::function<void(const std::string&)> m_sidebarNavigateCallback;
    std::function<void(const std::string&)> m_sidebarContextMenuCallback;
    std::function<void()> m_sidebarBackCallback;
    std::function<void()> m_sidebarRefreshCallback;
    std::function<void()> m_sidebarCloseCallback;
    std::function<void()> m_sidebarHideCallback;
    std::function<void()> m_toggleAppbarModeCallback;
    std::function<void(const std::string&)> m_copyToClipboardCallback;
    std::function<void()> m_toggleAIChatCallback;
};

} // namespace Qenba
