# OpenDAQ Qt GUI - Package

This directory contains packaging configurations for different platforms.

## Directory Structure

```
package/
├── mac/              # macOS packaging (DMG)
│   ├── CMakeLists.txt
│   └── build_dmg.sh
├── windows/          # Windows packaging (NSIS installer)
│   ├── CMakeLists.txt
│   ├── installer.nsi.in
│   └── build_installer.bat
├── linux/            # Linux packaging (DEB)
│   ├── CMakeLists.txt
│   └── build_deb.sh
└── README.md
```

## macOS DMG Package

To build a DMG installer for macOS:

```bash
./package/mac/build_dmg.sh
```

The DMG file will be created at: `build/package/OpenDAQ Qt GUI-1.0-<arch>.dmg` (where `<arch>` is `arm64` or `x86_64`)

For more details, see [mac/](mac/) directory.

## Windows Installer

To build a Windows installer (NSIS):

**Prerequisites:**
- NSIS installed (https://nsis.sourceforge.io/Download)
- Qt6 installed and available in PATH or CMAKE_PREFIX_PATH

**Build steps:**

On Windows:
```cmd
package\windows\build_installer.bat
```

Or manually:
```cmd
cd package\windows
cmake -B ..\..\build\package -S .
cmake --build ..\..\build\package --config Release
cmake --build ..\..\build\package --target create_installer
```

The installer will be created at: `build/package/OpenDAQ Qt GUI-1.0-Windows-<arch>.exe` (where `<arch>` is `x64` or `arm64`)

## Linux DEB Package

To build a DEB package for Linux:

**Prerequisites:**
- dpkg-dev installed
- Qt6 development packages installed

**Build steps:**

```bash
./package/linux/build_deb.sh
```

Or manually:
```bash
cd package/linux
cmake -B ../../build/package -S .
cmake --build ../../build/package
cmake --build ../../build/package --target create_deb
```

The DEB file will be created at: `build/package/opendaq-qt-gui_1.0_<arch>.deb` (where `<arch>` is `amd64`, `arm64`, `armhf`, etc.)

