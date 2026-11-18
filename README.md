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
and Windows tray integration. Android uses public Qt JNI APIs and requires
neither Widgets nor Qt private headers.

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

To run the desktop backend regression tests, configure with
`-DDUKTO_BUILD_TESTS=ON`, rebuild, and run
`ctest --test-dir build/desktop --output-on-failure`. Qt Test is required only
when these tests are enabled. Tests use temporary settings and localhost sockets.
The suites cover discovery, legacy transfers, recipient capture, retry and
temporary-file ownership, receive validation, history limits, sharing handoff,
and worker shutdown. Windows registry-dependent cases are skipped to avoid
modifying user settings.

# Appearance
Settings include System, Light and Dark appearance modes. System follows Qt's
platform appearance notifications; explicit choices remain in effect until
changed. The preference persists across restarts, independently of the accent
color. Appearance reuses the shared palette and Qt's platform notifications,
without polling the OS. Linux packages include Qt's desktop theme/portal plugins
so the bundled application can read the desktop appearance preference.

# Transfer backend
Queue entries capture the recipient and content when submitted. One transfer
runs at a time, with up to two automatic retries; retries keep their original
recipient and temporary resources. File enumeration, provider access, and
transfer I/O run on a dedicated worker. Streaming uses 64 KiB chunks with a
256 KiB socket backlog, and progress updates are limited to about ten per second.

The receiver handles fragmented headers and empty files, validates paths and
sizes, and checks writes. Individual files are committed atomically; an
interrupted file is discarded while previously completed files remain. The
legacy protocol has no receiver acknowledgement, encryption, or resume support.
Session history retains at most 100 entries and 32 MiB of content.

Android shares arrive through activity callbacks and remain pending until the
user chooses a buddy. Manual IP recipients use the existing Send button, labeled
Send shared content while a share is pending. Known-size URI content is streamed;
unknown-size content is staged on the worker and cleaned up after use. URI grants
can expire, in which case content must be selected or shared again.

On Android 10 and later, completed files go directly into public storage through
MediaStore, without broad storage permissions or a second whole-file copy:

- Images: `Pictures/Dukto`, indexed for gallery apps.
- Videos: `Movies/Dukto`; audio: `Music/Dukto`.
- Documents: `Documents/Dukto`.
- Archives and other files: `Download/Dukto`.

Transferred folders stay together under `Download/Dukto`, including empty
directories, so projects and HTML/assets remain usable. Repeated root folder
names receive a numeric suffix rather than merging into an earlier transfer.
Files are hidden with `IS_PENDING` until fully written. Interrupted files are
removed; startup recovers
pending output left by a process crash. Gallery display depends on the viewer's
refresh and supported formats. Public files survive uninstall. Existing files in
the old app-specific destination are not moved automatically.

Android settings show the automatic destination policy rather than a filesystem
path. An optional SD-card folder uses the system folder picker and a persisted
Storage Access Framework grant. Select a writable subfolder, rather than a card
root restricted by Android. This override keeps received content in the chosen
folder; automatic categories resume with **Use automatic folders**. A receive
batch captures its destination when accepted. Providers must support renaming and
deleting temporary output; duplicates preserve earlier files. Android 9 requires
a shared folder selected through this picker. Desktop destination settings are
unchanged.

New incoming transfers are accepted while Dukto is foregrounded. An active
transfer and already queued work can continue with the screen off using a
`connectedDevice` foreground service and a timed, renewed partial CPU wake lock.
Qt's coordinator keeps processing completion and queue events while suspended;
cosmetic and discovery timers pause in the background. The service and lock
stop when the transfer queue is idle. The **Battery
settings** shortcut opens the app's system settings; select unrestricted battery
use there if the device's power policy interrupts transfers. Deep Doze can still
restrict networking, and a force-stop, process kill, or connection loss interrupts
the transfer. The legacy protocol cannot resume interrupted files.

`tests/android-storage` is a small dependency-free instrumentation APK exercising
the production storage bridge against real MediaStore and a document provider
with opaque IDs. Build with JDK 17 and the Android SDK configured:

```sh
bash android/gradlew -p tests/android-storage assembleDebug
adb -s emulator-PORT install -r tests/android-storage/build/outputs/apk/debug/DuktoStorageChecks-debug.apk
adb -s emulator-PORT shell am instrument -w dev.dukto.storagecheck/.StorageChecks
```

For end-to-end tests, install and foreground the Dukto debug APK in a disposable
emulator, dismiss first-run system dialogs, and allow notifications. A root ADB
test shell is needed to inspect shared files; the app remains unprivileged.
Forward host TCP 15644 to emulator TCP 4644, then run:

```sh
python3 tests/android_receive_smoke.py --adb /path/to/adb --serial emulator-PORT --output /tmp/dukto-android-results.json
```

This checks real received bytes, publication, partial-file cleanup, folder
structure, foreground-service lifecycle, and a screen-off transfer. Physical
SD cards, manufacturer power policies, and Windows native behavior still require
device testing. Emulator and localhost results do not establish Wi-Fi throughput
or battery improvements.

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
  - Continue active transfers using a foreground service and CPU wake lock.
  - Receive shares through activity callbacks without polling.
- Outbound transfer queue:
  - Jobs retain their recipient and resources across queuing and retries.
  - Next queued transfer starts automatically when current transfer ends.

## Next
- Manual retry controls for queued jobs.
- Rich queue UI (view, reorder, cancel specific queued jobs).
- Foreground service status updates (current filename, progress percent).

## Later
- Resume interrupted transfers for large files.
- Encrypted transfer mode.
- Multi-target send with per-target result summary.

# Text and settings usability (6.9.9)
Settings use aligned, touch-sized controls and an explicitly themed dropdown.
On Android, automatic receive destinations are explained under the expandable
**Storage & battery** section; the default settings screen omits the redundant
received-files banner. Optional shared-folder and battery controls remain there.
The Android footer centers Addresses and Settings with persistent labels.
Desktop also retains the Received shortcut.

The text page keeps Paste, Edit and Send in an action bar above the on-screen
keyboard. Android reports its visible window frame through the existing JNI
bridge, covering fullscreen devices that do not resize or provide a useful Qt
keyboard rectangle. No keyboard polling is used. Desktop retains its full editor
height. Ctrl+Enter sends; normal Enter inserts a newline. Mouse/keyboard selection,
long-press selection, and the Edit menu provide Cut, Copy, Paste, Select all, Undo
and Redo; received messages are read-only and offer selection and copying.

Very short Android transfers queue the foreground service's STOP after START,
instead of destroying a service that has not yet called startForeground. This
prevents the delayed ForegroundServiceDidNotStartInTimeException after receiving
small text snippets. To check this on a disposable emulator, foreground Dukto,
disable receive confirmation, forward TCP 15644 to device TCP 4644 and run:

```sh
python3 tests/android_text_receive_smoke.py --serial emulator-PORT --output /tmp/dukto-text-results.json
```

The regression sends short, empty, Unicode and large text and waits beyond the
service-start deadline, checking process survival and idle-service cleanup.
