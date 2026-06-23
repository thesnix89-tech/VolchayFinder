#include "TaskbarController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QSettings>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <cstring>
#include <string>

#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shldisp.h>

namespace {
constexpr auto kTaskbarClass = TEXT("Shell_TrayWnd");
constexpr auto kStartButtonClass = TEXT("Button");
constexpr int kOffscreenTaskbarX = -20000;
constexpr int kOffscreenTaskbarY = -20000;
constexpr int kStuckRectsAutoHideByteIndex = 8;

constexpr wchar_t kStuckRects3Path[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3";
constexpr wchar_t kStuckRects2Path[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects2";

bool readStuckRectsSettings(const wchar_t* subkeyPath, QByteArray* out)
{
    if (!out) {
        return false;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subkeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    const LSTATUS sizeStatus = RegQueryValueExW(key, L"Settings", nullptr, &type, nullptr, &size);
    if (sizeStatus != ERROR_SUCCESS || type != REG_BINARY
        || size <= static_cast<DWORD>(kStuckRectsAutoHideByteIndex)) {
        RegCloseKey(key);
        return false;
    }

    out->resize(static_cast<int>(size));
    DWORD readSize = size;
    const LSTATUS readStatus = RegQueryValueExW(
        key, L"Settings", nullptr, &type,
        reinterpret_cast<LPBYTE>(out->data()), &readSize);
    RegCloseKey(key);
    return readStatus == ERROR_SUCCESS
        && readSize > static_cast<DWORD>(kStuckRectsAutoHideByteIndex);
}

bool writeStuckRectsSettings(const wchar_t* subkeyPath, const QByteArray& bytes)
{
    if (bytes.size() <= kStuckRectsAutoHideByteIndex) {
        return false;
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subkeyPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    const LSTATUS writeStatus = RegSetValueExW(
        key, L"Settings", 0, REG_BINARY,
        reinterpret_cast<const BYTE*>(bytes.constData()),
        static_cast<DWORD>(bytes.size()));
    RegCloseKey(key);
    return writeStatus == ERROR_SUCCESS;
}

quint8 autoHideEnabledRegistryByte(quint8 reference)
{
    if (reference == 122 || reference == 123 || (reference >= 120 && reference <= 130)) {
        return 123;
    }
    if (reference == 0x22) {
        return 0x02;
    }
    if (reference == 2 || reference == 3) {
        return 2;
    }
    return 2;
}

quint8 autoHideDisabledRegistryByte(quint8 reference)
{
    if (reference == 122 || reference == 123 || (reference >= 120 && reference <= 130)) {
        return 122;
    }
    if (reference == 0x02 || reference == 0x22) {
        return 0x22;
    }
    if (reference == 2 || reference == 3) {
        return 3;
    }
    return 3;
}

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

bool isCloakedWindow(HWND hwnd)
{
    BOOL cloaked = FALSE;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
        && cloaked;
}

bool isDesktopPeekCandidate(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return false;
    }
    if (isOwnProcessWindow(hwnd) || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return false;
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return false;
    }
    if (isCloakedWindow(hwnd)) {
        return false;
    }

    const LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return false;
    }

    const QString className = windowClassName(hwnd);
    if (className == QStringLiteral("WorkerW")
        || className == QStringLiteral("Progman")
        || className == QStringLiteral("Shell_TrayWnd")
        || className == QStringLiteral("Shell_SecondaryTrayWnd")) {
        return false;
    }

    const QString exePath = executablePathFromHwnd(hwnd);
    const QString exeName = QFileInfo(exePath).fileName().toLower();
    if (exePath.isEmpty()
        || exeName == QStringLiteral("shellexperiencehost.exe")
        || exeName == QStringLiteral("searchhost.exe")
        || exeName == QStringLiteral("startmenuexperiencehost.exe")
        || exeName == QStringLiteral("textinputhost.exe")
        || exeName == QStringLiteral("lockapp.exe")) {
        return false;
    }

    if (exeName == QStringLiteral("explorer.exe")) {
        return className == QStringLiteral("CabinetWClass")
            || className == QStringLiteral("ExploreWClass");
    }

    return true;
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

QString pickMenuBarIconFile(const QString& dialogTitle)
{
    wchar_t buffer[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Images\0*.png;*.svg;*.ico;*.jpg;*.jpeg;*.bmp;*.webp\0All Files\0*.*\0\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    const std::wstring nativeTitle = dialogTitle.toStdWString();
    ofn.lpstrTitle = nativeTitle.c_str();

    if (!GetOpenFileNameW(&ofn)) {
        return {};
    }
    return QString::fromWCharArray(buffer);
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
    m_appearanceTimer = new QTimer(this);
    m_appearanceTimer->setInterval(2000);
    connect(m_appearanceTimer, &QTimer::timeout, this, &TaskbarController::updateEffectiveAppearance);
    m_appearanceTimer->start();
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
        emit shellActionLogged(QStringLiteral("Taskbar relocated off-screen."));
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

void TaskbarController::capturePreShellTaskbarState()
{
    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (!taskbar) {
        return;
    }

    if (!m_hasOriginalTaskbarState) {
        APPBARDATA abd = {};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = taskbar;
        m_originalTaskbarState = SHAppBarMessage(ABM_GETSTATE, &abd);
        m_hasOriginalTaskbarState = true;
        emit shellActionLogged(QStringLiteral("Saved original taskbar AppBar state: %1")
                                   .arg(m_originalTaskbarState));
    }

    if (!m_hasOriginalStuckRectsSettings) {
        QByteArray settings;
        if (readStuckRectsSettings(kStuckRects3Path, &settings)) {
            m_stuckRectsRegPath = QStringLiteral("StuckRects3");
        } else if (readStuckRectsSettings(kStuckRects2Path, &settings)) {
            m_stuckRectsRegPath = QStringLiteral("StuckRects2");
        }

        if (!settings.isEmpty()) {
            m_originalStuckRectsSettings = settings;
            m_hasOriginalStuckRectsSettings = true;
            emit shellActionLogged(QStringLiteral("Saved original taskbar registry auto-hide byte: %1")
                                       .arg(static_cast<quint8>(
                                           m_originalStuckRectsSettings[kStuckRectsAutoHideByteIndex])));
        }
    }
}

void TaskbarController::showTaskbarWindows()
{
    restoreAllTaskbarPositions();

    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (!taskbar) {
        emit shellActionLogged(QStringLiteral("Shell_TrayWnd not found."));
        return;
    }

    ShowWindow(taskbar, SW_SHOW);
    EnableWindow(taskbar, TRUE);

    HWND startButton = FindWindow(kStartButtonClass, nullptr);
    if (startButton) {
        ShowWindow(startButton, SW_SHOW);
        EnableWindow(startButton, TRUE);
    }
}

void TaskbarController::relocateWindowOffScreen(quintptr hwndValue, bool force)
{
    if (!force && m_trayUiaBusy.load()) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(hwndValue);
    if (!hwnd) {
        return;
    }

    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) {
        return;
    }

    const quintptr key = reinterpret_cast<quintptr>(hwnd);
    if (!m_savedTaskbarRects.contains(key)) {
        m_savedTaskbarRects.insert(key, QRect(rect.left, rect.top,
                                              rect.right - rect.left,
                                              rect.bottom - rect.top));
    }

    ShowWindow(hwnd, SW_SHOW);
    EnableWindow(hwnd, TRUE);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetWindowPos(hwnd, HWND_BOTTOM, kOffscreenTaskbarX, kOffscreenTaskbarY,
                 width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TaskbarController::restoreWindowPosition(quintptr hwndValue)
{
    HWND hwnd = reinterpret_cast<HWND>(hwndValue);
    if (!hwnd) {
        return;
    }

    const quintptr key = reinterpret_cast<quintptr>(hwnd);
    const auto it = m_savedTaskbarRects.constFind(key);
    if (it == m_savedTaskbarRects.constEnd()) {
        ShowWindow(hwnd, SW_SHOW);
        EnableWindow(hwnd, TRUE);
        return;
    }

    const QRect saved = it.value();
    SetWindowPos(hwnd, HWND_TOP, saved.x(), saved.y(), saved.width(), saved.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_savedTaskbarRects.erase(it);
}

void TaskbarController::restoreAllTaskbarPositions()
{
    const QList<quintptr> keys = m_savedTaskbarRects.keys();
    for (quintptr key : keys) {
        restoreWindowPosition(key);
    }
    m_savedTaskbarRects.clear();
}

void TaskbarController::beginTrayOnScreenScope()
{
    m_trayScopeWasOffscreen = false;

    const auto restoreTrayIfOffscreen = [this](HWND hwnd) {
        if (!hwnd) {
            return;
        }

        RECT rect = {};
        if (!GetWindowRect(hwnd, &rect)) {
            return;
        }
        if (rect.left >= -1000 && rect.top >= -1000) {
            return;
        }

        m_trayScopeWasOffscreen = true;
        restoreWindowPosition(reinterpret_cast<quintptr>(hwnd));
        ShowWindow(hwnd, SW_SHOW);
        EnableWindow(hwnd, TRUE);
    };

    restoreTrayIfOffscreen(FindWindow(kTaskbarClass, nullptr));

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t className[256] = {};
        if (GetClassNameW(hwnd, className, 256) == 0) {
            return TRUE;
        }
        if (wcscmp(className, L"Shell_SecondaryTrayWnd") != 0) {
            return TRUE;
        }

        auto* controller = reinterpret_cast<TaskbarController*>(lParam);
        RECT rect = {};
        if (!GetWindowRect(hwnd, &rect)) {
            return TRUE;
        }
        if (rect.left >= -1000 && rect.top >= -1000) {
            return TRUE;
        }

        controller->m_trayScopeWasOffscreen = true;
        controller->restoreWindowPosition(reinterpret_cast<quintptr>(hwnd));
        ShowWindow(hwnd, SW_SHOW);
        EnableWindow(hwnd, TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));

    if (m_trayScopeWasOffscreen) {
        Sleep(500);
    }
}

void TaskbarController::endTrayOnScreenScope()
{
    if (!m_trayScopeWasOffscreen || !m_shellActive || !m_autoHideWindowsTaskbar) {
        return;
    }

    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (taskbar) {
        relocateWindowOffScreen(reinterpret_cast<quintptr>(taskbar), true);
    }

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* controller = reinterpret_cast<TaskbarController*>(lParam);
        wchar_t className[256] = {};
        if (GetClassNameW(hwnd, className, 256) == 0) {
            return TRUE;
        }
        if (wcscmp(className, L"Shell_SecondaryTrayWnd") != 0) {
            return TRUE;
        }

        controller->relocateWindowOffScreen(reinterpret_cast<quintptr>(hwnd), true);
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
}

void TaskbarController::withTrayOnScreen(const std::function<void()>& action)
{
    if (!action) {
        return;
    }

    beginTrayOnScreenScope();
    action();
    endTrayOnScreenScope();
}

void TaskbarController::withTrayOnScreenOnGuiThread(const std::function<void()>& action)
{
    if (!action) {
        return;
    }

    const auto runOnGui = [this](const std::function<void()>& fn) {
        if (QThread::currentThread() == thread()) {
            fn();
            return;
        }

        const std::function<void()> fnCopy = fn;
        QMetaObject::invokeMethod(this, [fnCopy]() {
            fnCopy();
        }, Qt::BlockingQueuedConnection);
    };

    runOnGui([this]() { beginTrayOnScreenScope(); });
    action();
    runOnGui([this]() { endTrayOnScreenScope(); });
}

void TaskbarController::setTrayUiaBusy(bool busy)
{
    m_trayUiaBusy.store(busy);
}

bool TaskbarController::trayUiaBusy() const
{
    return m_trayUiaBusy.load();
}

void TaskbarController::restoreTaskbarRegistrySettings()
{
    if (!m_hasOriginalStuckRectsSettings || m_stuckRectsRegPath.isEmpty()) {
        return;
    }

    const wchar_t* subkeyPath = m_stuckRectsRegPath == QStringLiteral("StuckRects2")
        ? kStuckRects2Path
        : kStuckRects3Path;
    if (writeStuckRectsSettings(subkeyPath, m_originalStuckRectsSettings)) {
        emit shellActionLogged(QStringLiteral("Restored taskbar registry Settings blob."));
    } else {
        emit shellActionLogged(QStringLiteral("Failed to restore taskbar registry Settings blob."));
    }

    m_hasOriginalStuckRectsSettings = false;
    m_originalStuckRectsSettings.clear();
    m_stuckRectsRegPath.clear();
}

void TaskbarController::setTaskbarRegistryAutoHide(bool enabled)
{
    const wchar_t* subkeyPath = nullptr;
    QByteArray settings;
    if (readStuckRectsSettings(kStuckRects3Path, &settings)) {
        subkeyPath = kStuckRects3Path;
    } else if (readStuckRectsSettings(kStuckRects2Path, &settings)) {
        subkeyPath = kStuckRects2Path;
    }

    if (!subkeyPath || settings.isEmpty()) {
        emit shellActionLogged(QStringLiteral("Taskbar registry Settings blob not found."));
        return;
    }

    const quint8 reference = m_hasOriginalStuckRectsSettings
        ? static_cast<quint8>(m_originalStuckRectsSettings[kStuckRectsAutoHideByteIndex])
        : static_cast<quint8>(settings[kStuckRectsAutoHideByteIndex]);
    settings[kStuckRectsAutoHideByteIndex] = static_cast<char>(
        enabled ? autoHideEnabledRegistryByte(reference)
                : autoHideDisabledRegistryByte(reference));

    if (writeStuckRectsSettings(subkeyPath, settings)) {
        emit shellActionLogged(QStringLiteral("Taskbar registry auto-hide set to %1.")
                                   .arg(enabled ? QStringLiteral("enabled")
                                                : QStringLiteral("disabled")));
    } else {
        emit shellActionLogged(QStringLiteral("Failed to update taskbar registry auto-hide."));
    }
}

void TaskbarController::restoreShell()
{
    restoreAllTaskbarPositions();

    if (m_keepTaskbarAutoHideOnExit) {
        showTaskbarWindows();

        HWND taskbar = FindWindow(kTaskbarClass, nullptr);
        if (taskbar) {
            APPBARDATA abd = {};
            abd.cbSize = sizeof(APPBARDATA);
            abd.hWnd = taskbar;
            abd.lParam = ABS_AUTOHIDE;
            SHAppBarMessage(ABM_SETSTATE, &abd);
        }

        setTaskbarRegistryAutoHide(true);
        m_hasOriginalTaskbarState = false;
        m_hasOriginalStuckRectsSettings = false;
        m_originalStuckRectsSettings.clear();
        m_stuckRectsRegPath.clear();
        m_taskbarShellModified = false;

        if (m_taskbarHidden) {
            m_taskbarHidden = false;
            emit taskbarHiddenChanged();
        }

        emit shellActionLogged(QStringLiteral("Exit: keeping Windows taskbar auto-hide enabled."));
        return;
    }

    showTaskbarWindows();

    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (taskbar) {
        APPBARDATA abd = {};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = taskbar;

        if (m_hasOriginalTaskbarState) {
            abd.lParam = m_originalTaskbarState;
            SHAppBarMessage(ABM_SETSTATE, &abd);
            emit shellActionLogged(QStringLiteral("Restored taskbar AppBar state: %1")
                                       .arg(m_originalTaskbarState));
            m_hasOriginalTaskbarState = false;
        } else if (m_taskbarShellModified) {
            abd.lParam = ABS_ALWAYSONTOP;
            SHAppBarMessage(ABM_SETSTATE, &abd);
            emit shellActionLogged(QStringLiteral("Cleared taskbar AppBar auto-hide state."));
        }
    }

    if (m_hasOriginalStuckRectsSettings) {
        restoreTaskbarRegistrySettings();
    } else if (m_taskbarShellModified) {
        setTaskbarRegistryAutoHide(false);
    }

    m_taskbarShellModified = false;

    if (m_taskbarHidden) {
        m_taskbarHidden = false;
        emit taskbarHiddenChanged();
    }
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

bool TaskbarController::showDesktopActive() const
{
    return m_showDesktopActive;
}

QVector<TaskbarController::DesktopPeekWindow> TaskbarController::collectDesktopPeekWindows() const
{
    QVector<DesktopPeekWindow> windows;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* result = reinterpret_cast<QVector<DesktopPeekWindow>*>(lParam);
        if (!result || !isDesktopPeekCandidate(hwnd)) {
            return TRUE;
        }

        WINDOWPLACEMENT placement = {};
        placement.length = sizeof(placement);
        if (!GetWindowPlacement(hwnd, &placement)) {
            return TRUE;
        }

        DesktopPeekWindow item;
        item.hwndValue = reinterpret_cast<quintptr>(hwnd);
        item.placement.resize(sizeof(WINDOWPLACEMENT));
        std::memcpy(item.placement.data(), &placement, sizeof(WINDOWPLACEMENT));
        result->push_back(item);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

bool TaskbarController::minimizeDesktopPeekWindows(bool persistent)
{
    QVector<DesktopPeekWindow> windows = collectDesktopPeekWindows();
    if (windows.isEmpty()) {
        return false;
    }

    if (!m_showDesktopActive) {
        m_desktopPeekWindows = windows;
        m_desktopPeekForeground = reinterpret_cast<quintptr>(GetForegroundWindow());
    }

    for (const DesktopPeekWindow& item : windows) {
        HWND hwnd = reinterpret_cast<HWND>(item.hwndValue);
        if (hwnd && IsWindow(hwnd) && !IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
    }

    return true;
}

void TaskbarController::restoreDesktopPeekWindows()
{
    QVector<DesktopPeekWindow> windows = m_desktopPeekWindows;
    m_desktopPeekWindows.clear();

    for (const DesktopPeekWindow& item : windows) {
        HWND hwnd = reinterpret_cast<HWND>(item.hwndValue);
        if (!hwnd || !IsWindow(hwnd) || item.placement.size() != sizeof(WINDOWPLACEMENT)) {
            continue;
        }

        WINDOWPLACEMENT placement = {};
        std::memcpy(&placement, item.placement.constData(), sizeof(WINDOWPLACEMENT));
        placement.length = sizeof(placement);
        SetWindowPlacement(hwnd, &placement);
        if (placement.showCmd == SW_SHOWMAXIMIZED
            || (placement.flags & WPF_RESTORETOMAXIMIZED)) {
            ShowWindow(hwnd, SW_MAXIMIZE);
        } else {
            ShowWindow(hwnd, SW_RESTORE);
        }
    }

    HWND foreground = reinterpret_cast<HWND>(m_desktopPeekForeground);
    if (foreground && IsWindow(foreground) && !isOwnProcessWindow(foreground)) {
        SetForegroundWindow(foreground);
    }
    m_desktopPeekForeground = 0;
}

bool TaskbarController::tryShellToggleDesktop()
{
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(init);
    if (init == RPC_E_CHANGED_MODE) {
        init = S_OK;
    }
    if (FAILED(init)) {
        return false;
    }

    IShellDispatch4* shell = nullptr;
    const HRESULT created = CoCreateInstance(CLSID_Shell,
                                             nullptr,
                                             CLSCTX_INPROC_SERVER,
                                             IID_PPV_ARGS(&shell));
    if (FAILED(created) || !shell) {
        if (shouldUninitialize) {
            CoUninitialize();
        }
        return false;
    }

    const HRESULT toggled = shell->ToggleDesktop();
    shell->Release();
    if (shouldUninitialize) {
        CoUninitialize();
    }
    return SUCCEEDED(toggled);
}

bool TaskbarController::toggleShowDesktop()
{
    const bool wasShowDesktopActive = m_showDesktopActive;
    bool ok = tryShellToggleDesktop();
    if (!ok) {
        if (m_showDesktopActive) {
            restoreDesktopPeekWindows();
            ok = true;
        } else {
            ok = minimizeDesktopPeekWindows(true);
        }
    }

    if (ok) {
        m_showDesktopActive = !m_showDesktopActive;
        if (wasShowDesktopActive && !m_showDesktopActive) {
            m_desktopPeekWindows.clear();
            m_desktopPeekForeground = 0;
        }
        emit showDesktopActiveChanged();
        emit shellActionLogged(m_showDesktopActive
            ? QStringLiteral("Show desktop enabled.")
            : QStringLiteral("Show desktop restored."));
    } else {
        emit shellActionLogged(QStringLiteral("Show desktop request failed."));
    }
    return ok;
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
    if (!m_shellActive || !m_autoHideWindowsTaskbar || m_trayUiaBusy.load()) {
        return;
    }

    bool restored = false;

    auto relocateIfOnScreen = [&](HWND hwnd) {
        if (!hwnd) {
            return;
        }
        RECT rect = {};
        if (!GetWindowRect(hwnd, &rect)) {
            return;
        }
        if (rect.left >= -1000 && rect.top >= -1000) {
            relocateWindowOffScreen(reinterpret_cast<quintptr>(hwnd));
            restored = true;
        }
    };

    relocateIfOnScreen(FindWindow(kTaskbarClass, nullptr));

    struct SecondaryTrayContext {
        TaskbarController* controller = nullptr;
        bool* restored = nullptr;
    };
    SecondaryTrayContext secondaryContext = { this, &restored };
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* context = reinterpret_cast<SecondaryTrayContext*>(lParam);
        wchar_t className[256] = {};
        if (GetClassNameW(hwnd, className, 256) == 0) {
            return TRUE;
        }
        if (wcscmp(className, L"Shell_SecondaryTrayWnd") != 0) {
            return TRUE;
        }
        RECT rect = {};
        if (GetWindowRect(hwnd, &rect) && rect.left >= -1000 && rect.top >= -1000) {
            context->controller->relocateWindowOffScreen(reinterpret_cast<quintptr>(hwnd));
            *context->restored = true;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&secondaryContext));

    HWND startButton = FindWindow(kStartButtonClass, nullptr);
    if (startButton && IsWindowVisible(startButton)) {
        ShowWindow(startButton, SW_HIDE);
        EnableWindow(startButton, FALSE);
        restored = true;
    }

    if (!restored) {
        return;
    }

    capturePreShellTaskbarState();

    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (taskbar) {
        APPBARDATA abd = {};
        abd.cbSize = sizeof(APPBARDATA);
        abd.hWnd = taskbar;
        abd.lParam = ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        m_taskbarShellModified = true;
    }
    if (!m_taskbarHidden) {
        m_taskbarHidden = true;
        emit taskbarHiddenChanged();
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastTaskbarRelocateLogMs >= 3000) {
        m_lastTaskbarRelocateLogMs = nowMs;
        emit shellActionLogged(QStringLiteral("Taskbar re-hidden after Windows shell interruption."));
    }
    emit shellLayoutRestoreNeeded();
}

bool TaskbarController::setTaskbarVisible(bool visible)
{
    HWND taskbar = FindWindow(kTaskbarClass, nullptr);
    if (!taskbar) {
        emit shellActionLogged(QStringLiteral("Shell_TrayWnd not found."));
        return false;
    }

    if (visible) {
        restoreWindowPosition(reinterpret_cast<quintptr>(taskbar));
        ShowWindow(taskbar, SW_SHOW);
        EnableWindow(taskbar, TRUE);

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            Q_UNUSED(lParam);
            wchar_t className[256] = {};
            if (GetClassNameW(hwnd, className, 256) == 0) {
                return TRUE;
            }
            if (wcscmp(className, L"Shell_SecondaryTrayWnd") != 0) {
                return TRUE;
            }
            auto* controller = reinterpret_cast<TaskbarController*>(lParam);
            controller->restoreWindowPosition(reinterpret_cast<quintptr>(hwnd));
            ShowWindow(hwnd, SW_SHOW);
            EnableWindow(hwnd, TRUE);
            return TRUE;
        }, reinterpret_cast<LPARAM>(this));
    } else {
        relocateWindowOffScreen(reinterpret_cast<quintptr>(taskbar));

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* controller = reinterpret_cast<TaskbarController*>(lParam);
            wchar_t className[256] = {};
            if (GetClassNameW(hwnd, className, 256) == 0) {
                return TRUE;
            }
            if (wcscmp(className, L"Shell_SecondaryTrayWnd") != 0) {
                return TRUE;
            }
            controller->relocateWindowOffScreen(reinterpret_cast<quintptr>(hwnd));
            return TRUE;
        }, reinterpret_cast<LPARAM>(this));
    }

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
        capturePreShellTaskbarState();

        abd.lParam = ABS_AUTOHIDE;
        SHAppBarMessage(ABM_SETSTATE, &abd);
        m_taskbarShellModified = true;
        emit shellActionLogged(QStringLiteral("Taskbar state set to ABS_AUTOHIDE."));
    } else {
        if (m_hasOriginalTaskbarState) {
            abd.lParam = m_originalTaskbarState;
            SHAppBarMessage(ABM_SETSTATE, &abd);
            m_hasOriginalTaskbarState = false;
            emit shellActionLogged(QString("Restored taskbar AppBar state: %1").arg(m_originalTaskbarState));
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
    size = qBound(36, size, 64);
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

bool TaskbarController::keepTaskbarAutoHideOnExit() const
{
    return m_keepTaskbarAutoHideOnExit;
}

void TaskbarController::setKeepTaskbarAutoHideOnExit(bool keep)
{
    if (m_keepTaskbarAutoHideOnExit == keep)
        return;
    m_keepTaskbarAutoHideOnExit = keep;
    emit keepTaskbarAutoHideOnExitChanged();
    saveSettings();
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

bool TaskbarController::dockDragFadeEnabled() const
{
    return m_dockDragFadeEnabled;
}

void TaskbarController::setDockDragFadeEnabled(bool enabled)
{
    if (m_dockDragFadeEnabled == enabled)
        return;
    m_dockDragFadeEnabled = enabled;
    emit dockDragFadeEnabledChanged();
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

bool TaskbarController::dockSeparateTransientApps() const
{
    return m_dockSeparateTransientApps;
}

void TaskbarController::setDockSeparateTransientApps(bool enabled)
{
    if (m_dockSeparateTransientApps == enabled)
        return;
    m_dockSeparateTransientApps = enabled;
    emit dockSeparateTransientAppsChanged();
}

QString TaskbarController::normalizeAppearanceMode(const QString& mode) const
{
    if (mode == QLatin1String("light") || mode == QLatin1String("dark")) {
        return mode;
    }
    return QStringLiteral("auto");
}

bool TaskbarController::isWindowsDarkMode() const
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0,
                      KEY_READ,
                      &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 1;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS status = RegQueryValueExW(
        key, L"AppsUseLightTheme", nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        return false;
    }
    return value == 0;
}

bool TaskbarController::effectiveDarkTheme() const
{
    if (m_appearanceMode == QLatin1String("dark")) {
        return true;
    }
    if (m_appearanceMode == QLatin1String("light")) {
        return false;
    }
    return isWindowsDarkMode();
}

void TaskbarController::updateEffectiveAppearance()
{
    if (m_appearanceMode != QLatin1String("auto")) {
        return;
    }

    const bool effective = effectiveDarkTheme();
    if (effective == m_cachedEffectiveDarkTheme) {
        return;
    }

    m_cachedEffectiveDarkTheme = effective;
    emit darkThemeChanged();
}

QString TaskbarController::appearanceMode() const
{
    return m_appearanceMode;
}

void TaskbarController::setAppearanceMode(const QString& mode)
{
    const QString normalized = normalizeAppearanceMode(mode);
    if (m_appearanceMode == normalized) {
        return;
    }

    m_appearanceMode = normalized;
    saveSettings();
    emit appearanceModeChanged();

    const bool effective = effectiveDarkTheme();
    if (effective != m_cachedEffectiveDarkTheme) {
        m_cachedEffectiveDarkTheme = effective;
        emit darkThemeChanged();
    }
}

bool TaskbarController::darkTheme() const
{
    return effectiveDarkTheme();
}

void TaskbarController::setDarkTheme(bool enabled)
{
    setAppearanceMode(enabled ? QStringLiteral("dark") : QStringLiteral("light"));
}

QString TaskbarController::dockLightStyle() const
{
    return m_dockLightStyle;
}

QString TaskbarController::normalizeDockLightStyle(const QString& style) const
{
    if (style == QStringLiteral("macos27")) {
        return QStringLiteral("macos27");
    }
    return QStringLiteral("white");
}

void TaskbarController::setDockLightStyle(const QString& style)
{
    const QString normalized = normalizeDockLightStyle(style);
    if (m_dockLightStyle == normalized) {
        return;
    }
    m_dockLightStyle = normalized;
    emit dockLightStyleChanged();
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

QString TaskbarController::trashIconStyle() const
{
    return m_trashIconStyle;
}

void TaskbarController::setTrashIconStyle(const QString& style)
{
    const QString normalized = style == QStringLiteral("macos")
            ? QStringLiteral("macos")
            : QStringLiteral("windows");
    if (m_trashIconStyle == normalized) {
        return;
    }
    m_trashIconStyle = normalized;
    emit trashIconStyleChanged();
}

QString TaskbarController::normalizeMenuBarIconStyle(const QString& style) const
{
    if (style == QStringLiteral("star")
        || style == QStringLiteral("windows")
        || style == QStringLiteral("custom")) {
        return style;
    }
    return QStringLiteral("apple");
}

QString TaskbarController::menuBarIconsDirectory() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dirPath = base + QStringLiteral("/icons");
    QDir().mkpath(dirPath);
    return dirPath;
}

QString TaskbarController::bundledMenuBarIconResource(const QString& style, bool darkTheme) const
{
    if (style == QStringLiteral("star")) {
        return darkTheme
                ? QStringLiteral("qrc:/src/MacDockShell/qml/menu_bar_icon_star_white.png")
                : QStringLiteral("qrc:/src/MacDockShell/qml/menu_bar_icon_star.png");
    }
    if (style == QStringLiteral("windows")) {
        return darkTheme
                ? QStringLiteral("qrc:/src/MacDockShell/qml/menu_bar_icon_windows_white.svg")
                : QStringLiteral("qrc:/src/MacDockShell/qml/menu_bar_icon_windows.svg");
    }
    return darkTheme
            ? QStringLiteral("qrc:/src/MacDockShell/qml/apple_logo_white.svg")
            : QStringLiteral("qrc:/src/MacDockShell/qml/apple_logo.svg");
}

QString TaskbarController::menuBarIconStyle() const
{
    return m_menuBarIconStyle;
}

QString TaskbarController::menuBarCustomIconPath() const
{
    return m_menuBarCustomIconPath;
}

void TaskbarController::setMenuBarIconStyle(const QString& style)
{
    const QString normalized = normalizeMenuBarIconStyle(style);
    if (m_menuBarIconStyle == normalized) {
        return;
    }
    m_menuBarIconStyle = normalized;
    emit menuBarIconStyleChanged();
}

QString TaskbarController::menuBarIconUrl(bool darkTheme) const
{
    if (m_menuBarIconStyle == QStringLiteral("custom")) {
        if (!m_menuBarCustomIconPath.isEmpty()) {
            const QFileInfo info(m_menuBarCustomIconPath);
            if (info.exists() && info.isFile()) {
                return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
            }
        }
        return bundledMenuBarIconResource(QStringLiteral("apple"), darkTheme);
    }
    return bundledMenuBarIconResource(m_menuBarIconStyle, darkTheme);
}

QString TaskbarController::menuBarIconPreviewUrl(const QString& style, bool darkTheme) const
{
    const QString normalized = normalizeMenuBarIconStyle(style);
    if (normalized == QStringLiteral("custom")) {
        if (!m_menuBarCustomIconPath.isEmpty()) {
            const QFileInfo info(m_menuBarCustomIconPath);
            if (info.exists() && info.isFile()) {
                return QUrl::fromLocalFile(info.absoluteFilePath()).toString();
            }
        }
        return QString();
    }
    return bundledMenuBarIconResource(normalized, darkTheme);
}

bool TaskbarController::importCustomMenuBarIcon()
{
    const QString sourcePath = pickMenuBarIconFile(tr("Choose menu bar icon"));
    if (sourcePath.isEmpty()) {
        return false;
    }

    const QFileInfo sourceInfo(sourcePath);
    const QString suffix = sourceInfo.suffix().isEmpty()
            ? QStringLiteral("png")
            : sourceInfo.suffix().toLower();
    const QString destPath = menuBarIconsDirectory()
            + QStringLiteral("/menu_bar_custom.") + suffix;

    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }
    if (!QFile::copy(sourcePath, destPath)) {
        emit shellActionLogged(QStringLiteral("Failed to import custom menu bar icon: %1").arg(sourcePath));
        return false;
    }

    m_menuBarCustomIconPath = destPath;
    emit menuBarCustomIconPathChanged();
    setMenuBarIconStyle(QStringLiteral("custom"));
    saveSettings();
    emit shellActionLogged(QStringLiteral("Imported custom menu bar icon: %1").arg(destPath));
    return true;
}

bool TaskbarController::showDownloadsInDock() const
{
    return m_showDownloadsInDock;
}

void TaskbarController::setShowDownloadsInDock(bool show)
{
    if (m_showDownloadsInDock == show) {
        return;
    }
    m_showDownloadsInDock = show;
    saveSettings();
    emit showDownloadsInDockChanged();
}

bool TaskbarController::showMenuBarExtras() const
{
    return m_showMenuBarExtras;
}

void TaskbarController::setShowMenuBarExtras(bool show)
{
    if (m_showMenuBarExtras == show) {
        return;
    }
    m_showMenuBarExtras = show;
    saveSettings();
    emit showMenuBarExtrasChanged();
}

int TaskbarController::trayExtrasRefreshMs() const
{
    return m_trayExtrasRefreshMs;
}

void TaskbarController::setTrayExtrasRefreshMs(int intervalMs)
{
    const int clamped = qBound(500, intervalMs, 10000);
    if (m_trayExtrasRefreshMs == clamped) {
        return;
    }
    m_trayExtrasRefreshMs = clamped;
    saveSettings();
    emit trayExtrasRefreshMsChanged();
}

void TaskbarController::loadSettings()
{
    QSettings settings;
    m_autoHideWindowsTaskbar = settings.value(QStringLiteral("shell/autoHideTaskbar"), true).toBool();
    m_keepTaskbarAutoHideOnExit = settings.value(QStringLiteral("shell/keepTaskbarAutoHideOnExit"), false).toBool();
    m_showTopBar = settings.value(QStringLiteral("shell/showTopBar"), true).toBool();
    m_dockIconSize = qBound(36, settings.value(QStringLiteral("shell/dockIconSize"), 54).toInt(), 64);
    if (m_dockIconSize > 54) {
        m_dockIconSize = 54;
        settings.setValue(QStringLiteral("shell/dockIconSize"), m_dockIconSize);
    }
    m_dockHoverBounce = settings.value(QStringLiteral("shell/dockHoverBounce"), true).toBool();
    m_dockDragFadeEnabled = settings.value(QStringLiteral("shell/dockDragFadeEnabled"), false).toBool();
    m_dockStaticIcons = settings.value(QStringLiteral("shell/dockStaticIcons"), false).toBool();
    m_dockSeparateTransientApps = settings.value(QStringLiteral("shell/dockSeparateTransientApps"), true).toBool();
    if (settings.contains(QStringLiteral("shell/appearanceMode"))) {
        m_appearanceMode = normalizeAppearanceMode(
            settings.value(QStringLiteral("shell/appearanceMode"), QStringLiteral("auto")).toString());
    } else {
        const bool legacyDark = settings.value(QStringLiteral("shell/darkTheme"), false).toBool();
        m_appearanceMode = legacyDark ? QStringLiteral("dark") : QStringLiteral("light");
    }
    m_cachedEffectiveDarkTheme = effectiveDarkTheme();
    setDockLightStyle(settings.value(QStringLiteral("shell/dockLightStyle"), QStringLiteral("white")).toString());
    m_startWithWindows = settings.value(QStringLiteral("shell/startWithWindows"), false).toBool();
    setExplorerIconStyle(settings.value(QStringLiteral("shell/explorerIconStyle"), QStringLiteral("default")).toString());
    setTrashIconStyle(settings.value(QStringLiteral("shell/trashIconStyle"), QStringLiteral("windows")).toString());
    setMenuBarIconStyle(settings.value(QStringLiteral("shell/menuBarIconStyle"), QStringLiteral("apple")).toString());
    m_menuBarCustomIconPath = settings.value(QStringLiteral("shell/menuBarCustomIconPath")).toString();
    m_showDownloadsInDock = settings.value(QStringLiteral("shell/showDownloadsInDock"), true).toBool();
    m_showMenuBarExtras = settings.value(QStringLiteral("shell/showMenuBarExtras"), true).toBool();
    m_trayExtrasRefreshMs = qBound(500, settings.value(QStringLiteral("shell/trayExtrasRefreshMs"), 1500).toInt(), 10000);
    m_uiLanguage = normalizeUiLanguage(settings.value(QStringLiteral("shell/uiLanguage"), QStringLiteral("system")).toString());
    reconcileWindowsStartup();
}

void TaskbarController::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("shell/autoHideTaskbar"), m_autoHideWindowsTaskbar);
    settings.setValue(QStringLiteral("shell/keepTaskbarAutoHideOnExit"), m_keepTaskbarAutoHideOnExit);
    settings.setValue(QStringLiteral("shell/showTopBar"), m_showTopBar);
    settings.setValue(QStringLiteral("shell/dockIconSize"), m_dockIconSize);
    settings.setValue(QStringLiteral("shell/dockHoverBounce"), m_dockHoverBounce);
    settings.setValue(QStringLiteral("shell/dockDragFadeEnabled"), m_dockDragFadeEnabled);
    settings.setValue(QStringLiteral("shell/dockStaticIcons"), m_dockStaticIcons);
    settings.setValue(QStringLiteral("shell/dockSeparateTransientApps"), m_dockSeparateTransientApps);
    settings.setValue(QStringLiteral("shell/appearanceMode"), m_appearanceMode);
    settings.setValue(QStringLiteral("shell/dockLightStyle"), m_dockLightStyle);
    settings.setValue(QStringLiteral("shell/startWithWindows"), m_startWithWindows);
    settings.setValue(QStringLiteral("shell/explorerIconStyle"), m_explorerIconStyle);
    settings.setValue(QStringLiteral("shell/trashIconStyle"), m_trashIconStyle);
    settings.setValue(QStringLiteral("shell/menuBarIconStyle"), m_menuBarIconStyle);
    settings.setValue(QStringLiteral("shell/menuBarCustomIconPath"), m_menuBarCustomIconPath);
    settings.setValue(QStringLiteral("shell/showDownloadsInDock"), m_showDownloadsInDock);
    settings.setValue(QStringLiteral("shell/showMenuBarExtras"), m_showMenuBarExtras);
    settings.setValue(QStringLiteral("shell/trayExtrasRefreshMs"), m_trayExtrasRefreshMs);
    settings.setValue(QStringLiteral("shell/uiLanguage"), m_uiLanguage);
}

void TaskbarController::updateTaskbarVisibility()
{
    if (m_shellActive && m_autoHideWindowsTaskbar) {
        hideTaskbar();
    } else {
        restoreShell();
    }
}

void TaskbarController::apply(bool autoHideWindowsTaskbar, bool keepTaskbarAutoHideOnExit, bool showTopBar, int iconSize, bool dockHoverBounce, bool dockDragFadeEnabled, bool dockStaticIcons, bool dockSeparateTransientApps, bool darkTheme, bool startWithWindows, const QString& explorerIconStyle, const QString& trashIconStyle, const QString& menuBarIconStyle, bool showDownloadsInDock, const QString& dockLightStyle, bool showMenuBarExtras)
{
    setAutoHideWindowsTaskbar(autoHideWindowsTaskbar);
    setKeepTaskbarAutoHideOnExit(keepTaskbarAutoHideOnExit);
    setShowTopBar(showTopBar);
    setDockIconSize(iconSize);
    setDockHoverBounce(dockHoverBounce);
    setDockDragFadeEnabled(dockDragFadeEnabled);
    setDockStaticIcons(dockStaticIcons);
    setDockSeparateTransientApps(dockSeparateTransientApps);
    setDarkTheme(darkTheme);
    setDockLightStyle(dockLightStyle);
    setStartWithWindows(startWithWindows);
    setExplorerIconStyle(explorerIconStyle);
    setTrashIconStyle(trashIconStyle);
    setMenuBarIconStyle(menuBarIconStyle);
    setShowDownloadsInDock(showDownloadsInDock);
    setShowMenuBarExtras(showMenuBarExtras);
    syncWindowsStartup(startWithWindows);
    saveSettings();
    setShellActive(true);
    setSettingsVisible(false);
}

QString TaskbarController::normalizeUiLanguage(const QString& language) const
{
    if (language == QLatin1String("en")
        || language == QLatin1String("uk")
        || language == QLatin1String("ru")
        || language == QLatin1String("system")) {
        return language;
    }
    return QStringLiteral("system");
}

QString TaskbarController::uiLanguage() const
{
    return m_uiLanguage;
}

void TaskbarController::setUiLanguage(const QString& language)
{
    const QString normalized = normalizeUiLanguage(language);
    if (m_uiLanguage == normalized) {
        return;
    }
    m_uiLanguage = normalized;
    saveSettings();
    emit languageChanged();
}

QString TaskbarController::effectiveLanguage() const
{
    if (m_uiLanguage != QLatin1String("system")) {
        return m_uiLanguage;
    }

    const LANGID language = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(language)) {
    case LANG_RUSSIAN:
        return QStringLiteral("ru");
    case LANG_UKRAINIAN:
        return QStringLiteral("uk");
    default:
        return QStringLiteral("en");
    }
}

QStringList TaskbarController::availableLanguages() const
{
    return {
        QStringLiteral("system"),
        QStringLiteral("en"),
        QStringLiteral("uk"),
        QStringLiteral("ru"),
    };
}

QString TaskbarController::languageDisplayName(const QString& code) const
{
    if (code == QLatin1String("system")) {
        return tr("System");
    }
    if (code == QLatin1String("en")) {
        return QStringLiteral("English");
    }
    if (code == QLatin1String("uk")) {
        return QStringLiteral("Українська");
    }
    if (code == QLatin1String("ru")) {
        return QStringLiteral("Русский");
    }
    return code;
}

QStringList TaskbarController::defaultMenuBarItems() const
{
    return {
        tr("File"),
        tr("Edit"),
        tr("View"),
        tr("Go"),
        tr("Window"),
        tr("Help"),
    };
}

void TaskbarController::refreshMenuBar()
{
    updateForegroundMenuBar();
}
