#include "TaskbarController.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QSettings>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>

#include <algorithm>

#include <windows.h>
#include <shellapi.h>

namespace {
constexpr auto kTaskbarClass = TEXT("Shell_TrayWnd");
constexpr auto kStartButtonClass = TEXT("Button");

QString executablePathFromHwnd(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return {};
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 4] = {};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    QString result;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size)) {
        result = QString::fromWCharArray(buffer, static_cast<int>(size));
    }
    CloseHandle(process);
    return result;
}

QString windowClassName(HWND hwnd)
{
    wchar_t classNameBuffer[256] = {};
    GetClassNameW(hwnd, classNameBuffer, 256);
    return QString::fromWCharArray(classNameBuffer);
}

bool isOwnProcessWindow(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid != 0 && pid == GetCurrentProcessId();
}

bool isDesktopForeground(HWND hwnd)
{
    if (!hwnd) {
        return true;
    }
    if (hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return true;
    }

    const QString className = windowClassName(hwnd);
    if (className == QStringLiteral("WorkerW") || className == QStringLiteral("Progman")) {
        return true;
    }

    const QString exePath = executablePathFromHwnd(hwnd);
    const QString exeName = QFileInfo(exePath).fileName().toLower();
    if (exeName == QStringLiteral("shellexperiencehost.exe")
        || exeName == QStringLiteral("searchhost.exe")
        || exeName == QStringLiteral("startmenuexperiencehost.exe")
        || exeName == QStringLiteral("textinputhost.exe")
        || exeName == QStringLiteral("lockapp.exe")) {
        return true;
    }

    if (exeName == QStringLiteral("explorer.exe")) {
        return className != QStringLiteral("CabinetWClass")
            && className != QStringLiteral("ExploreWClass");
    }

    return false;
}

QString foregroundAppName(HWND hwnd)
{
    const QString className = windowClassName(hwnd);
    const QString exePath = executablePathFromHwnd(hwnd);
    if (exePath.isEmpty()) {
        return {};
    }

    const QString exeName = QFileInfo(exePath).fileName().toLower();
    if (exeName == QStringLiteral("explorer.exe")) {
        if (className == QStringLiteral("CabinetWClass") || className == QStringLiteral("ExploreWClass")) {
            return QStringLiteral("File Explorer");
        }
        return {};
    }
    if (exeName == QStringLiteral("steam.exe")) {
        return QStringLiteral("Steam");
    }
    if (exeName == QStringLiteral("chrome.exe")) {
        return QStringLiteral("Chrome");
    }

    QString label = QFileInfo(exePath).completeBaseName();
    if (label.isEmpty()) {
        return {};
    }
    label[0] = label.at(0).toUpper();
    return label;
}

} // namespace

TaskbarController::TaskbarController(QObject* parent)
    : QObject(parent)
{
    // Poll the foreground window so the dock can auto-hide for fullscreen apps (macOS-style).
    m_fullscreenTimer = new QTimer(this);
    m_fullscreenTimer->setInterval(120);
    connect(m_fullscreenTimer, &QTimer::timeout, this, [this]() {
        updateForegroundAppName();
        updateFullscreenState();
    });
    m_fullscreenTimer->start();
    updateForegroundAppName();
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

bool TaskbarController::dockAutoHidden() const
{
    return m_dockAutoHidden;
}

bool TaskbarController::detectForegroundOccupiesScreen() const
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

    if (IsIconic(hwnd)) {
        return false;
    }

    // Maximized window: fills the screen like a macOS fullscreen space.
    if (IsZoomed(hwnd)) {
        return true;
    }

    // True fullscreen (game / fullscreen video / Electron fullscreen): a normal
    // window stretched to cover the whole monitor.
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
    return windowRect.left <= monitorRect.left
        && windowRect.top <= monitorRect.top
        && windowRect.right >= monitorRect.right
        && windowRect.bottom >= monitorRect.bottom;
}

void TaskbarController::updateForegroundAppName()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd && isOwnProcessWindow(hwnd)) {
        // Clicking the dock/top bar should not reset the label to Finder.
        return;
    }

    QString name = QStringLiteral("Finder");

    if (hwnd && !isDesktopForeground(hwnd)) {
        const QString resolved = foregroundAppName(hwnd);
        if (!resolved.isEmpty()) {
            name = resolved;
        }
    }

    if (m_menuBarAppName == name) {
        return;
    }
    m_menuBarAppName = name;
    emit menuBarAppNameChanged();
}

QString TaskbarController::menuBarAppName() const
{
    return m_menuBarAppName;
}

void TaskbarController::updateFullscreenState()
{
    const bool occupies = detectForegroundOccupiesScreen();

    // macOS-style reveal: while a fullscreen/maximized app is active, the dock
    // stays hidden but slides back up when the cursor reaches the bottom edge.
    bool revealed = false;
    if (occupies) {
        POINT cursor = {};
        if (GetCursorPos(&cursor)) {
            HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo = {};
            monitorInfo.cbSize = sizeof(monitorInfo);
            if (GetMonitorInfo(monitor, &monitorInfo)) {
                const int bottom = monitorInfo.rcMonitor.bottom;
                // Wider band keeps the dock up while the cursor is over it; a thin
                // edge triggers the reveal in the first place.
                const int band = m_dockIconSize + 70;
                revealed = m_dockRevealed ? (cursor.y >= bottom - band)
                                          : (cursor.y >= bottom - 2);
            }
        }
    }
    m_dockRevealed = revealed;

    const bool hidden = occupies && !revealed;
    if (m_dockAutoHidden == hidden) {
        return;
    }
    m_dockAutoHidden = hidden;
    emit dockAutoHiddenChanged();
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
