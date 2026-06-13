#include "WebTrack.h"
#include "app/ConfigManager.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>

#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <webview.h>
#include <WebView2.h>
#endif

namespace Qenba {

// Robust URL decoder to perfectly handle any special characters sent from JS
static std::string urlDecode(const std::string& encodedStr) {
    std::string ret;
    for (size_t i = 0; i < encodedStr.length(); i++) {
        if (encodedStr[i] == '%') {
            if (i + 2 < encodedStr.length()) {
                int ii;
                std::stringstream ss;
                ss << std::hex << encodedStr.substr(i + 1, 2);
                ss >> ii;
                ret += static_cast<char>(ii);
                i += 2;
            }
        } else {
            ret += encodedStr[i];
        }
    }
    return ret;
}

WebTrack::WebTrack(void* nativeWindowHandle) {
    m_webview = webview_create(0, nativeWindowHandle);
    if (!m_webview) {
        std::cerr << "[WebTrack] Failed to create WebView2 instance." << std::endl;
        return;
    }

    // Bind state updates: perfectly encoded string to prevent JSON corruption
    webview_bind(m_webview, "onStateUpdate", [](const char* seq, const char* req, void* arg) {
        auto self = static_cast<WebTrack*>(arg);
        if (self && self->m_onStateChanged) {
            std::string s(req);
            // req is ["encodedTitle|encodedUrl|canBack|canForward|encodedIcon"]
            size_t firstQuote = s.find_first_of("\"");
            size_t lastQuote = s.find_last_of("\"");

            if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote) {
                std::string content = s.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                
                std::vector<std::string> parts;
                size_t p = 0;
                while (true) {
                    size_t next = content.find("|", p);
                    if (next == std::string::npos) {
                        parts.push_back(content.substr(p));
                        break;
                    }
                    parts.push_back(content.substr(p, next - p));
                    p = next + 1;
                }

                if (parts.size() >= 5) {
                    std::string title = urlDecode(parts[0]);
                    std::string url = urlDecode(parts[1]);
                    bool canBack = (parts[2] == "true");
                    bool canForward = (parts[3] == "true");
                    std::string favicon = urlDecode(parts[4]);

#ifdef _WIN32
                    auto controller = (ICoreWebView2Controller*)webview_get_native_handle(self->m_webview, WEBVIEW_NATIVE_HANDLE_KIND_BROWSER_CONTROLLER);
                    if (controller) {
                        ICoreWebView2* wv2;
                        if (SUCCEEDED(controller->get_CoreWebView2(&wv2))) {
                            BOOL bCanBack = FALSE;
                            wv2->get_CanGoBack(&bCanBack);
                            BOOL bCanForward = FALSE;
                            wv2->get_CanGoForward(&bCanForward);
                            canBack = (bCanBack == TRUE);
                            canForward = (bCanForward == TRUE);
                            wv2->Release();
                        }
                    }
#endif
                    // Prevent the "went back too far" bug ending up on a data: URL or about:blank
                    if (url.starts_with("data:text") || url == "about:blank") {
                        self->goForward();
                        return; // Prevent further state update propagation right now
                    }

                    self->m_title = title;
                    self->m_url = url;
                    self->m_canBack = canBack;
                    self->m_canForward = canForward;
                    self->m_favicon = favicon;

                    self->m_onStateChanged(title, url, canBack, canForward, favicon);
                }
            }
        }
        webview_return(self->m_webview, seq, 0, "");
    }, this);

    // Bind interaction tracker for focus clearing
    webview_bind(m_webview, "onInteraction", [](const char* seq, const char* req, void* arg) {
        auto self = static_cast<WebTrack*>(arg);
        if (self && self->m_onInteraction) {
            self->m_onInteraction();
        }
        webview_return(self->m_webview, seq, 0, "");
    }, this);

