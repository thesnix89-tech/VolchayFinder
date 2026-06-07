#include "TaskbarController.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QSettings>
#include <QProcess>
#include <QTimer>

#include <windows.h>
#include <shellapi.h>

namespace {
constexpr auto kTaskbarClass = TEXT("Shell_TrayWnd");
constexpr auto kStartButtonClass = TEXT("Button");
}

TaskbarController::TaskbarController(QObject* parent)
    : QObject(parent)
{
    // Poll the foreground window so the dock can auto-hide for fullscreen apps (macOS-style).
    m_fullscreenTimer = new QTimer(this);
    m_fullscreenTimer->setInterval(350);
    connect(m_fullscreenTimer, &QTimer::timeout, this, &TaskbarController::updateFullscreenState);
    m_fullscreenTimer->start();
}

TaskbarController::~TaskbarController()
{
    restoreShell();
}

bool TaskbarController::hideTaskbar()
{
    const bool changed = setTaskbarVisible(false);
    if (changed) {
        emit shellActionLogged(QStringLiteral("Taskbar hidden."));
    }
    return changed;
}

bool TaskbarController::showTaskbar()
{
    const bool changed = setTaskbarVisible(true);
    if (changed) {
        emit shellActionLogged(QStringLiteral("Taskbar shown."));
    }
    return changed;
}

void TaskbarController::restoreShell()
{
    showTaskbar();
}

void TaskbarController::quitApplication()
{
    restoreShell();
    emit shellActionLogged(QStringLiteral("Quit requested. Restoring shell and exiting."));
    QMetaObject::invokeMethod(qApp, []() {
        QCoreApplication::quit();
    }, Qt::QueuedConnection);
}

bool TaskbarController::taskbarHidden() const
{
    return m_taskbarHidden;
}

bool TaskbarController::fullscreenAppActive() const
{
    return m_fullscreenAppActive;
}

bool TaskbarController::detectForegroundFullscreen() const
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return false;
    }
    if (hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return false;
    }

    // Ignore the desktop/shell windows.
    wchar_t classNameBuffer[256] = {};
    GetClassNameW(hwnd, classNameBuffer, 256);
    const QString className = QString::fromWCharArray(classNameBuffer);
    if (className == QStringLiteral("WorkerW")
        || className == QStringLiteral("Progman")
        || className == QStringLiteral("Shell_TrayWnd")) {
        return false;
    }

    RECT windowRect = {};
    if (!GetWindowRect(hwnd, &windowRect)) {
        return false;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return false;
    }

    const RECT& monitorRect = monitorInfo.rcMonitor;
    const bool coversMonitor = windowRect.left <= monitorRect.left
                            && windowRect.top <= monitorRect.top
                            && windowRect.right >= monitorRect.right
                            && windowRect.bottom >= monitorRect.bottom;
    if (!coversMonitor) {
        return false;
    }

    // Distinguish true fullscreen from a maximized window. A maximized window
    // reports IsZoomed()==true and keeps its window chrome; a fullscreen app
    // (game, fullscreen video, Electron/Chrome fullscreen) is a normal window
    // stretched to cover the whole monitor, so IsZoomed()==false.
    if (IsZoomed(hwnd)) {
        return false;
    }

    return true;
}

void TaskbarController::updateFullscreenState()
{
    const bool active = detectForegroundFullscreen();
    if (m_fullscreenAppActive == active) {
        return;
    }
    m_fullscreenAppActive = active;
    emit fullscreenAppActiveChanged();
    emit shellActionLogged(active
        ? QStringLiteral("Fullscreen app detected: hiding dock.")
        : QStringLiteral("Fullscreen app gone: showing dock."));
}

