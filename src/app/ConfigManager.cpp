#include "ConfigManager.h"
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Qenba {

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

std::string ConfigManager::getSettingsPath() {
    std::filesystem::path basePath;
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH)) {
        basePath = std::filesystem::path(path).parent_path();
    } else {
        basePath = std::filesystem::current_path();
    }
#else
    basePath = std::filesystem::current_path();
#endif
    
    auto configDir = basePath / "config";
    if (!std::filesystem::exists(configDir)) {
        std::filesystem::create_directories(configDir);
    }
    return (configDir / "config.json").string();
}

void ConfigManager::load() {
    std::string path = getSettingsPath();
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    // Very basic manual JSON extraction for Phase 2
    auto extractBool = [&](const std::string& key, bool defaultVal) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        size_t colon = content.find(":", pos);
        if (colon == std::string::npos) return defaultVal;
        size_t valPos = content.find_first_of("tf", colon);
        if (valPos == std::string::npos) return defaultVal;
        return content[valPos] == 't';
    };

    auto extractString = [&](const std::string& key, const std::string& defaultVal) {
        size_t pos = content.find("\"" + key + "\"");
        if (pos == std::string::npos) return defaultVal;
        size_t colon = content.find(":", pos);
        if (colon == std::string::npos) return defaultVal;
        size_t startQuote = content.find("\"", colon);
        if (startQuote == std::string::npos) return defaultVal;
        size_t endQuote = content.find("\"", startQuote + 1);
        if (endQuote == std::string::npos) return defaultVal;
        return content.substr(startQuote + 1, endQuote - startQuote - 1);
    };

    m_config.home_url = extractString("home_url", "qenba://home");
    m_config.theme = extractString("theme", "dark");
    m_config.ai_engine = extractString("ai_engine", "duck");
    m_config.search_engine = extractString("search_engine", "duckduckgo");
    m_config.show_home_button = extractBool("show_home_button", true);
    m_config.dynamic_wallpaper = extractBool("dynamic_wallpaper", true);

    // Parse pinned_apps array
    m_config.pinned_apps.clear();
    size_t apps_start = content.find("\"pinned_apps\"");
    if (apps_start != std::string::npos) {
        size_t array_start = content.find("[", apps_start);
        size_t array_end = content.find("]", apps_start);
        if (array_start != std::string::npos && array_end != std::string::npos && array_end > array_start) {
            std::string array_content = content.substr(array_start, array_end - array_start);
            size_t pos = 0;
            while ((pos = array_content.find("{", pos)) != std::string::npos) {
                size_t obj_end = array_content.find("}", pos);
                if (obj_end == std::string::npos) break;
                std::string obj = array_content.substr(pos, obj_end - pos);
                
                auto extractStrObj = [&](const std::string& key) {
                    size_t k_pos = obj.find("\"" + key + "\"");
                    if (k_pos == std::string::npos) return std::string("");
                    size_t colon = obj.find(":", k_pos);
                    if (colon == std::string::npos) return std::string("");
                    size_t startQuote = obj.find("\"", colon);
                    if (startQuote == std::string::npos) return std::string("");
                    size_t endQuote = obj.find("\"", startQuote + 1);
                    if (endQuote == std::string::npos) return std::string("");
                    return obj.substr(startQuote + 1, endQuote - startQuote - 1);
                };

                PinnedAppConfig app;
                app.title = extractStrObj("title");
                app.url = extractStrObj("url");
                app.icon = extractStrObj("icon");
                if (!app.url.empty()) {
                    m_config.pinned_apps.push_back(app);
                }
                pos = obj_end + 1;
            }
        }
    }
}

void ConfigManager::save() {
    std::string path = getSettingsPath();
    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "{\n";
    f << "  \"home_url\": \"" << m_config.home_url << "\",\n";
    f << "  \"theme\": \"" << m_config.theme << "\",\n";
    f << "  \"ai_engine\": \"" << m_config.ai_engine << "\",\n";
    f << "  \"search_engine\": \"" << m_config.search_engine << "\",\n";
    f << "  \"show_home_button\": " << (m_config.show_home_button ? "true" : "false") << ",\n";
    f << "  \"dynamic_wallpaper\": " << (m_config.dynamic_wallpaper ? "true" : "false") << ",\n";
    
    f << "  \"pinned_apps\": [\n";
    for (size_t i = 0; i < m_config.pinned_apps.size(); ++i) {
        const auto& app = m_config.pinned_apps[i];
        f << "    {\n";
        f << "      \"title\": \"" << app.title << "\",\n";
        f << "      \"url\": \"" << app.url << "\",\n";
        f << "      \"icon\": \"" << app.icon << "\"\n";
        f << "    }" << (i < m_config.pinned_apps.size() - 1 ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

} // namespace Qenba
