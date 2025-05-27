#include <qglobal.h>

#ifndef WINHELPER_H
#define WINHELPER_H

#if defined(Q_OS_WIN)
#include <windows.h>
#include <ShObjIdl.h>
#include <QGuiApplication>
#include <QWindow>
#endif

class WinHelper
{
public:
    WinHelper();

#if defined(Q_OS_WIN)
    class WinTaskbarProgressHelper {
    public:
        static ITaskbarList3* getTaskbar() {
            static ITaskbarList3* pTaskbar = nullptr;
            static bool initialized = false;
            if (!initialized) {
                CoInitialize(nullptr);
                CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_ITaskbarList3, (void**)&pTaskbar);
                initialized = true;
            }
            return pTaskbar;
        }
        static HWND getMainWindowHandle() {
            const auto windows = QGuiApplication::allWindows();
            for (QWindow *w : windows) {
                if (w && w->isVisible()) {
                    return (HWND)w->winId();
                }
            }
            return nullptr;
        }
        static void setProgress(int value, int maximum) {
            ITaskbarList3* pTaskbar = getTaskbar();
            HWND hwnd = getMainWindowHandle();
            if (pTaskbar && hwnd) {
                if (maximum > 0 && value >= 0) {
                    pTaskbar->SetProgressState(hwnd, TBPF_NORMAL);
                    pTaskbar->SetProgressValue(hwnd, value, maximum);
                } else {
                    pTaskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
                }
            }
        }
        static bool isMainWindowActive() {
            HWND hwnd = getMainWindowHandle();
            if (!hwnd) return false;
            return GetForegroundWindow() == hwnd;
        }
        static void setError(HWND hwnd) {
            ITaskbarList3* pTaskbar = getTaskbar();
            if (pTaskbar && hwnd) {
                pTaskbar->SetProgressState(hwnd, TBPF_ERROR); // Red
            }
        }
        static void setSuccess(HWND hwnd) {
            ITaskbarList3* pTaskbar = getTaskbar();
            if (pTaskbar && hwnd) {
                pTaskbar->SetProgressState(hwnd, TBPF_NORMAL); // Green
            }
        }
        static void flashWindowIfNotActive(HWND hwnd, int count = 3) {
            if (!hwnd) hwnd = getMainWindowHandle();
            if (!hwnd) return;
            if (GetForegroundWindow() != hwnd) {
                FLASHWINFO fi = { sizeof(FLASHWINFO), hwnd, FLASHW_TRAY | FLASHW_TIMERNOFG, static_cast<UINT>(count), 0 };
                FlashWindowEx(&fi);
            }
        }
        static void clearProgress() {
            ITaskbarList3* pTaskbar = getTaskbar();
            HWND hwnd = getMainWindowHandle();
            if (pTaskbar && hwnd) {
                pTaskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
            }
        }
        static void resetProgressAndState() {
            clearProgress();
            // Optionally, could add more reset logic here
        }
        static void cleanup() {
            ITaskbarList3* pTaskbar = getTaskbar();
            if (pTaskbar) {
                pTaskbar->Release();
                CoUninitialize();
            }
        }
    };

    // Add this helper for desktop screenshot
    class WinScreenCaptureHelper {
    public:
        // Returns true on success, saves PNG to filePath
        static bool captureDesktopToFile(const QString &filePath);
    };
#endif
};

#endif // WINHELPER_H
