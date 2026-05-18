#pragma once

#include "ui/UIManager.h"
#include "browser/WebTrack.h"
#include <vector>
#include <map>
#include <memory>
#include <string>

namespace Qenba {

struct AppState {
    std::vector<std::shared_ptr<WebTrack>> webTracks;
    int activeIndex = 0;
    float lastX = 42, lastY = 80, lastW = 1194, lastH = 720;
    HWND parentHwnd = nullptr;

    std::map<std::string, std::shared_ptr<WebTrack>> sidebarTracks;
    std::shared_ptr<WebTrack> activeSidebarTrack;
    std::string currentSidebarUrl;
};

class AppController {
public:
    AppController(UIManager& ui);
    ~AppController();

    void init();
    void setParentHwnd(HWND hwnd);

private:
    void setupCallbacks();
    std::shared_ptr<WebTrack> createTrack(HWND parent);
    std::shared_ptr<WebTrack> getSidebarTrack(HWND parent, const std::string& url);
    void updateUrlBar();

    UIManager& m_ui;
    std::shared_ptr<AppState> m_state;
};

} // namespace Qenba
