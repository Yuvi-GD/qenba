#include "WindowContext.h"
#include <iostream>

namespace Qenba {

WindowContext::WindowContext(const std::string& title, int width, int height)
    : m_title(title), m_width(width), m_height(height) {
}

WindowContext::~WindowContext() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

bool WindowContext::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create hidden to avoid dual-window conflict with Slint's default backend for Phase 1
    m_window = SDL_CreateWindow(m_title.c_str(), m_width, m_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!m_window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    return true;
}

void WindowContext::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_quit = true;
        }
        // Future: Handle window resize events here to sync Slint and Webview
        // Future: Pass input events to Slint if not automatically handled
    }
}

} // namespace Qenba
