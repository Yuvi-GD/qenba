#include "FaviconUtils.h"
#include <windows.h>
#include <shlobj.h>
#include <urlmon.h>
#include <thread>
#include <functional>

namespace Qenba {

static std::wstring s_faviconDir;

void initFaviconCache() {
    wchar_t appdata[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata);
    s_faviconDir = std::wstring(appdata) + L"\\Qenba\\favicons\\";
    CreateDirectoryW((std::wstring(appdata) + L"\\Qenba").c_str(), nullptr);
    CreateDirectoryW(s_faviconDir.c_str(), nullptr);
}

std::wstring faviconCachePath(const std::string& url) {
    size_t h = std::hash<std::string>{}(url);
    return s_faviconDir + std::to_wstring(h) + L".png";
}

void downloadFaviconAsync(const std::string& faviconUrl,
                          std::function<void(const std::wstring&)> onLoaded)
{
    if (faviconUrl.empty()) return;
    std::wstring cachePath = faviconCachePath(faviconUrl);

    if (GetFileAttributesW(cachePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        slint::invoke_from_event_loop([onLoaded, cachePath]() { onLoaded(cachePath); });
        return;
    }

    std::thread([faviconUrl, cachePath, onLoaded]() {
        std::wstring wUrl(faviconUrl.begin(), faviconUrl.end());
        HRESULT hr = URLDownloadToFileW(nullptr, wUrl.c_str(), cachePath.c_str(), 0, nullptr);
        if (SUCCEEDED(hr)) {
            slint::invoke_from_event_loop([onLoaded, cachePath]() { onLoaded(cachePath); });
        }
    }).detach();
}

slint::Image loadFaviconImage(const std::wstring& path) {
    std::string narrow(path.begin(), path.end());
    auto img = slint::Image::load_from_path(slint::SharedString(narrow.c_str()));
    return img;
}

} // namespace Qenba
