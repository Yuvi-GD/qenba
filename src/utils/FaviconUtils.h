#pragma once

#include "app.h"
#include <string>
#include <functional>

namespace Qenba {

void initFaviconCache();
std::wstring faviconCachePath(const std::string& url);
void downloadFaviconAsync(const std::string& faviconUrl,
                          std::function<void(const std::wstring&)> onLoaded);
slint::Image loadFaviconImage(const std::wstring& path);

} // namespace Qenba
