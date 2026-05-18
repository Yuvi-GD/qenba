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
    m_config.show_home_button = extractBool("show_home_button", true);
    m_config.dynamic_wallpaper = extractBool("dynamic_wallpaper", true);
}

void ConfigManager::save() {
    std::string path = getSettingsPath();
    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "{\n";
    f << "  \"home_url\": \"" << m_config.home_url << "\",\n";
    f << "  \"theme\": \"" << m_config.theme << "\",\n";
    f << "  \"ai_engine\": \"" << m_config.ai_engine << "\",\n";
    f << "  \"show_home_button\": " << (m_config.show_home_button ? "true" : "false") << ",\n";
    f << "  \"dynamic_wallpaper\": " << (m_config.dynamic_wallpaper ? "true" : "false") << "\n";
    f << "}\n";
}

} // namespace Qenba
