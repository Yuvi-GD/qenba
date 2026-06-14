#pragma once

#include <string>
#include <map>
#include <vector>
#include <filesystem>

namespace Qenba {

struct PinnedAppConfig {
    std::string title;
    std::string url;
    std::string icon;
};

struct AppConfig {
    std::string home_url = "qenba://home";
    std::string theme = "dark";
    std::string ai_engine = "duck";
    std::string search_engine = "duckduckgo";
    bool show_home_button = true;
    bool dynamic_wallpaper = true;
    std::vector<PinnedAppConfig> pinned_apps;
};

class ConfigManager {
public:
    static ConfigManager& instance();

    void load();
    void save();

    const AppConfig& getConfig() const { return m_config; }
    void setConfig(const AppConfig& config) { m_config = config; save(); }

    std::string getSettingsPath();

private:
    ConfigManager() = default;
    AppConfig m_config;
};

} // namespace Qenba
