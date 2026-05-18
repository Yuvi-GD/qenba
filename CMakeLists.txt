cmake_minimum_required(VERSION 3.25)

project(qenba VERSION 0.1.0 LANGUAGES CXX C)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# --- 1. Fetch SDL3 ---
message(STATUS "Fetching SDL3...")
FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/preview-3.1.3.zip
)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# --- 2. Slint UI (pre-compiled Windows binaries) ---
message(STATUS "Configuring Slint...")
set(Slint_DIR "${CMAKE_SOURCE_DIR}/third_party/slint/lib/cmake/Slint")
find_package(Slint REQUIRED PATHS ${Slint_DIR} NO_DEFAULT_PATH)

# --- 3. Fetch Webview (WebView2/Edge on Windows) ---
message(STATUS "Fetching Webview...")
set(WEBVIEW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    webview
    URL https://github.com/webview/webview/archive/refs/heads/master.zip
)
FetchContent_MakeAvailable(webview)

# --- Main Executable ---
add_executable(qenba WIN32
    src/main.cpp
    src/window/WindowContext.cpp
    src/ui/UIManager.cpp
    src/browser/WebTrack.cpp
    src/app/AppController.cpp
    src/app/ConfigManager.cpp
    src/utils/FaviconUtils.cpp
)

# Compile Slint UI file
if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/app.slint")
    slint_target_sources(qenba src/ui/app.slint)
endif()

# Copy config folder next to the built EXE so qenba:// protocol can find HTML files
add_custom_command(TARGET qenba POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:qenba>/config"
    COMMENT "Copying config assets to output directory"
)

# Include paths
target_include_directories(qenba PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
# webview_core_static contains the compiled WebView2 (Edge) backend for Windows
target_link_libraries(qenba PRIVATE
    SDL3::SDL3
    Slint::Slint
    webview_core_static
    version.lib
    advapi32.lib
    ole32.lib
    shell32.lib
    shlwapi.lib
    user32.lib
    uuid.lib
    urlmon.lib
)
