#include "TaskbarController.h"

#include <QCoreApplication>
#include <QMetaObject>

#include <windows.h>
#include <shellapi.h>

namespace {
constexpr auto kTaskbarClass = TEXT("Shell_TrayWnd");
constexpr auto kStartButtonClass = TEXT("Button");
}

TaskbarController::TaskbarController(QObject* parent)
    : QObject(parent)
{
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

    if (m_shellActive) {
        hideTaskbar();
    } else {
        restoreShell();
    }
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

void TaskbarController::apply(bool showTopBar, int iconSize)
{
    setShowTopBar(showTopBar);
    setDockIconSize(iconSize);
    setShellActive(true);
    setSettingsVisible(false);
}