    // Bind config updater for Settings page
    webview_bind(m_webview, "updateConfig", [](const char* seq, const char* req, void* arg) {
        // req is e.g. ["theme","light"] or ["show_home_button",true]
        std::string s(req);
        auto& config = ConfigManager::instance();
        auto appCfg = config.getConfig();

        if (s.find("\"theme\"") != std::string::npos) {
            if (s.find("\"light\"") != std::string::npos) appCfg.theme = "light";
            else if (s.find("\"dark\"") != std::string::npos) appCfg.theme = "dark";
            else if (s.find("\"system\"") != std::string::npos) appCfg.theme = "system";
        } else if (s.find("\"show_home_button\"") != std::string::npos) {
            appCfg.show_home_button = (s.find("true") != std::string::npos);
        } else if (s.find("\"dynamic_wallpaper\"") != std::string::npos) {
            appCfg.dynamic_wallpaper = (s.find("true") != std::string::npos);
        } else if (s.find("\"home_url\"") != std::string::npos) {
            size_t lastComma = s.find_last_of(',');
            if (lastComma != std::string::npos) {
                size_t start = s.find('"', lastComma);
                size_t end = s.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    appCfg.home_url = s.substr(start + 1, end - start - 1);
                }
            }
        } else if (s.find("\"ai_engine\"") != std::string::npos) {
            size_t lastComma = s.find_last_of(',');
            if (lastComma != std::string::npos) {
                size_t start = s.find('"', lastComma);
                size_t end = s.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    appCfg.ai_engine = s.substr(start + 1, end - start - 1);
                }
            }
        }
        
        config.setConfig(appCfg);
        webview_return(static_cast<WebTrack*>(arg)->m_webview, seq, 0, "");
    }, this);

    // Bind submitAddApp for Add App page
    webview_bind(m_webview, "submitAddApp", [](const char* seq, const char* req, void* arg) {
        auto self = static_cast<WebTrack*>(arg);
        if (self && self->m_onSubmitAddApp) {
            std::string s(req);
            // req is ["url"]
            size_t firstQuote = s.find_first_of("\"");
            size_t lastQuote = s.find_last_of("\"");
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote) {
                std::string url = s.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                self->m_onSubmitAddApp(url);
            }
        }
        webview_return(self->m_webview, seq, 0, "");
    }, this);

    // Bind getConfig for frontend to fetch the entire settings state
    webview_bind(m_webview, "getConfig", [](const char* seq, const char* req, void* arg) {
        auto& config = ConfigManager::instance();
        auto appCfg = config.getConfig();

        std::stringstream ss;
        ss << "{"
           << "\"theme\":\"" << appCfg.theme << "\","
           << "\"ai_engine\":\"" << appCfg.ai_engine << "\","
           << "\"home_url\":\"" << appCfg.home_url << "\","
           << "\"show_home_button\":" << (appCfg.show_home_button ? "true" : "false") << ","
           << "\"dynamic_wallpaper\":" << (appCfg.dynamic_wallpaper ? "true" : "false")
           << "}";

        webview_return(static_cast<WebTrack*>(arg)->m_webview, seq, 0, ss.str().c_str());
    }, this);

    // Inject state tracking script
    webview_init(m_webview, 
        "(function() {"
        "  const update = () => {"
        "    let icon = '';"
        "    const link = document.querySelector('link[rel*=\"icon\"]') || "
        "                 document.querySelector('link[rel*=\"apple-touch-icon\"]');"
        "    if (link) icon = link.href;"
        "    else icon = window.location.origin + '/favicon.ico';"
        "    "
        "    let title = document.title || window.location.hostname;"
        "    window.onStateUpdate("
        "      encodeURIComponent(title) + '|' + "
        "      encodeURIComponent(window.location.href) + '|' + "
        "      (window.history.length > 1) + '|' + "
        "      true + '|' + "
        "      encodeURIComponent(icon)"
        "    );"
        "  };"
        "  const origPush = history.pushState;"
        "  history.pushState = function() { origPush.apply(this, arguments); update(); };"
        "  const origReplace = history.replaceState;"
        "  history.replaceState = function() { origReplace.apply(this, arguments); update(); };"
        "  "
        "  window.addEventListener('load', () => setTimeout(update, 200));"
        "  window.addEventListener('popstate', update);"
        "  window.addEventListener('hashchange', update);"
        "  window.addEventListener('mousedown', () => { window.onInteraction(); });"
        "  setInterval(update, 1000);"
        "  update();"
        "})();"
    );

    // Do not navigate immediately here to prevent race conditions with AppController's navigate call.
}

