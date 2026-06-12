# Qenba Browser

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Framework: Slint](https://img.shields.io/badge/Framework-Slint-orange.svg)](https://slint.dev)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-blue.svg)]()

Qenba is a modern, lightweight, and highly customizable web browser designed for power users. Built on a performance-first C++20 core, it features a fluid, hardware-accelerated user interface built with the Slint UI framework, and is powered by Microsoft Edge WebView2 for state-of-the-art web rendering.

What makes Qenba unique is its hybrid architecture: a fast native core combined with highly polished modern web technologies (HTML5, CSS3, JS) that power its custom homepage (`qenba://home`) and settings panel (`qenba://settings`).

## Features

* **Native Performance:** Built on a C++20 core for fast load times and a low memory footprint.
* **Glassmorphic Interface:** A hardware-accelerated user experience with smooth animations, custom title bars, hover transitions, and a responsive tab bar.
* **Collapsible Vertical Tab Bar:** An expandable left-side sidebar for managing browser tabs dynamically.
* **Integrated AI Assistant Panel:** Quick access to your choice of AI engine (Duck AI by default, customizable to Google Gemini, Claude, or Perplexity) directly in a resizable, concurrent side panel.
* **Pinned Sidebar Apps:** Pin your favorite web applications (like YouTube, Spotify, or GitHub) to a right-hand sidebar for seamless multitasking.
* **Custom Internal Protocol:** Resolves internal pages (like `qenba://home` and `qenba://settings`) directly to local HTML files located inside the `/config` directory.
* **Modern Security Visuals:** Address bar indicators showing secure (`HTTPS`/`qenba://`) versus insecure (`HTTP`) status, alongside automated asynchronous favicon caching.
* **Deep Configuration:** Persistently save your browser state, custom home URLs, themes, and AI engine selections inside a lightweight JSON-backed configuration file.

## Tech Stack

* **Language:** C++20
* **UI Framework:** [Slint](https://slint.dev/) (a modern declarative GUI framework for Rust and C++)
* **Web Rendering:** Native Microsoft Edge WebView2 runtime via lightweight C++ wrappers
* **Build System:** CMake (minimum version 3.25)

## High-Level Architecture

Qenba cleanly separates UI presentation, runtime coordination, and native browser instances:

* **User Interface Layer:** The declarative definition of the window, including title bars, navigation controls, collapsible vertical tabs, and resizable layout splits, is managed by Slint.
* **Application Controller:** Coordinates active tabs, new tabs, closed tabs, side panel navigation, AI assistant URLs, and synchronizes geometry bounds between Slint layout components and WebView2's native child windows.
* **Browser Controller:** Hosts Edge WebView2 windows, intercepts navigation, coordinates back/forward states, and maps custom internal protocols.
* **Configuration Manager:** Persists settings (such as home page, theme, and selected AI assistant) to a local JSON file.

## Building and Running

### Prerequisites
* **Operating System:** Windows (WebView2 is natively supported).
* **Compiler:** MSVC 2022 (Visual Studio 2022) with C++20 support.
* **CMake:** Version 3.25 or higher.
* **WebView2 Runtime:** Pre-installed on most modern Windows systems.
* **Slint C++ Binaries:** Download the pre-compiled `Slint-cpp-1.9.0-win64-MSVC.exe` from the [Slint v1.9.0 GitHub Releases](https://github.com/slint-ui/slint/releases/tag/v1.9.0) and install/extract it into the `third_party/slint` folder of this repository.

### Compilation Instructions
To build Qenba from source, ensure you have populated the `third_party/slint` folder with the binaries, clone the repository, and execute the following commands in your terminal:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The build system will automatically fetch required dependencies via CMake `FetchContent`, copy the `/config` assets next to the generated executable, and compile the final binary into `build/Release/qenba.exe`.

## Customizing Local UI and Homepages

The user interface of Qenba's start page and settings is completely customizable using standard web design. You can modify:

* `/config/home.html`: The customizable homepage. Features glassmorphism widgets, dynamic background gradients, interactive bookmarks, and custom styling.
* `/config/settings.html`: The settings dashboard. Controls the browser's themes, home URL redirects, default AI chat engines, toolbar configurations, and performance settings.

These pages communicate with the native C++ backend using asynchronous window bindings:
* `window.updateConfig([key, value])`: Persists a configuration parameter to `config.json`.
* `window.getConfig()`: Retrieves the active configuration.
* `window.onStateUpdate(title, url, canBack, canForward, favicon)`: Notifies the native address bar and vertical tabs of dynamic state transitions inside the webviews.

## Contributing

Contributions to Qenba are highly welcome. Areas of interest include:
* Porting to macOS/Linux using alternative webview engines (e.g., WebKitGTK/WKWebView).
* Adding custom themes, animations, or visual layouts inside `app.slint`.
* Implementing ad-blocking, privacy shielding, or local history management.
* Adding search engine integrations.

Please feel free to open issues, discuss new features, or submit pull requests.

## License

This project is open-source and licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.
