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

// Downloads favicon from primary URL; if that produces a tiny/empty file, falls back to fallbackUrl.
void downloadFaviconWithFallback(const std::string& primaryUrl, const std::string& fallbackUrl,
                                  std::function<void(const std::wstring&)> onLoaded)
{
    if (primaryUrl.empty()) return;
    std::wstring cachePath = faviconCachePath(primaryUrl);

    if (GetFileAttributesW(cachePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        // Check size – Google returns a tiny 1x1 placeholder for unknown domains
        WIN32_FILE_ATTRIBUTE_DATA fa;
        GetFileAttributesExW(cachePath.c_str(), GetFileExInfoStandard, &fa);
        if (fa.nFileSizeLow > 50) {
            slint::invoke_from_event_loop([onLoaded, cachePath]() { onLoaded(cachePath); });
            return;
        }
        // else: placeholder – fall through to try fallback on disk
        std::wstring fbCachePath = faviconCachePath(fallbackUrl);
        if (!fallbackUrl.empty() && GetFileAttributesW(fbCachePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            slint::invoke_from_event_loop([onLoaded, fbCachePath]() { onLoaded(fbCachePath); });
            return;
        }
    }

    std::thread([primaryUrl, fallbackUrl, cachePath, onLoaded]() {
        std::wstring wUrl(primaryUrl.begin(), primaryUrl.end());
        HRESULT hr = URLDownloadToFileW(nullptr, wUrl.c_str(), cachePath.c_str(), 0, nullptr);
        bool primaryOk = false;
        if (SUCCEEDED(hr) && GetFileAttributesW(cachePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            WIN32_FILE_ATTRIBUTE_DATA fa;
            GetFileAttributesExW(cachePath.c_str(), GetFileExInfoStandard, &fa);
            if (fa.nFileSizeLow > 50) {
                primaryOk = true;
                slint::invoke_from_event_loop([onLoaded, cachePath]() { onLoaded(cachePath); });
            }
        }
        // Primary failed or returned a placeholder – try direct favicon.ico
        if (!primaryOk && !fallbackUrl.empty()) {
            std::wstring fbCachePath = faviconCachePath(fallbackUrl);
            std::wstring wFbUrl(fallbackUrl.begin(), fallbackUrl.end());
            HRESULT hr2 = URLDownloadToFileW(nullptr, wFbUrl.c_str(), fbCachePath.c_str(), 0, nullptr);
            if (SUCCEEDED(hr2) && GetFileAttributesW(fbCachePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                WIN32_FILE_ATTRIBUTE_DATA fa2;
                GetFileAttributesExW(fbCachePath.c_str(), GetFileExInfoStandard, &fa2);
                if (fa2.nFileSizeLow > 50) {
                    slint::invoke_from_event_loop([onLoaded, fbCachePath]() { onLoaded(fbCachePath); });
                }
            }
        }
    }).detach();
}

slint::Image loadFaviconImage(const std::wstring& path) {
    std::string narrow(path.begin(), path.end());
    auto img = slint::Image::load_from_path(slint::SharedString(narrow.c_str()));
    return img;
}

} // namespace Qenba
