#pragma once

#include <SDL3/SDL.h>
#include <string>

namespace Qenba {

class WindowContext {
public:
    WindowContext(const std::string& title, int width, int height);
    ~WindowContext();

    // Disable copy and assign
    WindowContext(const WindowContext&) = delete;
    WindowContext& operator=(const WindowContext&) = delete;

    bool init();
    void pollEvents();
    bool shouldQuit() const { return m_quit; }

    SDL_Window* getSDLWindow() const { return m_window; }

private:
    std::string m_title;
    int m_width;
    int m_height;
    SDL_Window* m_window = nullptr;
    bool m_quit = false;
};

} // namespace Qenba
