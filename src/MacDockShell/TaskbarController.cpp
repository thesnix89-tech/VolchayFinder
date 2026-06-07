#include "TaskbarController.h"

#include <QCoreApplication>
#include <QDir>
#include <QMetaObject>
#include <QSettings>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>

#include <algorithm>
#include <string>

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

QString normalizeMenuLabel(const QString& raw)
{
    QString label = raw;
    const int tabIndex = label.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0) {
        label = label.left(tabIndex);
    }
    label.remove(QLatin1Char('&'));
    return label.trimmed();
}

QStringList defaultMenuBarItems()
{
    const LANGID language = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(language) == LANG_RUSSIAN) {
        return {
            QStringLiteral("Файл"),
            QStringLiteral("Правка"),
            QStringLiteral("Вид"),
            QStringLiteral("Переход"),
            QStringLiteral("Окно"),
            QStringLiteral("Справка"),
        };
    }

    return {
        QStringLiteral("File"),
        QStringLiteral("Edit"),
        QStringLiteral("View"),
        QStringLiteral("Go"),
        QStringLiteral("Window"),
        QStringLiteral("Help"),
    };
}

QStringList readMenuBarItems(HWND hwnd)
{
    if (!hwnd) {
        return {};
    }

    const HWND root = GetAncestor(hwnd, GA_ROOT);
    HMENU menu = GetMenu(root);
    if (!menu) {
        menu = GetMenu(hwnd);
    }
    if (!menu) {
        return {};
    }

    const int count = GetMenuItemCount(menu);
    if (count <= 0) {
        return {};
    }

    QStringList items;
    items.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int length = GetMenuStringW(menu, static_cast<UINT>(i), nullptr, 0, MF_BYPOSITION);
        if (length <= 0) {
            continue;
        }

        std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
        GetMenuStringW(menu, static_cast<UINT>(i), buffer.data(), length + 1, MF_BYPOSITION);

        const QString label = normalizeMenuLabel(QString::fromWCharArray(buffer.c_str()));
        if (!label.isEmpty()) {
            items.push_back(label);
        }
    }

    return items;
}

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"MacDockShell";

QString expectedAutostartCommand()
{
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QStringLiteral("\"%1\" --autostart").arg(exePath);
}

bool readWindowsRunCommand(QString* commandOut)
{
    HKEY runKey = nullptr;
    const LSTATUS openStatus = RegOpenKeyExW(HKEY_CURRENT_USER,
                                             kRunKeyPath,
                                             0,
                                             KEY_QUERY_VALUE,
                                             &runKey);
    if (openStatus != ERROR_SUCCESS) {
        return false;
    }

    wchar_t buffer[1024] = {};
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    DWORD type = 0;
    const LSTATUS queryStatus = RegQueryValueExW(runKey,
                                                 kRunValueName,
                                                 nullptr,
                                                 &type,
                                                 reinterpret_cast<BYTE*>(buffer),
                                                 &size);
    RegCloseKey(runKey);

    if (queryStatus != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }

    if (commandOut) {
        *commandOut = QString::fromWCharArray(buffer);
    }
    return true;
}

