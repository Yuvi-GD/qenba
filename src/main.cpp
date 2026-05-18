#include "window/WindowContext.h"
#include "ui/UIManager.h"
#include "app/AppController.h"
#include "app/ConfigManager.h"
#include "utils/FaviconUtils.h"
#include <SDL3/SDL_main.h>
#include <thread>
#include <windows.h>

using namespace Qenba;

int main(int argc, char* argv[]) {
    ConfigManager::instance().load();
    initFaviconCache();

    WindowContext windowCtx("Qenba Background Context", 100, 100);
    if (!windowCtx.init()) return 1;

    UIManager ui;
    ui.init();
    ui.show();

    AppController controller(ui);
    controller.init();

    // Background thread to wait for window and initialize controller
    std::thread initThread([&controller]() {
        HWND hwnd = nullptr;
        for (int i = 0; i < 80; i++) {
            hwnd = FindWindowW(nullptr, L"QENBA_INIT_MODE");
            if (hwnd) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (hwnd) {
            slint::invoke_from_event_loop([&controller, hwnd]() {
                controller.setParentHwnd(hwnd);
            });
        }
    });
    initThread.detach();

    // SDL pump thread
    std::thread sdlPump([&windowCtx]() {
        while (!windowCtx.shouldQuit()) {
            windowCtx.pollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });
    sdlPump.detach();

    ui.run();
    return 0;
}
