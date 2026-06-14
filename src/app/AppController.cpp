#include "AppController.h"
#include "utils/FaviconUtils.h"
#include "ConfigManager.h"
#include "platform/Platform.h"
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

    // Load pinned apps from config
    auto pinnedModel = std::make_shared<slint::VectorModel<PinnedApp>>();
    for (const auto& appConf : config.pinned_apps) {
        PinnedApp pa;
        pa.title = slint::SharedString(appConf.title);
        pa.url = slint::SharedString(appConf.url);
        pa.icon = slint::SharedString(appConf.icon);
        pinnedModel->push_back(pa);
    }
    appWin->set_pinned_apps(pinnedModel);

    // Asynchronously fetch/load favicons for pinned apps
    for (size_t i = 0; i < config.pinned_apps.size(); ++i) {
        std::string trackUrl = config.pinned_apps[i].url;
        std::string domain = trackUrl;
        auto ps = domain.find("://");
        if (ps != std::string::npos) domain = domain.substr(ps + 3);
        auto sl = domain.find('/');
        if (sl != std::string::npos) domain = domain.substr(0, sl);
        
        if (!domain.empty()) {
            std::string svcUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
            downloadFaviconAsync(svcUrl, [this, trackUrl](const std::wstring& path) {
                auto img = loadFaviconImage(path);
                if (img.size().width == 0) return;

                auto& appWin = m_ui.getAppWindow();
                auto model = appWin->get_pinned_apps();
                auto nm = std::make_shared<slint::VectorModel<PinnedApp>>();
                bool found = false;
                for (int j = 0; j < model->row_count(); ++j) {
                    auto item = *model->row_data(j);
                    if (std::string(item.url.data()) == trackUrl) {
                        item.favicon = img;
                        found = true;
                    }
                    nm->push_back(item);
                }
                if (found) appWin->set_pinned_apps(nm);
            });
        }
    }
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
                std::string googleUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
                std::string directUrl = "https://" + domain + "/favicon.ico";
                
                WebTrack* trackPtr = raw;
                downloadFaviconWithFallback(googleUrl, directUrl, [this, trackPtr](const std::wstring& path) {
                    auto img = loadFaviconImage(path);
                    if (img.size().width == 0) return;
                    
                    auto& appWin = m_ui.getAppWindow();
                    auto current = appWin->get_tabs();
                    
                    int currentIdx = -1;
                    for (int i = 0; i < (int)m_state->webTracks.size(); ++i) {
                        if (m_state->webTracks[i].get() == trackPtr) {
                            currentIdx = i; break;
                        }
                    }
                    if (currentIdx == -1 || currentIdx >= current->row_count()) return;
                    
                    auto tab = *current->row_data(currentIdx);
                    tab.favicon = img;
                    
                    auto model = std::make_shared<slint::VectorModel<TabData>>();
                    for (int i = 0; i < current->row_count(); ++i)
                        model->push_back(i == currentIdx ? tab : *current->row_data(i));
                    
                    appWin->set_tabs(model);
                });
                return;
            }
        }

        if (faviconServiceUrl.empty()) {
            // Explicitly clear the favicon in the UI for internal pages or when no favicon is available
            auto& appWin = m_ui.getAppWindow();
            auto current = appWin->get_tabs();
            if (idx >= 0 && idx < current->row_count()) {
                auto tab = *current->row_data(idx);
                if (tab.favicon.size().width > 0) {
                    tab.favicon = slint::Image();
                    auto model = std::make_shared<slint::VectorModel<TabData>>();
                    for (int i = 0; i < current->row_count(); ++i)
                        model->push_back(i == idx ? tab : *current->row_data(i));
                    appWin->set_tabs(model);
                }
            }
            return;
        }

        WebTrack* trackPtr = raw;
        downloadFaviconWithFallback(faviconServiceUrl, "", [this, trackPtr](const std::wstring& path) {
            auto img = loadFaviconImage(path);
            if (img.size().width == 0) return;
            
            auto& appWin = m_ui.getAppWindow();
            auto current = appWin->get_tabs();
            
            int currentIdx = -1;
            for (int i = 0; i < (int)m_state->webTracks.size(); ++i) {
                if (m_state->webTracks[i].get() == trackPtr) {
                    currentIdx = i; break;
                }
            }
            if (currentIdx == -1 || currentIdx >= current->row_count()) return;
            
            auto tab = *current->row_data(currentIdx);
            tab.favicon = img;
            
            auto model = std::make_shared<slint::VectorModel<TabData>>();
            for (int i = 0; i < current->row_count(); ++i)
                model->push_back(i == currentIdx ? tab : *current->row_data(i));
            
            appWin->set_tabs(model);
        });
    });

    track->setOnInteraction([this]() {
        slint::invoke_from_event_loop([this]() {
            m_ui.getAppWindow()->invoke_clear_addr_focus();
        });
    });

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

        if (!domain.empty() && domain.find("config/") == std::string::npos
                            && domain.find("add-app") == std::string::npos) {
            std::string googleUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
            std::string directUrl = "https://" + domain + "/favicon.ico";
            downloadFaviconWithFallback(googleUrl, directUrl, [this, trackUrl](const std::wstring& path) {
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

    track->setOnSubmitAddApp([this](const std::string& url) {
        slint::invoke_from_event_loop([this, url]() {
            if (url == "CURRENT_TAB") {
                m_ui.getAppWindow()->invoke_sidebar_add_current_tab();
            } else {
                m_ui.getAppWindow()->invoke_sidebar_submit_add_app(slint::SharedString(url));
            }
        });
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
        // Force focus release so URL shows up cleanly
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
                    // Use configured search engine
                    auto& config = ConfigManager::instance().getConfig();
                    std::string searchBase = "https://duckduckgo.com/?q=";
                    if (config.search_engine == "google") searchBase = "https://www.google.com/search?q=";
                    else if (config.search_engine == "bing") searchBase = "https://www.bing.com/search?q=";
                    else if (config.search_engine == "brave") searchBase = "https://search.brave.com/search?q=";
                    targetUrl = searchBase + url;
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

    m_ui.setSidebarAddCurrentTabCallback([this]() {
        if (m_state->activeIndex >= 0 && m_state->activeIndex < (int)m_state->webTracks.size()) {
            auto track = m_state->webTracks[m_state->activeIndex];
            std::string url = track->getUrl();
            std::string title = track->getTitle();
            
            if (url.empty() || url == "about:blank" || url.find("qenba://") == 0) return;
            
            std::string domain = url;
            auto ps = domain.find("://");
            if (ps != std::string::npos) domain = domain.substr(ps + 3);
            auto sl = domain.find('/');
            if (sl != std::string::npos) domain = domain.substr(0, sl);
            std::string iconLetter = "W";
            if (!domain.empty() && std::isalnum((unsigned char)domain[0])) {
                iconLetter = std::string(1, (char)std::toupper((unsigned char)domain[0]));
            }
            
            auto& appWin = m_ui.getAppWindow();
            auto model = appWin->get_pinned_apps();
            auto nm = std::make_shared<slint::VectorModel<PinnedApp>>();
            bool alreadyPinned = false;
            for (int i = 0; i < model->row_count(); ++i) {
                auto item = *model->row_data(i);
                if (std::string(item.url.data()) == url) { alreadyPinned = true; break; }
                nm->push_back(item);
            }
            
            if (!alreadyPinned) {
                PinnedApp pa; pa.title = title.c_str(); pa.url = url.c_str(); pa.icon = iconLetter.c_str();
                nm->push_back(pa);
                appWin->set_pinned_apps(nm);
                
                auto config = ConfigManager::instance().getConfig();
                PinnedAppConfig pc; pc.title = title; pc.url = url; pc.icon = iconLetter;
                config.pinned_apps.push_back(pc);
                ConfigManager::instance().setConfig(config);
            }
            
            appWin->set_is_adding_app(false);
            if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->setVisible(true);
            appWin->set_sidebar_open(false);
            if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->setVisible(false);
        }
    });

    m_ui.setSidebarAddAppMenuCallback([this]() {
        auto& appWin = m_ui.getAppWindow();
        bool isAdding = !appWin->get_is_adding_app();
        appWin->set_is_adding_app(isAdding);
        
        if (isAdding) {
            appWin->set_sidebar_open(true);
            // Clear any stale title/favicon from previous panel
            appWin->set_sidebar_title(slint::SharedString("Add Sidebar App"));
            appWin->set_sidebar_url(slint::SharedString(""));
            appWin->set_sidebar_favicon(slint::Image{});
            if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->setVisible(false);
            
            // Switch to add-app HTML track
            std::string addAppUrl = m_state->isAppBarMode ? "qenba://add-app?appbar=1" : "qenba://add-app";
            m_state->currentSidebarUrl = addAppUrl;
            auto newTrack = getSidebarTrack(m_state->parentHwnd, addAppUrl);
            m_state->activeSidebarTrack = newTrack;
            newTrack->navigate(addAppUrl);
            newTrack->setVisible(true);
        } else {
            appWin->set_sidebar_open(false);
            if (m_state->activeSidebarTrack) {
                m_state->activeSidebarTrack->setVisible(false);
                m_state->activeSidebarTrack.reset();
                m_state->currentSidebarUrl = "";
            }
        }
    });

    m_ui.setNotifyAddingAppCallback([this](bool adding) {
        if (!adding && m_state->activeSidebarTrack && m_state->activeSidebarTrack->getUrl().find("add-app") != std::string::npos) {
            m_state->activeSidebarTrack->setVisible(false);
            auto newTrack = getSidebarTrack(m_state->parentHwnd, m_state->currentSidebarUrl);
            m_state->activeSidebarTrack = newTrack;
            if (newTrack) newTrack->setVisible(true);
        }
    });

    m_ui.setSidebarSubmitAddAppCallback([this](const std::string& inputUrl) {
        std::string url = inputUrl;
        if (url.find("://") == std::string::npos && !url.empty()) {
            url = "https://" + url;
        }
        
        auto& appWin = m_ui.getAppWindow();
        std::string d = url;
        auto ps = d.find("://"); if (ps != std::string::npos) d = d.substr(ps + 3);
        if (d.size() >= 4 && d.substr(0,4) == "www.") d = d.substr(4);
        auto sl = d.find('/'); if (sl != std::string::npos) d = d.substr(0, sl);
        
        std::string iconLetter = "W";
        if (!d.empty() && std::isalnum((unsigned char)d[0])) iconLetter = std::string(1, (char)std::toupper((unsigned char)d[0]));
        
        auto model = appWin->get_pinned_apps();
        auto nm = std::make_shared<slint::VectorModel<PinnedApp>>();
        bool alreadyPinned = false;
        for (int i = 0; i < model->row_count(); ++i) {
            auto item = *model->row_data(i);
            if (std::string(item.title.data()) == d || std::string(item.url.data()) == url) { alreadyPinned = true; }
            nm->push_back(item);
        }
        
        if (!alreadyPinned && !url.empty() && url != "https://") {
            PinnedApp pa; pa.title = d.c_str(); pa.url = url.c_str(); pa.icon = iconLetter.c_str();
            nm->push_back(pa);
            appWin->set_pinned_apps(nm);
            
            auto config = ConfigManager::instance().getConfig();
            PinnedAppConfig pc; pc.title = d; pc.url = url; pc.icon = iconLetter;
            config.pinned_apps.push_back(pc);
            ConfigManager::instance().setConfig(config);
            
            // Kick off an async load immediately for the newly added app
            std::string googleUrl = "https://www.google.com/s2/favicons?domain=" + d + "&sz=32";
            std::string directUrl = "https://" + d + "/favicon.ico";
            std::string trackUrl = url;
            downloadFaviconWithFallback(googleUrl, directUrl, [this, trackUrl](const std::wstring& path) {
                auto img = loadFaviconImage(path);
                if (img.size().width == 0) return;
                auto& aw = m_ui.getAppWindow();
                auto mod = aw->get_pinned_apps();
                auto nn = std::make_shared<slint::VectorModel<PinnedApp>>();
                bool fnd = false;
                for (int i = 0; i < mod->row_count(); ++i) {
                    auto item = *mod->row_data(i);
                    if (std::string(item.url.data()) == trackUrl) {
                        item.favicon = img;
                        fnd = true;
                    }
                    nn->push_back(item);
                }
                if (fnd) aw->set_pinned_apps(nn);
            });
        }
        
        appWin->set_is_adding_app(false);
        appWin->set_sidebar_open(false);
        if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->setVisible(false);
    });

    m_ui.setSidebarSettingsCallback([this]() {
        if (m_state->isAppBarMode) {
            m_ui.getAppWindow()->invoke_app_toggled(slint::SharedString("qenba://settings"));
        } else {
            if (m_state->activeIndex < (int)m_state->webTracks.size())
                m_state->webTracks[m_state->activeIndex]->navigate("qenba://settings");
        }
    });

    m_ui.setSidebarContextMenuCallback([this](const std::string& url) {
        slint::invoke_from_event_loop([this, url]() {
            Platform::initDarkTheme();
            POINT pt;
            GetCursorPos(&pt);
            
            SetForegroundWindow(m_state->parentHwnd);
            
            HMENU hMenu = CreatePopupMenu();
            AppendMenuA(hMenu, MF_STRING, 1, "Refresh");
            AppendMenuA(hMenu, MF_STRING, 2, "Close");
            AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuA(hMenu, MF_STRING, 3, "Remove");
            
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_state->parentHwnd, NULL);
            DestroyMenu(hMenu);
            PostMessage(m_state->parentHwnd, WM_NULL, 0, 0);
            
            if (cmd == 1) {
                // Refresh
                if (m_state->sidebarTracks.count(url)) m_state->sidebarTracks[url]->reload();
            } else if (cmd == 2) {
                // Close
                if (m_state->sidebarTracks.count(url)) {
                    if (m_state->currentSidebarUrl == url) m_ui.getAppWindow()->invoke_sidebar_close();
                    else m_state->sidebarTracks.erase(url);
                }
            } else if (cmd == 3) {
                // Remove
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
                
                // Save to config
                auto config = ConfigManager::instance().getConfig();
                std::vector<PinnedAppConfig> newPins;
                for (const auto& p : config.pinned_apps) {
                    if (p.url != url) newPins.push_back(p);
                }
                config.pinned_apps = newPins;
                ConfigManager::instance().setConfig(config);
                
                // Close if active
                if (m_state->sidebarTracks.count(url)) {
                    if (m_state->currentSidebarUrl == url) m_ui.getAppWindow()->invoke_sidebar_close();
                    else m_state->sidebarTracks.erase(url);
                }
            }
        });
    });

    m_ui.setSidebarNavigateCallback([this](const std::string& url) {
        if (url.empty() || !m_state->parentHwnd) return;

        m_ui.getAppWindow()->set_is_adding_app(false);

        // Resolve qenba://search to the configured search engine homepage
        std::string resolvedUrl = url;
        if (url == "qenba://search") {
            auto& config = ConfigManager::instance().getConfig();
            resolvedUrl = "https://duckduckgo.com";
            if (config.search_engine == "google") resolvedUrl = "https://www.google.com";
            else if (config.search_engine == "bing") resolvedUrl = "https://www.bing.com";
            else if (config.search_engine == "brave") resolvedUrl = "https://search.brave.com";
        }

        if (m_state->activeSidebarTrack && m_state->currentSidebarUrl == resolvedUrl && m_ui.getAppWindow()->get_sidebar_open()) {
            m_ui.getAppWindow()->set_sidebar_open(false);
            m_state->activeSidebarTrack->setVisible(false);
            m_state->activeSidebarTrack.reset();
            m_state->currentSidebarUrl = "";
            return;
        }

        if (m_state->activeSidebarTrack && m_state->currentSidebarUrl != resolvedUrl)
            m_state->activeSidebarTrack->setVisible(false);

        m_state->currentSidebarUrl = resolvedUrl;
        auto track = getSidebarTrack(m_state->parentHwnd, resolvedUrl);
        track->setVisible(true);
        m_state->activeSidebarTrack = track;

        std::string domain = resolvedUrl;
        auto ps = domain.find("://");
        if (ps != std::string::npos) domain = domain.substr(ps + 3);
        auto sl = domain.find('/');
        if (sl != std::string::npos) domain = domain.substr(0, sl);

        auto& appWin = m_ui.getAppWindow();
        auto cachedTitle = track->getTitle();

        // Build a compact 1-2 char icon label from the domain for the favicon placeholder
        std::string iconLabel = "?";
        if (!domain.empty() && std::isalnum((unsigned char)domain[0])) {
            iconLabel = std::string(1, (char)std::toupper((unsigned char)domain[0]));
            // Add second char if available and alphanumeric
            if (domain.size() > 1 && std::isalnum((unsigned char)domain[1]))
                iconLabel += (char)std::tolower((unsigned char)domain[1]);
        }

        appWin->set_sidebar_title(slint::SharedString(cachedTitle.empty() ? iconLabel.c_str() : cachedTitle.c_str()));
        appWin->set_sidebar_url(slint::SharedString(domain.c_str()));
        appWin->set_sidebar_favicon(slint::Image{});

        // Try Google favicon service first, then direct favicon.ico
        std::string googleUrl = "https://www.google.com/s2/favicons?domain=" + domain + "&sz=32";
        std::string directUrl = "https://" + domain + "/favicon.ico";
        // Check cache first (synchronously)
        std::wstring cPath = faviconCachePath(googleUrl);
        if (GetFileAttributesW(cPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            auto img = loadFaviconImage(cPath);
            if (img.size().width > 0) appWin->set_sidebar_favicon(img);
        } else {
            downloadFaviconWithFallback(googleUrl, directUrl, [this, resolvedUrl](const std::wstring& path) {
                if (m_state->currentSidebarUrl != resolvedUrl) return;
                auto img = loadFaviconImage(path);
                if (img.size().width > 0) m_ui.getAppWindow()->set_sidebar_favicon(img);
            });
        }
    });

    m_ui.setSidebarCloseCallback([this]() {
        if (m_state->activeSidebarTrack) {
            m_state->activeSidebarTrack->setVisible(false);
            m_state->sidebarTracks.erase(m_state->currentSidebarUrl);
            m_state->activeSidebarTrack.reset();
        }
    });

    m_ui.setSidebarHideCallback([this]() {
        if (m_state->activeSidebarTrack) m_state->activeSidebarTrack->setVisible(false);
    });

    m_ui.setCopyToClipboardCallback([this](const std::string& url) {
        Platform::setClipboardText(url);
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

    m_ui.setToggleAppbarModeCallback([this]() {
        m_state->isAppBarMode = !m_state->isAppBarMode;
        m_ui.getAppWindow()->set_is_appbar_mode(m_state->isAppBarMode);

#ifdef _WIN32
        if (!m_state->parentHwnd) return;

        if (m_state->isAppBarMode) {
            // Hide main webview
            if (m_state->activeIndex >= 0 && m_state->activeIndex < (int)m_state->webTracks.size()) {
                m_state->webTracks[m_state->activeIndex]->setVisible(false);
            }

            int width = m_state->appBarState.currentAppBarWidth > 0 ? m_state->appBarState.currentAppBarWidth : 400;
            Platform::WindowsAppBarManager::RegisterAppBar(m_state->parentHwnd, width, m_state->appBarState);
        } else {
            Platform::WindowsAppBarManager::UnregisterAppBar(m_state->parentHwnd, m_state->appBarState);

            // Show main webview again
            if (m_state->activeIndex >= 0 && m_state->activeIndex < (int)m_state->webTracks.size()) {
                m_state->webTracks[m_state->activeIndex]->setVisible(true);
            }
        }
#endif
    });

    m_ui.setSyncAppbarWidthCallback([this](float width) {
#ifdef _WIN32
        if (!m_state->isAppBarMode || !m_state->parentHwnd) return;
        Platform::WindowsAppBarManager::SetAppBarWidth(m_state->parentHwnd, width, m_state->appBarState);
#endif
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