bool setWindowsRunAtStartup(bool enabled)
{
    HKEY runKey = nullptr;
    const REGSAM access = KEY_SET_VALUE | KEY_QUERY_VALUE;
    const LSTATUS openStatus = RegOpenKeyExW(HKEY_CURRENT_USER,
                                             kRunKeyPath,
                                             0,
                                             access,
                                             &runKey);
    if (openStatus != ERROR_SUCCESS) {
        return false;
    }

    if (!enabled) {
        RegDeleteValueW(runKey, kRunValueName);
        RegCloseKey(runKey);
        return true;
    }

    const std::wstring nativeCommand = expectedAutostartCommand().toStdWString();

    const LSTATUS setStatus = RegSetValueExW(runKey,
                                             kRunValueName,
                                             0,
                                             REG_SZ,
                                             reinterpret_cast<const BYTE*>(nativeCommand.c_str()),
                                             static_cast<DWORD>((nativeCommand.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(runKey);
    return setStatus == ERROR_SUCCESS;
}

} // namespace

TaskbarController::TaskbarController(QObject* parent)
    : QObject(parent)
{
    // Poll the foreground window so the dock can auto-hide for fullscreen apps (macOS-style).
    m_fullscreenTimer = new QTimer(this);
    m_fullscreenTimer->setInterval(120);
    connect(m_fullscreenTimer, &QTimer::timeout, this, [this]() {
        updateForegroundMenuBar();
        updateFullscreenState();
        enforceTaskbarHidden();
    });
    m_fullscreenTimer->start();
    m_menuBarItems = defaultMenuBarItems();
    loadSettings();
    updateForegroundMenuBar();
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

void TaskbarController::updateForegroundMenuBar()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd && isOwnProcessWindow(hwnd)) {
        // Clicking the dock/top bar should not reset the label to Finder.
        return;
    }

    QString name = QStringLiteral("Finder");
    QStringList items = defaultMenuBarItems();

    if (hwnd && !isDesktopForeground(hwnd)) {
        const QString resolved = foregroundAppName(hwnd);
        if (!resolved.isEmpty()) {
            name = resolved;
        }

        const QStringList windowItems = readMenuBarItems(hwnd);
        if (!windowItems.isEmpty()) {
            items = windowItems;
        }
    }

    if (m_menuBarAppName != name) {
        m_menuBarAppName = name;
        emit menuBarAppNameChanged();
    }
    if (m_menuBarItems != items) {
        m_menuBarItems = items;
        emit menuBarItemsChanged();
    }
}

QString TaskbarController::menuBarAppName() const
{
    return m_menuBarAppName;
}

QStringList TaskbarController::menuBarItems() const
{
    return m_menuBarItems;
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

void TaskbarController::enforceTaskbarHidden()
{
    if (!m_shellActive || !m_autoHideWindowsTaskbar) {
        return;
    }

    bool restored = false;

    auto hideIfVisible = [&](HWND hwnd) {
        if (!hwnd || !IsWindowVisible(hwnd)) {
            return;
        }
        ShowWindow(hwnd, SW_HIDE);
        EnableWindow(hwnd, FALSE);
        restored = true;
    };

    hideIfVisible(FindWindow(kTaskbarClass, nullptr));

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t className[256] = {};
        if (GetClassNameW(hwnd, className, 256) == 0) {
            return TRUE;
        }
        if (wcscmp(className, L"Shell_SecondaryTrayWnd") != 0) {
            return TRUE;
        }
        if (!IsWindowVisible(hwnd)) {
            return TRUE;
        }
        ShowWindow(hwnd, SW_HIDE);
        EnableWindow(hwnd, FALSE);
        *reinterpret_cast<bool*>(lParam) = true;
        return TRUE;
    }, reinterpret_cast<LPARAM>(&restored));

    HWND startButton = FindWindow(kStartButtonClass, nullptr);
    hideIfVisible(startButton);

    if (!restored) {
        return;
    }

    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (taskbar) {
        APPBARDATA abd = {};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = taskbar;
        abd.lParam = ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
    }
    if (!m_taskbarHidden) {
        m_taskbarHidden = true;
        emit taskbarHiddenChanged();
    }

    emit shellActionLogged(QStringLiteral("Taskbar re-hidden after Windows shell interruption."));
    emit shellLayoutRestoreNeeded();
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

bool TaskbarController::darkTheme() const
{
    return m_darkTheme;
}

void TaskbarController::setDarkTheme(bool enabled)
{
    if (m_darkTheme == enabled) {
        return;
    }
    m_darkTheme = enabled;
    emit darkThemeChanged();
}

bool TaskbarController::startWithWindows() const
{
    return m_startWithWindows;
}

void TaskbarController::setStartWithWindows(bool enabled)
{
    if (m_startWithWindows == enabled) {
        return;
    }
    m_startWithWindows = enabled;
    emit startWithWindowsChanged();
}

void TaskbarController::syncWindowsStartup(bool enabled)
{
    if (setWindowsRunAtStartup(enabled)) {
        emit shellActionLogged(enabled
            ? QStringLiteral("Windows autostart enabled.")
            : QStringLiteral("Windows autostart disabled."));
    } else {
        emit shellActionLogged(QStringLiteral("Failed to update Windows autostart."));
    }
}

void TaskbarController::reconcileWindowsStartup()
{
    QString registeredCommand;
    const bool registered = readWindowsRunCommand(&registeredCommand);
    const QString expected = expectedAutostartCommand();

    if (m_startWithWindows) {
        if (!registered || registeredCommand.compare(expected, Qt::CaseInsensitive) != 0) {
            syncWindowsStartup(true);
        }
        return;
    }

    if (registered) {
        syncWindowsStartup(false);
        emit shellActionLogged(QStringLiteral("Removed stale Windows autostart entry."));
    }
}

void TaskbarController::tryAutostartShell()
{
    if (!m_startWithWindows) {
        return;
    }

    setSettingsVisible(false);
    setShellActive(true);
    updateTaskbarVisibility();
    emit shellActionLogged(QStringLiteral("Autostart: shell activated."));
}

QString TaskbarController::explorerIconStyle() const
{
    return m_explorerIconStyle;
}

void TaskbarController::setExplorerIconStyle(const QString& style)
{
    const QString normalized = style == QStringLiteral("macos")
            ? QStringLiteral("macos")
            : QStringLiteral("default");
    if (m_explorerIconStyle == normalized) {
        return;
    }
    m_explorerIconStyle = normalized;
    emit explorerIconStyleChanged();
}

void TaskbarController::loadSettings()
{
    QSettings settings;
    m_autoHideWindowsTaskbar = settings.value(QStringLiteral("shell/autoHideTaskbar"), true).toBool();
    m_showTopBar = settings.value(QStringLiteral("shell/showTopBar"), true).toBool();
    m_dockIconSize = settings.value(QStringLiteral("shell/dockIconSize"), 54).toInt();
    m_dockHoverBounce = settings.value(QStringLiteral("shell/dockHoverBounce"), true).toBool();
    m_dockStaticIcons = settings.value(QStringLiteral("shell/dockStaticIcons"), false).toBool();
    m_darkTheme = settings.value(QStringLiteral("shell/darkTheme"), false).toBool();
    m_startWithWindows = settings.value(QStringLiteral("shell/startWithWindows"), false).toBool();
    setExplorerIconStyle(settings.value(QStringLiteral("shell/explorerIconStyle"), QStringLiteral("default")).toString());
    reconcileWindowsStartup();
}

void TaskbarController::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("shell/autoHideTaskbar"), m_autoHideWindowsTaskbar);
    settings.setValue(QStringLiteral("shell/showTopBar"), m_showTopBar);
    settings.setValue(QStringLiteral("shell/dockIconSize"), m_dockIconSize);
    settings.setValue(QStringLiteral("shell/dockHoverBounce"), m_dockHoverBounce);
    settings.setValue(QStringLiteral("shell/dockStaticIcons"), m_dockStaticIcons);
    settings.setValue(QStringLiteral("shell/darkTheme"), m_darkTheme);
    settings.setValue(QStringLiteral("shell/startWithWindows"), m_startWithWindows);
    settings.setValue(QStringLiteral("shell/explorerIconStyle"), m_explorerIconStyle);
}

void TaskbarController::updateTaskbarVisibility()
{
    if (m_shellActive && m_autoHideWindowsTaskbar) {
        hideTaskbar();
    } else {
        restoreShell();
    }
}

void TaskbarController::apply(bool autoHideWindowsTaskbar, bool showTopBar, int iconSize, bool dockHoverBounce, bool dockStaticIcons, bool darkTheme, bool startWithWindows, const QString& explorerIconStyle)
{
    setAutoHideWindowsTaskbar(autoHideWindowsTaskbar);
    setShowTopBar(showTopBar);
    setDockIconSize(iconSize);
    setDockHoverBounce(dockHoverBounce);
    setDockStaticIcons(dockStaticIcons);
    setDarkTheme(darkTheme);
    setStartWithWindows(startWithWindows);
    setExplorerIconStyle(explorerIconStyle);
    syncWindowsStartup(startWithWindows);
    saveSettings();
    setShellActive(true);
    setSettingsVisible(false);
}