bool TaskbarController::setTaskbarVisible(bool visible)
{
    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (!taskbar) {
        emit shellActionLogged(QStringLiteral("Shell_TrayWnd not found."));
        return false;
    }

    ShowWindow(taskbar, visible ? SW_SHOW : SW_HIDE);
    EnableWindow(taskbar, visible ? TRUE : FALSE);

    HWND startButton = FindWindow(kStartButtonClass, nullptr);
    if (startButton) {
        ShowWindow(startButton, visible ? SW_SHOW : SW_HIDE);
        EnableWindow(startButton, visible ? TRUE : FALSE);
    }

    // Modify taskbar state to adjust desktop work area
    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = taskbar;

    if (!visible) {
        if (!m_hasOriginalTaskbarState) {
            m_originalTaskbarState = SHAppBarMessage(ABM_GETSTATE, &abd);
            m_hasOriginalTaskbarState = true;
            emit shellActionLogged(QString("Saved original taskbar state: %1").arg(m_originalTaskbarState));
        }
        
        abd.lParam = ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        emit shellActionLogged(QStringLiteral("Taskbar state set to ABS_AUTOHIDE."));
    } else {
        if (m_hasOriginalTaskbarState) {
            abd.lParam = m_originalTaskbarState;
            SHAppBarMessage(ABM_SETSTATE, &abd);
            m_hasOriginalTaskbarState = false;
            emit shellActionLogged(QString("Restored taskbar state: %1").arg(m_originalTaskbarState));
        }
    }

    const bool newHidden = !visible;
    if (m_taskbarHidden != newHidden) {
        m_taskbarHidden = newHidden;
        emit taskbarHiddenChanged();
    }

    return true;
}

bool TaskbarController::shellActive() const
{
    return m_shellActive;
}

void TaskbarController::setShellActive(bool active)
{
    if (m_shellActive == active)
        return;
    m_shellActive = active;
    emit shellActiveChanged();

    updateTaskbarVisibility();
}

bool TaskbarController::settingsVisible() const
{
    return m_settingsVisible;
}

void TaskbarController::setSettingsVisible(bool visible)
{
    if (m_settingsVisible == visible)
        return;
    m_settingsVisible = visible;
    emit settingsVisibleChanged();
}

int TaskbarController::dockIconSize() const
{
    return m_dockIconSize;
}

void TaskbarController::setDockIconSize(int size)
{
    if (m_dockIconSize == size)
        return;
    m_dockIconSize = size;
    emit dockIconSizeChanged();
}

bool TaskbarController::showTopBar() const
{
    return m_showTopBar;
}

void TaskbarController::setShowTopBar(bool show)
{
    if (m_showTopBar == show)
        return;
    m_showTopBar = show;
    emit showTopBarChanged();
}

bool TaskbarController::autoHideWindowsTaskbar() const
{
    return m_autoHideWindowsTaskbar;
}

void TaskbarController::setAutoHideWindowsTaskbar(bool autoHide)
{
    if (m_autoHideWindowsTaskbar == autoHide)
        return;
    m_autoHideWindowsTaskbar = autoHide;
    emit autoHideWindowsTaskbarChanged();

    updateTaskbarVisibility();
}

bool TaskbarController::dockHoverBounce() const
{
    return m_dockHoverBounce;
}

void TaskbarController::setDockHoverBounce(bool enabled)
{
    if (m_dockHoverBounce == enabled)
        return;
    m_dockHoverBounce = enabled;
    emit dockHoverBounceChanged();
}

bool TaskbarController::dockStaticIcons() const
{
    return m_dockStaticIcons;
}

void TaskbarController::setDockStaticIcons(bool enabled)
{
    if (m_dockStaticIcons == enabled)
        return;
    m_dockStaticIcons = enabled;
    emit dockStaticIconsChanged();
}

void TaskbarController::updateTaskbarVisibility()
{
    if (m_shellActive && m_autoHideWindowsTaskbar) {
        hideTaskbar();
    } else {
        restoreShell();
    }
}

void TaskbarController::apply(bool autoHideWindowsTaskbar, bool showTopBar, int iconSize, bool dockHoverBounce, bool dockStaticIcons)
{
    setAutoHideWindowsTaskbar(autoHideWindowsTaskbar);
    setShowTopBar(showTopBar);
    setDockIconSize(iconSize);
    setDockHoverBounce(dockHoverBounce);
    setDockStaticIcons(dockStaticIcons);
    setShellActive(true);
    setSettingsVisible(false);
}
