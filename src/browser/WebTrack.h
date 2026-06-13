#pragma once

#include <string>
#include <functional>

// Use the C API declared in api.h — implementation is in webview_core_static.lib
// This avoids the LNK2005 / ODR issue caused by the C++ header-only implementation.
#include <webview/api.h>

namespace Qenba {

class WebTrack {
public:
    explicit WebTrack(void* nativeWindowHandle);
    ~WebTrack();

    void setOnStateChanged(std::function<void(const std::string&, const std::string&, bool, bool, const std::string&)> cb);
    void setOnInteraction(std::function<void()> cb);
    void setOnSubmitAddApp(std::function<void(const std::string&)> cb);
    void navigate(const std::string& url);
    void setBounds(int x, int y, int width, int height);
    void setVisible(bool visible);
    void goBack();
    void goForward();
    void reload();

    // Getters for current state
    std::string getTitle() const { return m_title; }
    std::string getUrl() const { return m_url; }
    bool canGoBack() const { return m_canBack; }
    bool canGoForward() const { return m_canForward; }
    std::string getFavicon() const { return m_favicon; }

private:
    webview_t m_webview = nullptr;
    std::function<void(const std::string&, const std::string&, bool, bool, const std::string&)> m_onStateChanged;
    std::function<void()> m_onInteraction;
    std::function<void(const std::string&)> m_onSubmitAddApp;
    
    std::string m_title;
    std::string m_url;
    bool m_canBack = false;
    bool m_canForward = false;
    std::string m_favicon;
};

} // namespace Qenba
