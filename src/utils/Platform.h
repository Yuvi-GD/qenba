#pragma once

#include <string>

namespace Qenba {

class Platform {
public:
    static void setClipboardText(const std::string& text);
    static void initDarkTheme();
};

} // namespace Qenba
