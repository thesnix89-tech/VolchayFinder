#include "TaskbarController.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QSettings>
#include <QProcess>

#include <windows.h>
#include <shellapi.h>

namespace {
constexpr auto kTaskbarClass = TEXT("Shell_TrayWnd");
constexpr auto kStartButtonClass = TEXT("Button");
}

TaskbarController::TaskbarController(QObject* parent)
    : QObject(parent)
{
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Colors"), QSettings::NativeFormat);
    QSettings advSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced"), QSettings::NativeFormat);
    const QString hilight = settings.value(QStringLiteral("Hilight")).toString();
    const QString hotTracking = settings.value(QStringLiteral("HotTrackingColor")).toString();
    const int alpha = advSettings.value(QStringLiteral("ListviewAlphaSelect"), 1).toInt();
    if (hilight == QStringLiteral("255 255 255") && hotTracking == QStringLiteral("255 255 255") && alpha == 0) {
        m_macOSSelectionStyle = true;
    }
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

void TaskbarController::updateTaskbarVisibility()
{
    if (m_shellActive && m_autoHideWindowsTaskbar) {
        hideTaskbar();
    } else {
        restoreShell();
    }
}

void TaskbarController::apply(bool autoHideWindowsTaskbar, bool showTopBar, bool macOSSelectionStyle, int iconSize)
{
    setAutoHideWindowsTaskbar(autoHideWindowsTaskbar);
    setShowTopBar(showTopBar);
    setMacOSSelectionStyle(macOSSelectionStyle);
    setDockIconSize(iconSize);
    setShellActive(true);
    setSettingsVisible(false);
}

bool TaskbarController::macOSSelectionStyle() const
{
    return m_macOSSelectionStyle;
}

void TaskbarController::setMacOSSelectionStyle(bool enable)
{
    if (m_macOSSelectionStyle == enable)
        return;
    m_macOSSelectionStyle = enable;
    emit macOSSelectionStyleChanged();

    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Colors"), QSettings::NativeFormat);
    QSettings advSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced"), QSettings::NativeFormat);
    if (enable) {
        settings.setValue(QStringLiteral("Hilight"), QStringLiteral("255 255 255"));
        settings.setValue(QStringLiteral("HotTrackingColor"), QStringLiteral("255 255 255"));
        advSettings.setValue(QStringLiteral("ListviewAlphaSelect"), 0);
    } else {
        settings.setValue(QStringLiteral("Hilight"), QStringLiteral("0 120 212"));
        settings.setValue(QStringLiteral("HotTrackingColor"), QStringLiteral("0 102 204"));
        advSettings.setValue(QStringLiteral("ListviewAlphaSelect"), 1);
    }

    PostMessageW(HWND_BROADCAST, WM_SYSCOLORCHANGE, 0, 0);

    // Restart explorer.exe to reload registry colors immediately
    QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("taskkill /f /im explorer.exe && start explorer.exe")});
}

void TaskbarController::restartExplorer()
{
    emit shellActionLogged(QStringLiteral("Manually restarting explorer.exe"));
    QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("taskkill /f /im explorer.exe && start explorer.exe")});
}