void WebTrack::setOnStateChanged(std::function<void(const std::string&, const std::string&, bool, bool, const std::string&)> cb) {
    m_onStateChanged = cb;
}

void WebTrack::setOnInteraction(std::function<void()> cb) {
    m_onInteraction = cb;
}

void WebTrack::setOnSubmitAddApp(std::function<void(const std::string&)> cb) {
    m_onSubmitAddApp = cb;
}

WebTrack::~WebTrack() {
    if (m_webview) {
        webview_destroy(m_webview);
        m_webview = nullptr;
    }
}

void WebTrack::navigate(const std::string& url) {
    if (m_webview) {
        if (url.starts_with("qenba://")) {
            std::filesystem::path basePath;
#ifdef _WIN32
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
                basePath = std::filesystem::path(exePath).parent_path();
            } else {
                basePath = std::filesystem::current_path();
            }
#else
            basePath = std::filesystem::current_path();
#endif

            std::string page = url.substr(8); // remove qenba://
            // Strip query string — e.g. "add-app?appbar=1" → "add-app"
            auto qmark = page.find('?');
            if (qmark != std::string::npos) page = page.substr(0, qmark);

            auto target = basePath / "config" / (page + ".html");
            
            // Append original query string to the file:// URL so JS can read it
            std::string query = "";
            auto qpos = url.find('?');
            if (qpos != std::string::npos) query = url.substr(qpos); // "?appbar=1"

            std::string fileUrl = "file:///" + target.string();
            for (char& c : fileUrl) if (c == '\\') c = '/';
            fileUrl += query; // append ?appbar=1 etc.
            webview_navigate(m_webview, fileUrl.c_str());
        } else {
            webview_navigate(m_webview, url.c_str());
        }
    }
}

void WebTrack::setBounds(int x, int y, int width, int height) {
    if (m_webview) {
        // webview_set_size only changes Width and Height but forces (0,0) offset inside parent.
        // We bypass this for Phase 1 by manipulating the inner child HWND natively to map exactly to the Slint white rectangle bounds!
        
#ifdef _WIN32
        // Fetch the inner WebView2 child widget HWND
        HWND wvHwnd = (HWND)webview_get_native_handle(m_webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET);
        if (wvHwnd) {
            SetWindowPos(wvHwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        } else {
            // Fallback (might just render at 0,0)
            webview_set_size(m_webview, width, height, WEBVIEW_HINT_NONE);
        }
#else
        webview_set_size(m_webview, width, height, WEBVIEW_HINT_NONE);
#endif
    }
}

void WebTrack::setVisible(bool visible) {
    if (m_webview) {
#ifdef _WIN32
        HWND wvHwnd = (HWND)webview_get_native_handle(m_webview, WEBVIEW_NATIVE_HANDLE_KIND_UI_WIDGET);
        if (wvHwnd) {
            ShowWindow(wvHwnd, visible ? SW_SHOW : SW_HIDE);
        }
#endif
    }
}

void WebTrack::goBack() {
    if (m_webview) {
        webview_eval(m_webview, "window.history.back()");
    }
}

void WebTrack::goForward() {
    if (m_webview) {
        webview_eval(m_webview, "window.history.forward()");
    }
}

void WebTrack::reload() {
    if (m_webview) {
        webview_eval(m_webview, "window.location.reload()");
    }
}

} // namespace Qenba
