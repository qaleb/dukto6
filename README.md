# Dukto
Dukto LAN file transfer tool (mirror of SVN) https://github.com/coolshou/dukto
# About
Dukto is a simple file transfer app for LAN that lets you transfer all kinds of files and documents between devices, regardless of their operating system.
# Porting to QT6
Development is ongoing.

# Development approach
Keep Dukto lightweight, responsive, and portable across Debian, Android, and
Windows. Make small, reviewable changes, validating each step before adding
features or changing the interface.

# Building
Use CMake 3.16 or newer, a C++17 compiler, and Qt 6.8 or newer with Quick, Qml,
and Network. Desktop builds also require Qt Widgets for the existing file dialogs
and Windows tray integration. Android builds use Qt Core private headers for the
existing permission bridge; Widgets is not required on Android.

For a desktop build, set `CMAKE_PREFIX_PATH` to your Qt kit:

```sh
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/gcc_64
cmake --build build/desktop --parallel
```

On Windows, use a matching MSVC or MinGW Qt kit and compiler. With a multi-config
generator, build with `cmake --build build/desktop --config Release`.

On GNOME Wayland desktops, Dukto prefers XWayland when available because Qt 6.9's
Adwaita decoration ignores the maximize-button hint. An explicit
`QT_QPA_PLATFORM` setting takes precedence; native Wayland remains the fallback.

For Android, set `JAVA_HOME` to a JDK 17 installation and configure with the
selected kit's `qt-cmake`, setting
`ANDROID_SDK_ROOT` and `ANDROID_NDK_ROOT` to your installed SDK and NDK:

```sh
/path/to/Qt/android_arm64_v8a/bin/qt-cmake -S . -B build/android -DCMAKE_BUILD_TYPE=Release -DANDROID_SDK_ROOT=/path/to/sdk -DANDROID_NDK_ROOT=/path/to/ndk
cmake --build build/android --target apk --parallel
```

For Debian packaging, run `cpack -G DEB` from the desktop build directory with
`dpkg-shlibdeps` installed. The package copy is stripped; development binaries
retain their debug symbols when built with Debug or RelWithDebInfo.

To run the desktop discovery regression tests, configure with
`-DDUKTO_BUILD_TESTS=ON`, rebuild, and run
`ctest --test-dir build/desktop --output-on-failure`. Qt Test is required only
when these tests are enabled. Tests use temporary settings and local UDP sockets.

# Android fullscreen (May 2026)
If Dukto does not use the full display on recent Android versions, check these:

- In `src/qml/dukto6/Main.qml`, avoid fixed mobile window sizes like `360x600`.
- Use screen-based sizing on Android (`Screen.width` / `Screen.height`).
- Set Android window visibility to fullscreen (`Window.FullScreen`).
- Keep desktop sizing behavior separate from Android behavior.

This repository now applies that approach in `Main.qml`.

# Improvement roadmap
## Now
- Android transfer resilience:
  - Keep screen awake while transfers are active.
  - Run transfer foreground service during active transfers.
- Outbound transfer queue:
  - If a transfer is already running, new send requests are queued.
  - Next queued transfer starts automatically when current transfer ends.

## Next
- Retry manager for queued jobs (per-job retry count and manual retry).
- Rich queue UI (view, reorder, cancel specific queued jobs).
- Foreground service status updates (current filename, progress percent).

## Later
- Resume interrupted transfers for large files.
- Encrypted transfer mode.
- Multi-target send with per-target result summary.
