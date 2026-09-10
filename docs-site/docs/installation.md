---
title: Installation & Setup
description: How to install Luce via Windows Installer, portable package, or build from source.
sidebar_position: 2
---

# Installation & Setup

Luce is available as an automated Windows Installer (`.exe`), a standalone portable archive (`.zip`), or can be built directly from source.

---

## Windows

### Method 1: Windows Installer (Recommended)

1. Head over to the [Luce Releases](https://github.com/luce-editor/luce/releases) page on GitHub.
2. Download the latest `Luce-Setup-x64.exe`.
3. Run the installer:
   - **User Mode (Default)**: Installs into `%LOCALAPPDATA%\Programs\Luce` without requiring administrator privileges.
   - **Context Menu Integration**: Automatically registers `"Open with Luce"` for files and directories in Windows Explorer.
   - **Command Line**: Optionally adds Luce to your system `PATH`, enabling `luce .` or `luce <filename>` directly from PowerShell or Command Prompt.
   - **Desktop & Start Menu Shortcuts**: Generates clean shortcuts with the official Luce icon.

### Method 2: Portable Package

If you prefer not to use an installer:
1. Download `Luce-win64-portable.zip` from [Releases](https://github.com/luce-editor/luce/releases).
2. Extract the archive into any directory.
3. Launch `luce.exe`. All settings and sessions are stored portably next to the executable.

---

## Building from Source

### Prerequisites

- **CMake**: Version 3.20 or newer
- **C++23 Compiler**:
  - Windows: Visual Studio 2022 (MSVC v17.10+) with *Desktop development with C++*
  - Linux: GCC 13+ or Clang 16+
- **OpenGL**: 3.3 or newer compatible graphics drivers
- **Git**: Required for automatic dependency fetching via `FetchContent`

### Building on Windows (MSVC)

```powershell
# Clone the repository
git clone https://github.com/luce-editor/luce.git
cd luce

# Configure with CMake
cmake -B build

# Build Release executable
cmake --build build --config Release

# Run Luce
.\build\Release\luce.exe
```

### Building on Linux (Ubuntu / Debian)

```bash
# Install development headers and libraries
sudo apt update
sudo apt install -y build-essential cmake git \
    libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Clone and configure
git clone https://github.com/luce-editor/luce.git
cd luce
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Run Luce
./build/luce
```

All external libraries (Dear ImGui docking branch, SDL2, Lua 5.4, libvterm) and typography assets are downloaded and linked automatically during configuration.
