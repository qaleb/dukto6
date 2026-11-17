# Dukto
Dukto LAN file transfer tool (mirror of SVN) https://github.com/coolshou/dukto
# About
Dukto is a simple file transfer app for LAN that lets you transfer all kinds of files and documents between devices, regardless of their operating system.
# Porting to QT6
Development is ongoing.

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
