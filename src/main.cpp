#include "ui/UIManager.h"
#include "app/AppController.h"
#include "app/ConfigManager.h"
#include "utils/FaviconUtils.h"
#include <thread>
#include <windows.h>

using namespace Qenba;

int main(int argc, char* argv[]) {
    ConfigManager::instance().load();
    initFaviconCache();

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

    ui.run();
    return 0;
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
    return main(__argc, __argv);
}
#endif
