#include "AppController.h"
#include "utils/FaviconUtils.h"
#include "ConfigManager.h"
#include <iostream>

namespace Qenba {

AppController::AppController(UIManager& ui) : m_ui(ui) {
    m_state = std::make_shared<AppState>();
}

AppController::~AppController() {}

void AppController::init() {
    setupCallbacks();
    
    // Add interaction listener to clear Address Bar focus when clicking inside the WebView
    for (auto& track : m_state->webTracks) {
        track->setOnInteraction([this]() {
            m_ui.getAppWindow()->invoke_clear_addr_focus();
        });
    }

    // Sync Slint UI with loaded configuration
    auto& config = ConfigManager::instance().getConfig();
    auto& appWin = m_ui.getAppWindow();
    appWin->set_show_home_button(config.show_home_button);
    appWin->set_config_theme(config.theme.c_str());
}

void AppController::setParentHwnd(HWND hwnd) {
    m_state->parentHwnd = hwnd;
    m_ui.setMainHwnd(hwnd);
    m_ui.getAppWindow()->set_initialized(true);

    auto track = createTrack(hwnd);
    m_state->webTracks.push_back(track);
    track->setVisible(true);
    track->setBounds((int)m_state->lastX, (int)m_state->lastY,
                     (int)m_state->lastW, (int)m_state->lastH);
    
    auto& config = ConfigManager::instance().getConfig();
    track->navigate(config.home_url);
}

void AppController::updateUrlBar() {
    if (m_state->activeIndex >= 0 && m_state->activeIndex < (int)m_state->webTracks.size()) {
        auto url = m_state->webTracks[m_state->activeIndex]->getUrl();
        if (!m_ui.getAppWindow()->get_addr_focused())
            m_ui.getAppWindow()->set_url(url.c_str());
    }
}

std::shared_ptr<WebTrack> AppController::createTrack(HWND parent) {
    auto track = std::make_shared<WebTrack>(parent);
    auto raw = track.get();

    track->setOnStateChanged([this, raw](
        const std::string& title, const std::string& url,
        bool canBack, bool canForward, const std::string& faviconUrl)
    {
        int idx = -1;
        for (int i = 0; i < (int)m_state->webTracks.size(); ++i)
            if (m_state->webTracks[i].get() == raw) { idx = i; break; }
        if (idx < 0) return;

        // Update the tab state in UI
        slint::Image emptyImg;
        m_ui.updateActiveTabState(idx, title, url, canBack, canForward, emptyImg);

        // Sync the main Address Bar if this is the active tab
        // MOVED TO UIManager::updateActiveTabState for thread safety

        // Favicon handling
        std::string faviconServiceUrl;

        if (!faviconUrl.empty() && faviconUrl.starts_with("http")) {
            // Frontend explicitly gave us a favicon path (like from config/home.html)
            faviconServiceUrl = faviconUrl;
        } else {
            // Fallback to domain-based heuristic
            std::string domain = url;
            auto ps = domain.find("://");
            if (ps != std::string::npos) domain = domain.substr(ps + 3);
            auto sl = domain.find('/');
            if (sl != std::string::npos) domain = domain.substr(0, sl);
            
            if (!domain.empty() && domain.find("config/") == std::string::npos) {
                faviconServiceUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
            }
        }

        if (faviconServiceUrl.empty()) return;

        downloadFaviconAsync(faviconServiceUrl, [this, idx](const std::wstring& path) {
            auto img = loadFaviconImage(path);
            if (img.size().width == 0) return;
            
            auto& appWin = m_ui.getAppWindow();
            auto current = appWin->get_tabs();
            if (idx >= current->row_count()) return;
            
            auto tab = *current->row_data(idx);
            tab.favicon = img;
            
            auto model = std::make_shared<slint::VectorModel<TabData>>();
            for (int i = 0; i < current->row_count(); ++i)
                model->push_back(i == idx ? tab : *current->row_data(i));
            appWin->set_tabs(model);
        });
    });

    /* track->setOnInteraction([this]() {
        slint::invoke_from_event_loop([this]() {
            m_ui.getAppWindow()->invoke_clear_addr_focus();
        });
    }); */

    return track;
}

std::shared_ptr<WebTrack> AppController::getSidebarTrack(HWND parent, const std::string& url) {
    auto it = m_state->sidebarTracks.find(url);
    if (it != m_state->sidebarTracks.end()) return it->second;

    auto track = std::make_shared<WebTrack>(parent);
    std::string trackUrl = url;

    track->setOnStateChanged([this, trackUrl](
        const std::string& title, const std::string& pageUrl, bool, bool, const std::string&)
    {
        std::string displayUrl = pageUrl;
        if (displayUrl.find("config/home.html") != std::string::npos) displayUrl = "qenba://home";
        else if (displayUrl.find("config/settings.html") != std::string::npos) displayUrl = "qenba://settings";

        std::string domain = pageUrl;
        if (pageUrl.find("config/") != std::string::npos) {
            domain = "Internal Page";
        } else {
            auto ps = domain.find("://");
            if (ps != std::string::npos) domain = domain.substr(ps + 3);
            auto sl2 = domain.find('/');
            if (sl2 != std::string::npos) domain = domain.substr(0, sl2);
        }

        slint::invoke_from_event_loop([this, trackUrl, title, displayUrl]() {
            if (m_state->currentSidebarUrl == trackUrl) {
                if (!title.empty())
                    m_ui.getAppWindow()->set_sidebar_title(slint::SharedString(title.c_str()));
                m_ui.getAppWindow()->set_sidebar_url(slint::SharedString(displayUrl.c_str()));
            }
        });

        if (!domain.empty()) {
            std::string svcUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
            downloadFaviconAsync(svcUrl, [this, trackUrl](const std::wstring& path) {
                auto img = loadFaviconImage(path);
                if (img.size().width == 0) return;

                if (m_state->currentSidebarUrl == trackUrl) {
                    m_ui.getAppWindow()->set_sidebar_favicon(img);
                }

                auto& appWin = m_ui.getAppWindow();
                auto model = appWin->get_pinned_apps();
                auto nm = std::make_shared<slint::VectorModel<PinnedApp>>();
                bool found = false;
                for (int i = 0; i < model->row_count(); ++i) {
                    auto item = *model->row_data(i);
                    if (std::string(item.url.data()) == trackUrl) {
                        item.favicon = img;
                        found = true;
                    }
                    nm->push_back(item);
                }
                if (found) appWin->set_pinned_apps(nm);
            });
        }
    });

    track->setVisible(false);
    track->navigate(url);
    m_state->sidebarTracks[url] = track;
    return track;
}

void AppController::setupCallbacks() {
    m_ui.setTabClickedCallback([this](int index) {
        if (index < 0 || index >= (int)m_state->webTracks.size()) return;
        m_state->webTracks[m_state->activeIndex]->setVisible(false);
        m_state->activeIndex = index;
        auto& t = m_state->webTracks[index];
        t->setVisible(true);
        t->setBounds((int)m_state->lastX, (int)m_state->lastY,
                     (int)m_state->lastW, (int)m_state->lastH);
        
        // Ensure UI state is fully synced when switching tabs
        slint::Image emptyImg;
        m_ui.updateActiveTabState(index, t->getTitle(), t->getUrl(),
                                t->canGoBack(), t->canGoForward(), emptyImg);
        // Force URL bar update and clear focus to ensure sync
        m_ui.getAppWindow()->set_url(t->getUrl().c_str());
        m_ui.getAppWindow()->invoke_clear_addr_focus();
    });

    m_ui.setNewTabCallback([this]() {
        if (!m_state->webTracks.empty())
            m_state->webTracks[m_state->activeIndex]->setVisible(false);
        auto track = createTrack(m_state->parentHwnd);
        m_state->webTracks.push_back(track);
        m_state->activeIndex = (int)m_state->webTracks.size() - 1;
        track->setVisible(true);
        track->setBounds((int)m_state->lastX, (int)m_state->lastY,
                         (int)m_state->lastW, (int)m_state->lastH);
        
        // Open the configured home page on completely new tabs!
        auto& config = ConfigManager::instance().getConfig();
        track->navigate(config.home_url);
    });

    m_ui.setCloseTabCallback([this](int index) {
        if (index < 0 || index >= (int)m_state->webTracks.size() ||
            m_state->webTracks.size() <= 1) return;
        m_state->webTracks[index]->setVisible(false);
        m_state->webTracks.erase(m_state->webTracks.begin() + index);
        m_state->activeIndex = m_ui.getAppWindow()->get_active_tab();
        if (m_state->activeIndex >= (int)m_state->webTracks.size())
            m_state->activeIndex = (int)m_state->webTracks.size() - 1;
        auto& t = m_state->webTracks[m_state->activeIndex];
        t->setVisible(true);
        t->setBounds((int)m_state->lastX, (int)m_state->lastY,
                     (int)m_state->lastW, (int)m_state->lastH);
        slint::Image emptyImg;
        m_ui.updateActiveTabState(m_state->activeIndex, t->getTitle(), t->getUrl(),
                                t->canGoBack(), t->canGoForward(), emptyImg);
        m_ui.getAppWindow()->set_url(t->getUrl().c_str());
    });

    m_ui.setUrlLoadCallback([this](const std::string& url) {
        if (m_state->activeIndex < (int)m_state->webTracks.size()) {
            std::string targetUrl = url;
            // If it contains no spaces and has a dot, treat as URL. Otherwise search.
            if (targetUrl.find("://") == std::string::npos) {
                bool hasDot = targetUrl.find('.') != std::string::npos;
                bool hasSpace = targetUrl.find(' ') != std::string::npos;
                if (hasDot && !hasSpace) {
                    targetUrl = "https://" + targetUrl;
                } else {
                    // Smart search via DuckDuckGo
                    targetUrl = "https://duckduckgo.com/?q=" + url;
                }
            }
            m_state->webTracks[m_state->activeIndex]->navigate(targetUrl);
        }
    });

    m_ui.setBackCallback([this]() {
        if (m_state->activeIndex < (int)m_state->webTracks.size())
            m_state->webTracks[m_state->activeIndex]->goBack();
    });

    m_ui.setForwardCallback([this]() {
        if (m_state->activeIndex < (int)m_state->webTracks.size())
            m_state->webTracks[m_state->activeIndex]->goForward();
    });

    m_ui.setRefreshCallback([this]() {
        if (m_state->activeIndex < (int)m_state->webTracks.size())
            m_state->webTracks[m_state->activeIndex]->reload();
    });

    m_ui.setHomeCallback([this]() {
        if (m_state->activeIndex < (int)m_state->webTracks.size())
            m_state->webTracks[m_state->activeIndex]->navigate("qenba://home");
    });

    m_ui.setProfileCallback([this]() {
        if (m_state->activeIndex < (int)m_state->webTracks.size())
            m_state->webTracks[m_state->activeIndex]->navigate("qenba://settings");
    });

    m_ui.setSidebarPinCurrentCallback([this]() {
        if (m_state->activeIndex >= (int)m_state->webTracks.size()) return;
        auto& activeTrack = m_state->webTracks[m_state->activeIndex];
        auto url = activeTrack->getUrl();
        if (url.empty()) return;

        auto& appWin = m_ui.getAppWindow();
        slint::SharedString title = activeTrack->getTitle().c_str();
        slint::Image favicon;
        
        auto tabs = appWin->get_tabs();
        if (m_state->activeIndex < tabs->row_count()) {
            favicon = tabs->row_data(m_state->activeIndex)->favicon;
        }

        std::string d = url;
        auto ps = d.find("://"); if (ps != std::string::npos) d = d.substr(ps + 3);
        if (d.size() >= 4 && d.substr(0,4) == "www.") d = d.substr(4);
        auto sl = d.find('/'); if (sl != std::string::npos) d = d.substr(0, sl);

        std::string iconLetter = "W";
        if (!d.empty() && std::isalnum((unsigned char)d[0]))
            iconLetter = std::string(1, (char)std::toupper((unsigned char)d[0]));

        auto model = appWin->get_pinned_apps();
        auto nm = std::make_shared<slint::VectorModel<PinnedApp>>();
        bool alreadyPinned = false;
        for (int i = 0; i < model->row_count(); ++i) {
            auto item = *model->row_data(i);
            if (std::string(item.url.data()) == url) { alreadyPinned = true; break; }
            nm->push_back(item);
        }
        if (alreadyPinned) return;

        PinnedApp pa; 
        pa.title = title; pa.url = url.c_str(); pa.icon = iconLetter.c_str(); pa.favicon = favicon;
        nm->push_back(pa);
        appWin->set_pinned_apps(nm);
    });

    m_ui.setSidebarRemoveAppCallback([this](const std::string& url) {
        auto& appWin = m_ui.getAppWindow();
        auto model = appWin->get_pinned_apps();
        auto nm = std::make_shared<slint::VectorModel<PinnedApp>>();
        
        for (int i = 0; i < model->row_count(); ++i) {
            auto item = *model->row_data(i);
            if (std::string(item.url.data()) != url) {
                nm->push_back(item);
            }
        }
        appWin->set_pinned_apps(nm);
        
        // If we closed the active panel app, close the panel
        if (m_state->currentSidebarUrl == url) {
            m_ui.getAppWindow()->invoke_sidebar_close();
        }
    });

    m_ui.setSidebarNavigateCallback([this](const std::string& url) {
        if (url.empty() || !m_state->parentHwnd) return;

        if (m_state->activeSidebarTrack && m_state->currentSidebarUrl != url)
            m_state->activeSidebarTrack->setVisible(false);

        m_state->currentSidebarUrl = url;
        auto track = getSidebarTrack(m_state->parentHwnd, url);
        track->setVisible(true);
        m_state->activeSidebarTrack = track;

        std::string domain = url;
        auto ps = domain.find("://");
        if (ps != std::string::npos) domain = domain.substr(ps + 3);
        auto sl = domain.find('/');
        if (sl != std::string::npos) domain = domain.substr(0, sl);

        auto& appWin = m_ui.getAppWindow();
        auto cachedTitle = track->getTitle();
        appWin->set_sidebar_title(slint::SharedString(cachedTitle.empty() ? domain.c_str() : cachedTitle.c_str()));
        appWin->set_sidebar_url(slint::SharedString(domain.c_str()));
        appWin->set_sidebar_favicon(slint::Image{});

        std::string svcUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
        std::wstring cPath = faviconCachePath(svcUrl);
        if (GetFileAttributesW(cPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            auto img = loadFaviconImage(cPath);
            if (img.size().width > 0) appWin->set_sidebar_favicon(img);
        }
    });

    m_ui.setSidebarCloseCallback([this]() {
        if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->setVisible(false);
    });

    m_ui.setSidebarBackCallback([this]()    { if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->goBack(); });
    m_ui.setSidebarRefreshCallback([this]() { if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->reload(); });

    m_ui.setToggleAIChatCallback([this]() {
        auto& config = ConfigManager::instance().getConfig();
        std::string targetUrl = "https://duck.ai/";
        if (config.ai_engine == "claude") targetUrl = "https://claude.ai/";
        else if (config.ai_engine == "perplexity") targetUrl = "https://www.perplexity.ai/";
        else if (config.ai_engine == "gemini") targetUrl = "https://gemini.google.com/";
        
        m_ui.getAppWindow()->invoke_app_toggled(slint::SharedString(targetUrl));
    });

    m_ui.setSyncGeometryCallback([this](float x, float y, float w, float h) {
#ifdef _WIN32
        UINT dpi = 96;
        if (m_state->parentHwnd) {
            dpi = GetDpiForWindow(m_state->parentHwnd);
        }
        float scale = dpi / 96.0f;
        x *= scale; y *= scale; w *= scale; h *= scale;
#endif

        m_state->lastX = x; m_state->lastY = y; m_state->lastW = w; m_state->lastH = h;
        if (m_state->activeIndex < (int)m_state->webTracks.size())
            m_state->webTracks[m_state->activeIndex]->setBounds((int)x,(int)y,(int)w,(int)h);
    });

    m_ui.setSidebarSyncGeometryCallback([this](float x, float y, float w, float h) {
#ifdef _WIN32
        UINT dpi = 96;
        if (m_state->parentHwnd) {
            dpi = GetDpiForWindow(m_state->parentHwnd);
        }
        float scale = dpi / 96.0f;
        x *= scale; y *= scale; w *= scale; h *= scale;
#endif

        if (m_state->activeSidebarTrack)
            m_state->activeSidebarTrack->setBounds((int)x, (int)y, (int)w, (int)h);
    });
}

} // namespace Qenba
