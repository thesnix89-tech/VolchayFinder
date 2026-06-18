#include "DockModel.h"
#include "PinnedTaskbarResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QCryptographicHash>
#include <QUrl>
#include <QSet>
#include <QSettings>
#include <QProcessEnvironment>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QStandardPaths>

#include <algorithm>

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <objbase.h>
#include <commctrl.h>
#include <commoncontrols.h>

namespace {

struct EnumContext {
    QVector<DockItemEntry> entries;
};

QString normalizePath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
}

QString lowerPath(const QString& path)
{
    return normalizePath(path);
}

QString fileNameKey(const QString& path)
{
    return QFileInfo(path).fileName().toLower();
}

bool entriesEqualForView(const DockItemEntry& a, const DockItemEntry& b)
{
    return a.appId == b.appId
        && a.label == b.label
        && a.windowTitle == b.windowTitle
        && a.exePath == b.exePath
        && a.iconHint == b.iconHint
        && a.iconUrl == b.iconUrl
        && a.running == b.running
        && a.active == b.active
        && a.pinned == b.pinned
        && a.minimized == b.minimized
        && a.pinnedOnly == b.pinnedOnly;
}

QString expandEnvPath(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD copied = ExpandEnvironmentStringsW(reinterpret_cast<LPCWSTR>(path.utf16()),
                                                   buffer,
                                                   static_cast<DWORD>(std::size(buffer)));
    if (copied == 0 || copied >= std::size(buffer)) {
        return path;
    }
    return QString::fromWCharArray(buffer);
}

bool fileExistsOnDisk(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    const QString nativePath = QDir::toNativeSeparators(path);
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES;
}

QString iconLocationToExePath(const QString& iconLocation)
{
    if (iconLocation.isEmpty()) {
        return {};
    }

    QString path = iconLocation;
    const int commaIndex = path.lastIndexOf(QLatin1Char(','));
    if (commaIndex >= 0) {
        path = path.left(commaIndex);
    }
    return QDir::fromNativeSeparators(expandEnvPath(path.trimmed()));
}

QString defaultExplorerExePath()
{
    const QString windir = QProcessEnvironment::systemEnvironment().value(QStringLiteral("WINDIR"));
    if (!windir.isEmpty()) {
        return QDir::fromNativeSeparators(windir + QStringLiteral("/explorer.exe"));
    }
    return iconLocationToExePath(QStringLiteral("%windir%\\explorer.exe"));
}

bool isExplorerEntry(const DockItemEntry& entry)
{
    if (entry.appUserModelId.compare(QLatin1String("Microsoft.Windows.Explorer"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (entry.appId.compare(QLatin1String("microsoft.windows.explorer"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (fileNameKey(entry.exePath) == QLatin1String("explorer.exe")) {
        return true;
    }
    return entry.pinnedOnly
            && (entry.label.compare(QStringLiteral("File Explorer"), Qt::CaseInsensitive) == 0
                || entry.label.compare(QStringLiteral("Finder"), Qt::CaseInsensitive) == 0);
}

bool isExplorerOrderKey(const QString& key)
{
    if (key.compare(QLatin1String("microsoft.windows.explorer"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return fileNameKey(key) == QLatin1String("explorer.exe");
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

QString windowText(HWND hwnd)
{
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return {};
    }

    QString text(length + 1, Qt::Uninitialized);
    const int copied = GetWindowTextW(hwnd, reinterpret_cast<LPWSTR>(text.data()), length + 1);
    text.resize(copied);
    return text.trimmed();
}

bool isOwnProcessWindow(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid != 0 && pid == GetCurrentProcessId();
}

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* context = reinterpret_cast<EnumContext*>(lParam);

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    if (isOwnProcessWindow(hwnd)) {
        return TRUE;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    const LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        return TRUE;
    }

    const QString exePath = executablePathFromHwnd(hwnd);
    if (exePath.isEmpty()) {
        return TRUE;
    }

    QString resolvedExePath = exePath;
    QString exeName = QFileInfo(exePath).fileName().toLower();
    bool isFileExplorerWindow = false;
    if (exeName == QLatin1String("explorer.exe")) {
        wchar_t classNameBuffer[256] = {};
        GetClassNameW(hwnd, classNameBuffer, 256);
        const QString className = QString::fromWCharArray(classNameBuffer);
        // Only actual folder windows (not the desktop/shell surfaces).
        isFileExplorerWindow = className == QLatin1String("CabinetWClass")
                            || className == QLatin1String("ExploreWClass");
        if (!isFileExplorerWindow) {
            return TRUE;
        }
    } else if (exeName == QLatin1String("shellexperiencehost.exe")
               || exeName == QLatin1String("searchhost.exe")
               || exeName == QLatin1String("startmenuexperiencehost.exe")
               || exeName == QLatin1String("textinputhost.exe")
               || exeName == QLatin1String("lockapp.exe")) {
        return TRUE;
    }

    if (exeName == "steamwebhelper.exe") {
        QDir dir = QFileInfo(exePath).absoluteDir();
        for (int i = 0; i < 4; ++i) {
            if (dir.exists("steam.exe")) {
                resolvedExePath = dir.filePath("steam.exe");
                exeName = "steam.exe";
                break;
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    const QString title = windowText(hwnd);

    DockItemEntry entry;
    entry.exePath = resolvedExePath;
    entry.launchPath = resolvedExePath;
    if (isFileExplorerWindow) {
        entry.appUserModelId = QStringLiteral("Microsoft.Windows.Explorer");
        entry.appId = QStringLiteral("microsoft.windows.explorer");
        entry.label = QStringLiteral("File Explorer");
    } else {
        entry.appId = lowerPath(resolvedExePath);
        entry.label = (exeName == QLatin1String("steam.exe"))
                ? QStringLiteral("Steam")
                : QFileInfo(resolvedExePath).completeBaseName();
    }
    entry.windowTitle = title.isEmpty() ? entry.label : title;
    entry.iconHint = entry.label.left(1).toUpper();
    entry.hwndValue = reinterpret_cast<qint64>(hwnd);
    entry.running = true;
    entry.active = (GetForegroundWindow() == hwnd);
    entry.minimized = IsIconic(hwnd);

    context->entries.push_back(entry);
    return TRUE;
}

QImage hiconToImage(HICON hIcon)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) {
        return {};
    }

    BITMAP bmp = {};
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, image.bits(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    return image;
}

} // namespace

DockModel::DockModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_iconCacheDir = QDir(QCoreApplication::applicationDirPath()).filePath("iconcache");
    QDir().mkpath(m_iconCacheDir);
    m_pinnedResolver = new PinnedTaskbarResolver(this);
    QObject::connect(m_pinnedResolver, &PinnedTaskbarResolver::logMessage, this, &DockModel::logMessage);

    m_pinFolderWatcher = new QFileSystemWatcher(this);
    const QString userPinned = QDir(qEnvironmentVariable("APPDATA"))
        .filePath(QStringLiteral("Microsoft/Internet Explorer/Quick Launch/User Pinned"));
    for (const QString& sub : { QStringLiteral("TaskBar"), QStringLiteral("ImplicitAppShortcuts") }) {
        const QString dir = QDir(userPinned).filePath(sub);
        if (QDir(dir).exists()) {
            m_pinFolderWatcher->addPath(dir);
        }
    }
    QObject::connect(m_pinFolderWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        invalidatePinnedCache();
    });

    QSettings settings;
    m_explorerIconStyle = settings.value(QStringLiteral("shell/explorerIconStyle"), QStringLiteral("default")).toString();
    if (m_explorerIconStyle != QLatin1String("macos")) {
        m_explorerIconStyle = QStringLiteral("default");
    }
    m_trashIconStyle = settings.value(QStringLiteral("shell/trashIconStyle"), QStringLiteral("windows")).toString();
    if (m_trashIconStyle != QLatin1String("macos")) {
        m_trashIconStyle = QStringLiteral("windows");
    }
    loadOrder();
    refresh();
}

DockModel::~DockModel() = default;

int DockModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant DockModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const auto& item = m_entries.at(index.row());
    switch (role) {
    case AppIdRole: return item.appId;
    case LabelRole: return item.label;
    case WindowTitleRole: return item.pinnedOnly ? QString() : item.windowTitle;
    case ExePathRole: return item.exePath;
    case IconHintRole: return item.iconHint;
    case IconUrlRole: return item.iconUrl;
    case RunningRole: return item.running;
    case ActiveRole: return item.active;
    case PinnedRole: return item.pinned;
    case MinimizedRole: return item.minimized;
    case ClickableRole: return item.running || item.pinned || isRightSectionPin(item);
    case KindRole: return item.kind;
    case RightSectionPinRole: return isRightSectionPin(item);
    default: return {};
    }
}

QHash<int, QByteArray> DockModel::roleNames() const
{
    return {
        { AppIdRole, "appId" },
        { LabelRole, "label" },
        { WindowTitleRole, "windowTitle" },
        { ExePathRole, "exePath" },
        { IconHintRole, "iconHint" },
        { IconUrlRole, "iconUrl" },
        { RunningRole, "running" },
        { ActiveRole, "active" },
        { PinnedRole, "pinned" },
        { MinimizedRole, "minimized" },
        { ClickableRole, "clickable" },
        { KindRole, "kind" },
        { RightSectionPinRole, "rightSectionPinned" }
    };
}

void DockModel::refresh()
{
    if (m_actionInProgress || m_reorderActive) {
        return;
    }

    const QVector<DockItemEntry> previous = m_entries;
    QStringList previousKeys;
    previousKeys.reserve(previous.size());
    for (const auto& entry : previous) {
        previousKeys.append(entryOrderKey(entry));
    }

    m_entries.clear();
    loadPinnedApps();
    scanWindows();
    appendRightSectionPins();
    applyCustomOrder();
    // Trailing macOS-style shell items (Downloads stack + Trash) are appended last,
    // after custom ordering, so the user's drag order never affects them.
    appendTrailingShellItems();

    QStringList newKeys;
    newKeys.reserve(m_entries.size());
    for (const auto& entry : m_entries) {
        newKeys.append(entryOrderKey(entry));
    }

    if (previousKeys == newKeys) {
        for (int i = 0; i < m_entries.size(); ++i) {
            if (!entriesEqualForView(previous[i], m_entries[i])) {
                emit dataChanged(index(i), index(i));
            }
        }
    } else {
        beginResetModel();
        endResetModel();
    }

    emitActiveWindowState();
    if (previousKeys == newKeys) {
        emit logMessage(QStringLiteral("DockModel refreshed via dataChanged. Items: %1").arg(m_entries.size()));
    } else {
        emit logMessage(QStringLiteral("DockModel refreshed via resetModel. Items: %1").arg(m_entries.size()));
    }
}

void DockModel::scheduleRunningStateRefresh()
{
    // Apps often appear a few hundred ms after ShellExecute returns; poll quickly
    // instead of waiting for the slow periodic refresh timer.
    static const int delaysMs[] = { 0, 120, 280, 500, 900 };
    for (const int delayMs : delaysMs) {
        QTimer::singleShot(delayMs, this, [this]() {
            if (!m_actionInProgress && !m_reorderActive) {
                refresh();
            }
        });
    }
}

QString DockModel::entryOrderKey(const DockItemEntry& entry) const
{
    if (!entry.exePath.isEmpty()) {
        return lowerPath(entry.exePath);
    }
    if (!entry.appId.isEmpty()) {
        return entry.appId;
    }
    return entry.label.toLower();
}

QStringList DockModel::hiddenPinKeysForEntry(const DockItemEntry& entry) const
{
    QStringList keys;
    keys << entryOrderKey(entry);
    if (!entry.launchPath.isEmpty()) {
        keys << lowerPath(entry.launchPath);
    }
    if (!entry.exePath.isEmpty()) {
        keys << lowerPath(entry.exePath);
    }
    if (!entry.appUserModelId.isEmpty()) {
        keys << entry.appUserModelId.toLower();
    }
    keys.removeDuplicates();
    return keys;
}

bool DockModel::isHiddenFromDock(const DockItemEntry& entry) const
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        if (m_dockHiddenPins.contains(key)) {
            return true;
        }
    }
    return false;
}

void DockModel::hideEntryFromDock(const DockItemEntry& entry)
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        if (!m_dockHiddenPins.contains(key)) {
            m_dockHiddenPins.append(key);
        }
    }
    invalidatePinnedCache();
}

void DockModel::restoreEntryToDock(const DockItemEntry& entry)
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        m_dockHiddenPins.removeAll(key);
    }
    invalidatePinnedCache();
}

void DockModel::markExplicitDockPin(const DockItemEntry& entry)
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        if (!m_dockExplicitPins.contains(key)) {
            m_dockExplicitPins.append(key);
        }
    }
    invalidatePinnedCache();
}

void DockModel::clearExplicitDockPin(const DockItemEntry& entry)
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        m_dockExplicitPins.removeAll(key);
    }
    invalidatePinnedCache();
}

bool DockModel::isExplicitDockPin(const DockItemEntry& entry) const
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        if (m_dockExplicitPins.contains(key)) {
            return true;
        }
    }
    return false;
}

void DockModel::restorePathsToDock(const QString& sourcePath, const QString& shortcutPath)
{
    const QString sourceKey = lowerPath(sourcePath);
    const QString shortcutKey = lowerPath(shortcutPath);

    DockItemEntry probe;
    probe.exePath = sourcePath;
    probe.launchPath = shortcutPath;
    restoreEntryToDock(probe);

    if (m_pinnedResolver) {
        const QList<PinnedShortcutEntry> shortcuts = m_pinnedResolver->resolvePinnedShortcuts();
        for (const PinnedShortcutEntry& shortcut : shortcuts) {
            const QString entryShortcut = lowerPath(shortcut.shortcutPath);
            const QString entryTarget = lowerPath(shortcut.targetPath);
            if ((!shortcutKey.isEmpty() && entryShortcut == shortcutKey)
                || (!sourceKey.isEmpty() && entryTarget == sourceKey)) {
                restoreEntryToDock(makePinnedEntry(shortcut));
                break;
            }
        }
    }

    const QString sourceName = sourceKey.isEmpty() ? QString() : fileNameKey(sourceKey);
    for (int i = m_dockHiddenPins.size() - 1; i >= 0; --i) {
        const QString hidden = m_dockHiddenPins.at(i);
        if (hidden == sourceKey || hidden == shortcutKey
            || (!sourceName.isEmpty() && fileNameKey(hidden) == sourceName)) {
            m_dockHiddenPins.removeAt(i);
        }
    }
}

void DockModel::applyCustomOrder()
{
    if (m_separateTransientApps) {
        applySeparateCustomOrder();
    } else {
        applyFlatCustomOrder();
    }
}

bool DockModel::isTransientEntry(const DockItemEntry& entry) const
{
    return entry.kind == QLatin1String("app") && entry.running && !entry.pinned;
}

void DockModel::applyFlatCustomOrder()
{
    // Register newly seen apps. Explorer goes to the far left by default (macOS Finder).
    for (const auto& entry : m_entries) {
        const QString key = entryOrderKey(entry);
        if (!m_customOrder.contains(key)) {
            if (isExplorerEntry(entry)) {
                m_customOrder.prepend(key);
            } else {
                m_customOrder.append(key);
            }
        }
    }

    // Remove dead keys — apps that are no longer in m_entries (e.g. after unpin).
    // This prevents empty slots from lingering in the dock width calculation.
    QStringList aliveKeys;
    aliveKeys.reserve(m_entries.size());
    for (const auto& entry : m_entries) {
        aliveKeys.append(entryOrderKey(entry));
    }
    aliveKeys.removeDuplicates();
    for (auto it = m_customOrder.begin(); it != m_customOrder.end(); ) {
        if (!aliveKeys.contains(*it)) {
            it = m_customOrder.erase(it);
        } else {
            ++it;
        }
    }

    // Stable sort by the saved order so icons keep their position across refreshes.
    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [this](const DockItemEntry& a, const DockItemEntry& b) {
                         return m_customOrder.indexOf(entryOrderKey(a)) < m_customOrder.indexOf(entryOrderKey(b));
                     });
    updateLayoutCounts();
}

void DockModel::applySeparateCustomOrder()
{
    QVector<DockItemEntry> pinned;
    QVector<DockItemEntry> transient;
    pinned.reserve(m_entries.size());
    transient.reserve(m_entries.size());

    for (const auto& entry : m_entries) {
        if (entry.kind != QLatin1String("app")) {
            continue;
        }
        if (isLeftSectionEntry(entry)) {
            pinned.push_back(entry);
        } else if (isRightSectionEntry(entry)) {
            transient.push_back(entry);
        }
    }

    for (const auto& entry : pinned) {
        const QString key = entryOrderKey(entry);
        if (!m_customOrder.contains(key)) {
            if (isExplorerEntry(entry)) {
                m_customOrder.prepend(key);
            } else {
                m_customOrder.append(key);
            }
        }
        m_transientOrder.removeAll(key);
    }

    for (const auto& entry : transient) {
        if (isRightSectionPin(entry) || entry.running) {
            bool orderChanged = false;
            if (isRightSectionPin(entry)) {
                rememberRightSectionEntry(entry);
                bool hasExistingOrderKey = false;
                for (const QString& alias : hiddenPinKeysForEntry(entry)) {
                    if (m_transientOrder.contains(alias)) {
                        hasExistingOrderKey = true;
                        break;
                    }
                }
                if (!hasExistingOrderKey) {
                    const QString key = entryOrderKey(entry);
                    if (!m_transientOrder.contains(key)) {
                        m_transientOrder.append(key);
                        orderChanged = true;
                    }
                }
            } else {
                const QString key = entryOrderKey(entry);
                if (!m_transientOrder.contains(key)) {
                    m_transientOrder.append(key);
                    orderChanged = true;
                }
            }
            for (const QString& alias : hiddenPinKeysForEntry(entry)) {
                m_customOrder.removeAll(alias);
            }
            if (orderChanged) {
                QSettings settings;
                settings.setValue(QStringLiteral("dock/transientOrder"), m_transientOrder);
            }
        }
    }

    QStringList pinnedAlive;
    pinnedAlive.reserve(pinned.size());
    for (const auto& entry : pinned) {
        pinnedAlive.append(entryOrderKey(entry));
    }
    pinnedAlive.removeDuplicates();

    QStringList transientAlive;
    transientAlive.reserve(transient.size());
    for (const auto& entry : transient) {
        transientAlive.append(entryOrderKey(entry));
        for (const QString& alias : hiddenPinKeysForEntry(entry)) {
            transientAlive.append(alias);
        }
    }
    transientAlive.removeDuplicates();

    for (auto it = m_customOrder.begin(); it != m_customOrder.end(); ) {
        if (!pinnedAlive.contains(*it)) {
            it = m_customOrder.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_transientOrder.begin(); it != m_transientOrder.end(); ) {
        if (!transientAlive.contains(*it) && !hasRightSectionCacheForOrderKey(*it)) {
            it = m_transientOrder.erase(it);
        } else {
            ++it;
        }
    }

    const auto sortByOrder = [this](const DockItemEntry& a, const DockItemEntry& b, const QStringList& order) {
        return order.indexOf(entryOrderKey(a)) < order.indexOf(entryOrderKey(b));
    };

    std::stable_sort(pinned.begin(), pinned.end(),
                     [&](const DockItemEntry& a, const DockItemEntry& b) {
                         return sortByOrder(a, b, m_customOrder);
                     });
    std::stable_sort(transient.begin(), transient.end(),
                     [&](const DockItemEntry& a, const DockItemEntry& b) {
                         return sortByOrder(a, b, m_transientOrder);
                     });

    m_entries.clear();
    m_entries += pinned;
    m_entries += transient;
    updateLayoutCounts();
}

void DockModel::updateLayoutCounts()
{
    int pinned = 0;
    int transient = 0;
    for (const auto& entry : m_entries) {
        if (entry.kind != QLatin1String("app")) {
            continue;
        }
        if (isLeftSectionEntry(entry)) {
            ++pinned;
        } else if (m_separateTransientApps && isRightSectionEntry(entry)) {
            ++transient;
        }
    }

    if (pinned != m_pinnedAppCount || transient != m_transientAppCount) {
        m_pinnedAppCount = pinned;
        m_transientAppCount = transient;
        emit dockLayoutChanged();
    } else {
        m_pinnedAppCount = pinned;
        m_transientAppCount = transient;
    }
}

void DockModel::mergeTransientIntoCustomOrder()
{
    for (const QString& key : m_transientOrder) {
        if (!m_customOrder.contains(key)) {
            m_customOrder.append(key);
        }
    }
    m_transientOrder.clear();
}

void DockModel::setSeparateTransientApps(bool enabled)
{
    if (m_separateTransientApps == enabled) {
        return;
    }

    m_separateTransientApps = enabled;
    if (!enabled) {
        mergeTransientIntoCustomOrder();
        saveOrder();
    }
    refresh();
}

int DockModel::pinnedAppCount() const
{
    return m_pinnedAppCount;
}

int DockModel::transientAppCount() const
{
    return m_transientAppCount;
}

void DockModel::ensureExplorerLeadingInOrder()
{
    int explorerIndex = -1;
    for (int i = 0; i < m_customOrder.size(); ++i) {
        if (isExplorerOrderKey(m_customOrder.at(i))) {
            explorerIndex = i;
            break;
        }
    }

    if (explorerIndex <= 0) {
        return;
    }

    const QString explorerKey = m_customOrder.takeAt(explorerIndex);
    m_customOrder.prepend(explorerKey);
    saveOrder();
}

bool DockModel::isRightSectionPin(const DockItemEntry& entry) const
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        if (m_transientOrder.contains(key)) {
            return true;
        }
    }
    return false;
}

bool DockModel::hasRightSectionCacheForOrderKey(const QString& orderKey) const
{
    if (orderKey.isEmpty()) {
        return false;
    }
    if (m_rightSectionPinCache.contains(orderKey)) {
        return true;
    }
    for (auto it = m_rightSectionPinCache.constBegin(); it != m_rightSectionPinCache.constEnd(); ++it) {
        if (hiddenPinKeysForEntry(it.value()).contains(orderKey)) {
            return true;
        }
    }
    return false;
}

DockItemEntry DockModel::rightSectionCachedEntryForOrderKey(const QString& orderKey) const
{
    if (orderKey.isEmpty()) {
        return {};
    }
    if (m_rightSectionPinCache.contains(orderKey)) {
        return m_rightSectionPinCache.value(orderKey);
    }
    for (auto it = m_rightSectionPinCache.constBegin(); it != m_rightSectionPinCache.constEnd(); ++it) {
        if (hiddenPinKeysForEntry(it.value()).contains(orderKey)) {
            return it.value();
        }
    }
    return {};
}

bool DockModel::isLeftSectionEntry(const DockItemEntry& entry) const
{
    if (entry.kind != QLatin1String("app")) {
        return false;
    }
    if (isRightSectionPin(entry)) {
        return false;
    }
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        if (m_customOrder.contains(key)) {
            return true;
        }
    }
    return entry.pinned;
}

bool DockModel::isRightSectionEntry(const DockItemEntry& entry) const
{
    if (entry.kind != QLatin1String("app")) {
        return false;
    }
    if (isRightSectionPin(entry)) {
        return true;
    }
    if (isLeftSectionEntry(entry)) {
        return false;
    }
    return entry.running;
}

void DockModel::insertKeyAtIndex(QStringList& order, const QString& key, int index)
{
    order.removeAll(key);
    index = qBound(0, index, order.size());
    order.insert(index, key);
}

void DockModel::insertPinnedKeyAtSlot(const QString& orderKey, int pinnedSlot)
{
    if (hasRightSectionCacheForOrderKey(orderKey)) {
        clearRightSectionPin(rightSectionCachedEntryForOrderKey(orderKey));
    } else {
        m_transientOrder.removeAll(orderKey);
        m_rightSectionPinCache.remove(orderKey);
    }
    m_customOrder.removeAll(orderKey);
    insertKeyAtIndex(m_customOrder, orderKey, pinnedSlot);
    ensureExplorerLeadingInOrder();
}

bool DockModel::ensurePinnedShortcut(const QString& path, QString* shortcutPathOut)
{
    if (path.isEmpty() || !m_pinnedResolver) {
        return false;
    }

    const QString shortcutPath = m_pinnedResolver->createPinFromPath(path);
    if (shortcutPath.isEmpty()) {
        return false;
    }

    restorePathsToDock(path, shortcutPath);

    DockItemEntry explicitEntry;
    explicitEntry.exePath = path;
    explicitEntry.launchPath = shortcutPath;
    markExplicitDockPin(explicitEntry);
    invalidatePinnedCache();

    if (shortcutPathOut) {
        *shortcutPathOut = shortcutPath;
    }
    return true;
}

void DockModel::transferToLeftSection(int from, int to)
{
    if (from < 0 || from >= m_entries.size()) {
        return;
    }

    const DockItemEntry& entry = m_entries.at(from);
    if (entry.kind != QLatin1String("app")) {
        return;
    }

    const QString path = entry.exePath.isEmpty() ? entry.launchPath : entry.exePath;
    if (path.isEmpty()) {
        return;
    }

    const QString orderKey = entryOrderKey(entry);

    QString shortcutPath;
    if (!ensurePinnedShortcut(path, &shortcutPath)) {
        emit logMessage(QStringLiteral("transferToLeftSection: failed to pin %1").arg(path));
        return;
    }

    m_reorderActive = false;
    refresh();

    const QString shortcutKey = lowerPath(shortcutPath);
    const QString sourceKey = lowerPath(path);

    QString resolvedKey = orderKey;
    for (int i = 0; i < m_entries.size(); ++i) {
        const DockItemEntry& pinnedEntry = m_entries.at(i);
        if (!pinnedEntry.pinned) {
            continue;
        }
        const QString entryLaunch = lowerPath(pinnedEntry.launchPath);
        const QString entryExe = lowerPath(pinnedEntry.exePath);
        if (entryLaunch == shortcutKey || entryLaunch == sourceKey
            || (!entryExe.isEmpty() && (entryExe == sourceKey || entryExe == shortcutKey))
            || (!sourceKey.isEmpty() && !entryExe.isEmpty()
                && fileNameKey(entryExe) == fileNameKey(sourceKey))) {
            resolvedKey = entryOrderKey(pinnedEntry);
            break;
        }
    }

    const int pinnedSlot = qBound(0, to, qMax(0, m_pinnedAppCount - 1));
    clearRightSectionPin(entry);
    insertPinnedKeyAtSlot(resolvedKey, pinnedSlot);
    saveOrder();

    m_reorderActive = false;
    refresh();
    emit logMessage(QStringLiteral("Transferred to left section: %1 at slot %2").arg(path).arg(pinnedSlot));
}

void DockModel::transferPinnedToRightSection(int from, int to)
{
    if (from < 0 || from >= m_entries.size()) {
        return;
    }

    if (!canUnpinIndex(from)) {
        return;
    }

    const DockItemEntry entry = m_entries.at(from);
    const QString key = entryOrderKey(entry);
    const int pinnedCount = m_pinnedAppCount;
    const int transientSlot = qBound(0, to - pinnedCount, qMax(0, m_transientAppCount));

    m_customOrder.removeAll(key);
    for (const QString& alias : hiddenPinKeysForEntry(entry)) {
        m_customOrder.removeAll(alias);
    }
    insertKeyAtIndex(m_transientOrder, key, transientSlot);
    rememberRightSectionEntry(entry);

    saveOrder();
    m_reorderActive = false;
    invalidatePinnedCache();
    refresh();
    emit logMessage(QStringLiteral("Transferred to right section: %1 at transient slot %2")
                        .arg(entry.exePath.isEmpty() ? entry.label : entry.exePath)
                        .arg(transientSlot));
}

void DockModel::appendRightSectionPins()
{
    if (!m_separateTransientApps || m_transientOrder.isEmpty()) {
        return;
    }

    // Reuse the pin list already resolved in loadPinnedApps() for this refresh.
    // Calling resolveDockPinnedShortcuts() here walked the pin folders every second
    // whenever m_transientOrder was non-empty (right-section pins).
    const QList<PinnedShortcutEntry>& shortcuts = m_cachedPinnedShortcuts;

    for (const QString& rightKey : m_transientOrder) {
        bool alreadyPresent = false;
        for (const auto& entry : m_entries) {
            if (entry.kind != QLatin1String("app")) {
                continue;
            }
            if (entryOrderKey(entry) == rightKey || hiddenPinKeysForEntry(entry).contains(rightKey)) {
                alreadyPresent = true;
                break;
            }
        }
        if (alreadyPresent) {
            continue;
        }

        const DockItemEntry cachedEntry = rightSectionCachedEntryForOrderKey(rightKey);
        if (hasRightSectionCacheForOrderKey(rightKey)) {
            DockItemEntry cached = cachedEntry;
            cached.pinned = false;
            cached.running = false;
            cached.pinnedOnly = false;
            upsertEntry(cached);
            rememberRightSectionEntry(cached);
            continue;
        }

        for (const PinnedShortcutEntry& shortcut : shortcuts) {
            DockItemEntry candidate = makePinnedEntry(shortcut);
            if (entryOrderKey(candidate) != rightKey
                && !hiddenPinKeysForEntry(candidate).contains(rightKey)) {
                continue;
            }

            candidate.pinned = false;
            candidate.running = false;
            candidate.pinnedOnly = false;
            upsertEntry(candidate);
            rememberRightSectionEntry(candidate);
            break;
        }
    }
}

void DockModel::pinPathAt(const QString& path, int index)
{
    if (path.isEmpty() || !m_pinnedResolver) {
        return;
    }

    QString shortcutPath;
    if (!ensurePinnedShortcut(path, &shortcutPath)) {
        return;
    }

    saveOrder();

    // QML keeps reorderActive=true while the external drop preview is open.
    // For the final pin we must still force one model refresh so the newly
    // created shortcut enters m_entries; otherwise the next ordinary refresh
    // discovers it later and applyCustomOrder appends it to the right edge.
    m_reorderActive = false;
    refresh();

    const QString shortcutKey = lowerPath(shortcutPath);
    const QString sourceKey = lowerPath(path);

    QString orderKey;
    for (int i = 0; i < m_entries.size(); ++i) {
        const DockItemEntry& entry = m_entries.at(i);
        if (!entry.pinned) {
            continue;
        }
        const QString entryLaunch = lowerPath(entry.launchPath);
        const QString entryExe = lowerPath(entry.exePath);
        if (entryLaunch == shortcutKey || entryLaunch == sourceKey
            || (!entryExe.isEmpty() && (entryExe == sourceKey || entryExe == shortcutKey))
            || (!sourceKey.isEmpty() && !entryExe.isEmpty()
                && fileNameKey(entryExe) == fileNameKey(sourceKey))) {
            orderKey = entryOrderKey(entry);
            break;
        }
    }

    if (orderKey.isEmpty()) {
        const QList<PinnedShortcutEntry> shortcuts = m_pinnedResolver->resolveAllFolderShortcuts();
        for (const PinnedShortcutEntry& shortcut : shortcuts) {
            if (lowerPath(shortcut.shortcutPath) != shortcutKey
                && lowerPath(shortcut.targetPath) != sourceKey) {
                continue;
            }
            DockItemEntry entry = makePinnedEntry(shortcut);
            restoreEntryToDock(entry);
            orderKey = entryOrderKey(entry);
            break;
        }
    }

    if (orderKey.isEmpty()) {
        emit logMessage(QStringLiteral("Pinned shortcut created but dock entry missing: %1").arg(shortcutPath));
        return;
    }

    const int pinnedSlot = index < 0
            ? qMax(0, m_pinnedAppCount - 1)
            : qBound(0, index, qMax(0, m_pinnedAppCount - 1));
    insertPinnedKeyAtSlot(orderKey, pinnedSlot);
    saveOrder();
    m_reorderActive = false;
    refresh();

    emit logMessage(QStringLiteral("Pinned to dock: %1 at slot %2").arg(shortcutPath).arg(pinnedSlot));
}

bool DockModel::canUnpinIndex(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }

    const DockItemEntry& entry = m_entries.at(index);
    return entry.kind == QLatin1String("app") && !isExplorerEntry(entry)
            && (entry.pinned || isRightSectionPin(entry));
}

void DockModel::unpinIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    const DockItemEntry entry = m_entries.at(index);

    if (!canUnpinIndex(index)) {
        if (isExplorerEntry(entry)) {
            emit logMessage(QStringLiteral("unpinIndex: blocked — cannot unpin Explorer/Finder (AUMID=%1)")
                                .arg(entry.appUserModelId));
        }
        return;
    }

    if (isRightSectionPin(entry) && !entry.pinned) {
        clearRightSectionPin(entry);
        saveOrder();
        m_reorderActive = false;
        refresh();
        emit logMessage(QStringLiteral("unpinIndex: right-section pin removed: %1")
                            .arg(entry.exePath.isEmpty() ? entry.label : entry.exePath));
        return;
    }

    if (!entry.pinned) {
        return;
    }

    auto& mutableEntry = m_entries[index];
    const bool staysOnDockWhileRunning = mutableEntry.running;
    hideEntryFromDock(mutableEntry);
    clearExplicitDockPin(mutableEntry);
    m_customOrder.removeOne(entryOrderKey(mutableEntry));
    saveOrder();
    if (staysOnDockWhileRunning) {
        emit logMessage(QStringLiteral("unpinIndex: dock pin removed; icon stays while running: %1")
                            .arg(mutableEntry.exePath.isEmpty() ? mutableEntry.label : mutableEntry.exePath));
    } else {
        emit logMessage(QStringLiteral("unpinIndex: hidden from dock only (taskbar pin kept): %1")
                            .arg(mutableEntry.exePath.isEmpty() ? mutableEntry.label : mutableEntry.exePath));
    }
    m_reorderActive = false;
    refresh();
    emit logMessage(QStringLiteral("Unpinned at index %1. Dock items after refresh: %2").arg(index).arg(m_entries.size()));
}

void DockModel::moveItem(int from, int to)
{
    if (from < 0 || from >= m_entries.size() || to < 0 || to >= m_entries.size() || from == to) {
        return;
    }

    // Trailing shell items (Downloads/Trash) are not reorderable, and apps must not
    // move past them into the right section. Clamp to the app range.
    if (m_entries.at(from).kind != QLatin1String("app")) {
        return;
    }
    const int apps = appItemCount();
    if (to > apps - 1) {
        to = apps - 1;
    }
    if (to < 0 || from == to) {
        return;
    }

    if (m_separateTransientApps) {
        const int pinnedCount = m_pinnedAppCount;
        const bool fromPinned = from < pinnedCount;
        const bool fromTransient = !fromPinned && from < pinnedCount + m_transientAppCount;

        if (fromTransient && to < pinnedCount) {
            transferToLeftSection(from, to);
            return;
        }

        if (fromPinned && to >= pinnedCount) {
            transferPinnedToRightSection(from, to);
            return;
        }
    }

    beginResetModel();
    m_entries.move(from, to);

    if (m_separateTransientApps) {
        QStringList pinnedKeys;
        QStringList transientKeys;
        for (const auto& entry : m_entries) {
            if (entry.kind != QLatin1String("app")) {
                continue;
            }
            const QString key = entryOrderKey(entry);
            if (isLeftSectionEntry(entry)) {
                pinnedKeys.append(key);
            } else if (isRightSectionEntry(entry)) {
                transientKeys.append(key);
            }
        }

        QStringList newCustomOrder;
        for (const QString& key : pinnedKeys) {
            newCustomOrder.append(key);
        }
        for (const QString& key : m_customOrder) {
            if (!newCustomOrder.contains(key) && !transientKeys.contains(key)) {
                newCustomOrder.append(key);
            }
        }
        m_customOrder = newCustomOrder;

        QStringList newTransientOrder;
        for (const QString& key : transientKeys) {
            newTransientOrder.append(key);
        }
        for (const QString& key : m_transientOrder) {
            if (!newTransientOrder.contains(key) && !pinnedKeys.contains(key)) {
                newTransientOrder.append(key);
            }
        }
        m_transientOrder = newTransientOrder;
        updateLayoutCounts();
    } else {
        // Rebuild the saved order: currently visible apps in their new order first,
        // then any other remembered apps that are not on the dock right now.
        QStringList newOrder;
        for (const auto& entry : m_entries) {
            if (entry.kind != QLatin1String("app")) {
                continue; // never persist the trailing shell items in the custom order
            }
            newOrder.append(entryOrderKey(entry));
        }
        for (const QString& key : m_customOrder) {
            if (!newOrder.contains(key)) {
                newOrder.append(key);
            }
        }
        m_customOrder = newOrder;
    }
    endResetModel();

    saveOrder();
    emit logMessage(QStringLiteral("Dock item moved: %1 -> %2").arg(from).arg(to));
}

void DockModel::setReorderActive(bool active)
{
    m_reorderActive = active;
}

void DockModel::loadOrder()
{
    QSettings settings;
    m_customOrder = settings.value(QStringLiteral("dock/customOrder")).toStringList();
    m_transientOrder = settings.value(QStringLiteral("dock/transientOrder")).toStringList();
    m_dockHiddenPins = settings.value(QStringLiteral("dock/hiddenPins")).toStringList();
    m_dockExplicitPins = settings.value(QStringLiteral("dock/explicitPins")).toStringList();
    ensureExplorerLeadingInOrder();
    loadRightSectionCache();
}

void DockModel::saveOrder()
{
    QSettings settings;
    settings.setValue(QStringLiteral("dock/customOrder"), m_customOrder);
    settings.setValue(QStringLiteral("dock/transientOrder"), m_transientOrder);
    settings.setValue(QStringLiteral("dock/hiddenPins"), m_dockHiddenPins);
    settings.setValue(QStringLiteral("dock/explicitPins"), m_dockExplicitPins);
    saveRightSectionCache();
}

void DockModel::rememberRightSectionEntry(const DockItemEntry& entry)
{
    DockItemEntry cached = entry;
    cached.hwndValue = 0;
    cached.running = false;
    cached.active = false;
    cached.minimized = false;
    cached.pinned = false;
    cached.pinnedOnly = false;
    cached.windowTitle.clear();

    bool changed = false;
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        const auto it = m_rightSectionPinCache.constFind(key);
        if (it == m_rightSectionPinCache.constEnd() || it.value().label != cached.label
            || it.value().exePath != cached.exePath || it.value().launchPath != cached.launchPath
            || it.value().appId != cached.appId || it.value().iconUrl != cached.iconUrl
            || it.value().appUserModelId != cached.appUserModelId) {
            m_rightSectionPinCache.insert(key, cached);
            changed = true;
        }
    }
    if (changed) {
        saveRightSectionCache();
    }
}

void DockModel::clearRightSectionPin(const DockItemEntry& entry)
{
    for (const QString& key : hiddenPinKeysForEntry(entry)) {
        m_transientOrder.removeAll(key);
        m_rightSectionPinCache.remove(key);
    }
}

void DockModel::loadRightSectionCache()
{
    m_rightSectionPinCache.clear();
    QSettings settings;
    const QVariantList items = settings.value(QStringLiteral("dock/rightSectionCache")).toList();
    for (const QVariant& var : items) {
        const QVariantMap map = var.toMap();
        const QString key = map.value(QStringLiteral("key")).toString();
        if (key.isEmpty()) {
            continue;
        }

        DockItemEntry entry;
        entry.appId = map.value(QStringLiteral("appId")).toString();
        entry.label = map.value(QStringLiteral("label")).toString();
        entry.exePath = map.value(QStringLiteral("exePath")).toString();
        entry.launchPath = map.value(QStringLiteral("launchPath")).toString();
        entry.appUserModelId = map.value(QStringLiteral("appUserModelId")).toString();
        entry.iconHint = map.value(QStringLiteral("iconHint")).toString();
        entry.iconUrl = map.value(QStringLiteral("iconUrl")).toString();

        m_rightSectionPinCache.insert(key, entry);
        for (const QString& alias : hiddenPinKeysForEntry(entry)) {
            m_rightSectionPinCache.insert(alias, entry);
        }
    }
}

void DockModel::saveRightSectionCache()
{
    QSet<QString> savedKeys;
    QVariantList items;
    for (auto it = m_rightSectionPinCache.constBegin(); it != m_rightSectionPinCache.constEnd(); ++it) {
        const QString canonicalKey = entryOrderKey(it.value());
        if (savedKeys.contains(canonicalKey)) {
            continue;
        }
        savedKeys.insert(canonicalKey);

        const DockItemEntry& entry = it.value();
        QVariantMap map;
        map.insert(QStringLiteral("key"), canonicalKey);
        map.insert(QStringLiteral("appId"), entry.appId);
        map.insert(QStringLiteral("label"), entry.label);
        map.insert(QStringLiteral("exePath"), entry.exePath);
        map.insert(QStringLiteral("launchPath"), entry.launchPath);
        map.insert(QStringLiteral("appUserModelId"), entry.appUserModelId);
        map.insert(QStringLiteral("iconHint"), entry.iconHint);
        map.insert(QStringLiteral("iconUrl"), entry.iconUrl);
        items.append(map);
    }

    QSettings settings;
    settings.setValue(QStringLiteral("dock/rightSectionCache"), items);
}

void DockModel::activateIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_entries[index].hwndValue);
    if (!hwnd || !IsWindow(hwnd)) {
        emit logMessage(QStringLiteral("activateIndex: invalid hwnd for index %1").arg(index));
        return;
    }

    m_actionInProgress = true;
    if (IsIconic(hwnd)) {
        // Only minimized windows need restoring. Preserve the previous state so a
        // window that was maximized before minimizing comes back maximized, not windowed.
        WINDOWPLACEMENT placement = {};
        placement.length = sizeof(placement);
        if (GetWindowPlacement(hwnd, &placement) && (placement.flags & WPF_RESTORETOMAXIMIZED)) {
            ShowWindow(hwnd, SW_MAXIMIZE);
        } else {
            ShowWindow(hwnd, SW_RESTORE);
        }
    }
    // If the window is not minimized (e.g. maximized but unfocused), just bring it
    // forward without changing its size/state.
    SetForegroundWindow(hwnd);
    m_actionInProgress = false;
    emit logMessage(QStringLiteral("Activated window: %1").arg(m_entries[index].windowTitle));
    refresh();
}

void DockModel::minimizeIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_entries[index].hwndValue);
    if (!hwnd || !IsWindow(hwnd)) {
        emit logMessage(QStringLiteral("minimizeIndex: invalid hwnd for index %1").arg(index));
        return;
    }

    m_actionInProgress = true;
    ShowWindow(hwnd, SW_MINIMIZE);
    m_actionInProgress = false;
    emit logMessage(QStringLiteral("Minimized window: %1").arg(m_entries[index].windowTitle));
    refresh();
}

void DockModel::toggleIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    const auto& item = m_entries[index];
    if (!item.running || item.hwndValue == 0) {
        launchIndex(index);
        return;
    }

    if (item.active && !item.minimized) {
        minimizeIndex(index);
    } else {
        activateIndex(index);
    }
}

void DockModel::launchIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    const auto& item = m_entries[index];
    const QString launchPath = item.launchPath.isEmpty() ? item.exePath : item.launchPath;
    if (launchPath.isEmpty()) {
        emit logMessage(QStringLiteral("launchIndex: empty launch path for index %1").arg(index));
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(launchPath);
    const QString nativeWorkingDirectory = item.workingDirectory.isEmpty()
            ? QFileInfo(launchPath).absolutePath()
            : QDir::toNativeSeparators(item.workingDirectory);
    const QString nativeArguments = item.launchArguments;

    emit logMessage(QStringLiteral("launchIndex: path=%1 args=%2 cwd=%3")
                    .arg(nativePath,
                         nativeArguments.isEmpty() ? QStringLiteral("<none>") : nativeArguments,
                         nativeWorkingDirectory.isEmpty() ? QStringLiteral("<none>") : nativeWorkingDirectory));

    m_actionInProgress = true;
    HINSTANCE result = ShellExecuteW(nullptr,
                                     L"open",
                                     reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                     nativeArguments.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(nativeArguments.utf16()),
                                     nativeWorkingDirectory.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(nativeWorkingDirectory.utf16()),
                                     SW_SHOWNORMAL);
    m_actionInProgress = false;
    const quintptr resultCode = reinterpret_cast<quintptr>(result);
    if (resultCode <= 32) {
        emit logMessage(QStringLiteral("launchIndex: failed for %1 code=%2").arg(launchPath).arg(resultCode));
        return;
    }

    emit logMessage(QStringLiteral("Launched pinned app: %1").arg(launchPath));
    scheduleRunningStateRefresh();
}

void DockModel::closeIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_entries[index].hwndValue);
    if (!hwnd || !IsWindow(hwnd)) {
        emit logMessage(QStringLiteral("closeIndex: invalid hwnd for index %1").arg(index));
        return;
    }

    // PostMessage is asynchronous, so this does not pump a nested message loop.
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    emit logMessage(QStringLiteral("Requested close for window: %1").arg(m_entries[index].windowTitle));
    scheduleRunningStateRefresh();
}

void DockModel::revealIndex(int index)
{
    if (index < 0 || index >= m_entries.size()) {
        return;
    }

    const QString exePath = m_entries[index].exePath;
    if (exePath.isEmpty()) {
        emit logMessage(QStringLiteral("revealIndex: empty exe path for index %1").arg(index));
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(exePath);
    const QString parameters = QStringLiteral("/select,\"%1\"").arg(nativePath);

    m_actionInProgress = true;
    ShellExecuteW(nullptr,
                  nullptr,
                  L"explorer.exe",
                  reinterpret_cast<LPCWSTR>(parameters.utf16()),
                  nullptr,
                  SW_SHOWNORMAL);
    m_actionInProgress = false;
    emit logMessage(QStringLiteral("Reveal in Explorer: %1").arg(nativePath));
}

void DockModel::syncFromWindowsTaskbarPins()
{
    if (!m_pinnedResolver) {
        return;
    }

    const QList<PinnedShortcutEntry> activeShortcuts = m_pinnedResolver->resolvePinnedShortcutsForDockSync();
    if (activeShortcuts.isEmpty()) {
        emit logMessage(QStringLiteral("Taskbar sync skipped: no active Windows taskbar pins resolved."));
        return;
    }

    QSet<QString> activeShortcutPaths;
    activeShortcutPaths.reserve(activeShortcuts.size());
    for (const PinnedShortcutEntry& shortcut : activeShortcuts) {
        if (!shortcut.shortcutPath.isEmpty()) {
            activeShortcutPaths.insert(lowerPath(shortcut.shortcutPath));
        }
    }

    const int preservedExplicitPins = m_dockExplicitPins.size();

    for (const PinnedShortcutEntry& shortcut : activeShortcuts) {
        if (shortcut.shortcutPath.isEmpty()) {
            continue;
        }
        DockItemEntry entry = makePinnedEntry(shortcut);
        restoreEntryToDock(entry);
        markExplicitDockPin(entry);
    }

    int hiddenCount = 0;
    for (const PinnedShortcutEntry& shortcut : m_pinnedResolver->resolveAllFolderShortcuts()) {
        if (!shortcut.shortcutPath.isEmpty()
            && activeShortcutPaths.contains(lowerPath(shortcut.shortcutPath))) {
            continue;
        }

        const DockItemEntry folderEntry = makePinnedEntry(shortcut);
        if (isExplicitDockPin(folderEntry)) {
            continue;
        }

        hideEntryFromDock(folderEntry);
        ++hiddenCount;
    }

    saveOrder();
    emit logMessage(QStringLiteral("Synced dock with Windows taskbar pins: %1 from taskbar, %2 folder shortcuts hidden, %3 user dock pins kept.")
                        .arg(activeShortcuts.size())
                        .arg(hiddenCount)
                        .arg(preservedExplicitPins));
}

void DockModel::invalidatePinnedCache()
{
    m_pinnedShortcutsCacheValid = false;
}

void DockModel::loadPinnedApps()
{
    // Re-resolve pins only when the cache is dirty (a pin/unpin happened) or after a
    // long safety interval to pick up external Windows taskbar changes. Otherwise the
    // expensive folder walk is skipped and we reuse the cached shortcut list.
    constexpr int kPinRescanIntervalRefreshes = 300; // ~5 min safety net; the watcher handles real changes
    if (!m_pinnedShortcutsCacheValid || m_refreshesSincePinScan >= kPinRescanIntervalRefreshes) {
        m_cachedPinnedShortcuts = m_pinnedResolver->resolveDockPinnedShortcuts(m_dockExplicitPins);
        m_pinnedShortcutsCacheValid = true;
        m_refreshesSincePinScan = 0;
    } else {
        ++m_refreshesSincePinScan;
    }
    const QList<PinnedShortcutEntry>& shortcuts = m_cachedPinnedShortcuts;

    QStringList currentShortcutSnapshot;
    currentShortcutSnapshot.reserve(shortcuts.size());

    for (const PinnedShortcutEntry& shortcut : shortcuts) {
        currentShortcutSnapshot << lowerPath(shortcut.shortcutPath.isEmpty() ? shortcut.targetPath : shortcut.shortcutPath);
    }
    currentShortcutSnapshot.removeDuplicates();
    const bool pinnedDirty = currentShortcutSnapshot != m_lastPinnedShortcutSnapshot;
    m_lastPinnedShortcutSnapshot = currentShortcutSnapshot;

    m_pinnedPaths.clear();

    for (const PinnedShortcutEntry& shortcut : shortcuts) {
        // Allow special shell links (e.g. File Explorer) that have an empty target
        // path; they still launch via the .lnk. Only skip truly empty entries.
        if (shortcut.targetPath.isEmpty() && shortcut.shortcutPath.isEmpty()) {
            continue;
        }

        DockItemEntry entry = makePinnedEntry(shortcut);
        if (isHiddenFromDock(entry) || isRightSectionPin(entry)) {
            continue;
        }
        if (!entry.exePath.isEmpty()) {
            m_pinnedPaths << entry.exePath;
        }
        upsertEntry(entry);
        if (pinnedDirty) {
            emit logMessage(QStringLiteral("Pinned app loaded: target=%1 launch=%2 aumid=%3 args=%4")
                            .arg(entry.exePath,
                                 entry.launchPath,
                                 entry.appUserModelId.isEmpty() ? QStringLiteral("<none>") : entry.appUserModelId,
                                 entry.launchArguments.isEmpty() ? QStringLiteral("<none>") : entry.launchArguments));
        }
    }

    ensureExplorerPin();
}

void DockModel::ensureExplorerPin()
{
    const QString explorerExe = defaultExplorerExePath();

    for (auto& existing : m_entries) {
        if (existing.appUserModelId.compare(QStringLiteral("Microsoft.Windows.Explorer"), Qt::CaseInsensitive) != 0
            && !(existing.pinned
                 && (existing.label.compare(QStringLiteral("File Explorer"), Qt::CaseInsensitive) == 0
                     || existing.label.compare(QStringLiteral("Finder"), Qt::CaseInsensitive) == 0))) {
            continue;
        }

        if (existing.exePath.isEmpty()) {
            existing.exePath = explorerExe;
        }
        if (existing.launchPath.isEmpty()) {
            existing.launchPath = explorerExe;
        }
        if (existing.iconUrl.isEmpty() && !explorerExe.isEmpty()) {
            const QString iconUrl = ensureIconFile(
                existing.appId.isEmpty() ? QStringLiteral("microsoft.windows.explorer") : existing.appId,
                explorerExe);
            if (!iconUrl.isEmpty()) {
                existing.iconUrl = iconUrl;
                m_existingIconUrls.insert(existing.appId, iconUrl);
            }
        }
        return;
    }

    // The "File Explorer" taskbar pin is a special shell link that is not always
    // visible to QFileInfo while our shell is active, so synthesize the pin entry.
    DockItemEntry explorer;
    explorer.appUserModelId = QStringLiteral("Microsoft.Windows.Explorer");
    explorer.appId = QStringLiteral("microsoft.windows.explorer");
    explorer.exePath = explorerExe;
    explorer.launchPath = explorerExe;
    explorer.label = explorerDisplayLabel();
    explorer.iconHint = m_explorerIconStyle == QLatin1String("macos") ? QString() : QStringLiteral("F");
    explorer.pinned = true;
    explorer.pinnedOnly = true;
    upsertEntry(explorer);
}

void DockModel::scanWindows()
{
    EnumContext context;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&context));
    for (const auto& entry : context.entries) {
        // Running apps stay visible after dock-unpin until they quit.
        upsertEntry(entry);
    }
}

DockItemEntry DockModel::makePinnedEntry(const PinnedShortcutEntry& shortcut) const
{
    DockItemEntry entry;
    entry.exePath = shortcut.targetPath;
    entry.launchPath = shortcut.shortcutPath.isEmpty() ? shortcut.targetPath : shortcut.shortcutPath;
    entry.launchArguments = shortcut.arguments;
    entry.workingDirectory = shortcut.workingDirectory;
    entry.appUserModelId = shortcut.appUserModelId;

    // Shell pins like "File Explorer" have an empty target path but carry the real
    // executable in IconLocation (e.g. %windir%\explorer.exe,0).
    if (entry.exePath.isEmpty() && !shortcut.iconLocation.isEmpty()) {
        entry.exePath = iconLocationToExePath(shortcut.iconLocation);
    }
    if (entry.exePath.isEmpty()
        && entry.appUserModelId.compare(QStringLiteral("Microsoft.Windows.Explorer"), Qt::CaseInsensitive) == 0) {
        entry.exePath = defaultExplorerExePath();
    }

    const QString shortcutBaseName = QFileInfo(shortcut.shortcutPath).completeBaseName();
    if (entry.appUserModelId.isEmpty()
        && shortcutBaseName.compare(QStringLiteral("File Explorer"), Qt::CaseInsensitive) == 0) {
        entry.appUserModelId = QStringLiteral("Microsoft.Windows.Explorer");
        if (entry.exePath.isEmpty()) {
            entry.exePath = defaultExplorerExePath();
        }
    }

    // Fall back to the shortcut path for a stable id when the link has no target
    // path or AppUserModelID (e.g. the special "File Explorer" pin).
    entry.appId = !entry.appUserModelId.isEmpty()
            ? entry.appUserModelId.toLower()
            : (!entry.exePath.isEmpty() ? lowerPath(entry.exePath) : lowerPath(shortcut.shortcutPath));
    entry.label = QFileInfo(shortcut.targetPath).completeBaseName();
    if (entry.label.isEmpty()) {
        entry.label = shortcutBaseName;
    }
    entry.iconHint = entry.label.left(1).toUpper();
    entry.pinned = true;
    entry.pinnedOnly = true;
    return entry;
}

QString DockModel::normalizeAppId(const QString& path) const
{
    return lowerPath(path);
}

QString DockModel::labelFromPath(const QString& path) const
{
    return QFileInfo(path).completeBaseName();
}

QPixmap pixmapFromHicon(HICON hIcon)
{
    if (!hIcon) {
        return {};
    }
    const QImage image = hiconToImage(hIcon);
    DestroyIcon(hIcon);
    return image.isNull() ? QPixmap() : QPixmap::fromImage(image);
}

QPixmap DockModel::extractFileIcon(const QString& exePath) const
{
    const QString nativePath = QDir::toNativeSeparators(exePath);
    if (nativePath.isEmpty()) {
        return {};
    }

    const LPCWSTR pathW = reinterpret_cast<LPCWSTR>(nativePath.utf16());

    // Prefer the shell jumbo/extra-large image lists (256/48 px). The old path
    // used SHGFI_LARGEICON (32 px) first, which looked soft when scaled up in the dock.
    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(pathW, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX)) {
        const int imageListIds[] = { SHIL_JUMBO, SHIL_EXTRALARGE };
        for (const int listId : imageListIds) {
            IImageList* imageList = nullptr;
            HRESULT hr = SHGetImageList(listId, IID_IImageList, reinterpret_cast<void**>(&imageList));
            if (FAILED(hr) || !imageList) {
                continue;
            }

            HICON hIcon = nullptr;
            hr = imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            imageList->Release();
            if (FAILED(hr) || !hIcon) {
                continue;
            }

            const QPixmap pixmap = pixmapFromHicon(hIcon);
            if (!pixmap.isNull() && pixmap.width() >= 48) {
                return pixmap;
            }
        }
    }

    // Embedded multi-size icons inside the executable.
    HICON largeIcon = nullptr;
    const UINT extracted = ExtractIconExW(pathW, 0, &largeIcon, nullptr, 1);
    if (extracted > 0 && largeIcon) {
        const QPixmap pixmap = pixmapFromHicon(largeIcon);
        if (!pixmap.isNull()) {
            return pixmap;
        }
    }

    SHFILEINFOW sfiLarge = {};
    if (SHGetFileInfoW(pathW, 0, &sfiLarge, sizeof(sfiLarge), SHGFI_ICON | SHGFI_LARGEICON)) {
        return pixmapFromHicon(sfiLarge.hIcon);
    }

    return {};
}

QString DockModel::ensureIconFile(const QString& appId, const QString& exePath) const
{
    const QByteArray hash = QCryptographicHash::hash(appId.toUtf8(), QCryptographicHash::Md5).toHex();
    // v2 suffix invalidates older 32 px caches from previous extraction logic.
    const QString filePath = QDir(m_iconCacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral("_256.png"));

    if (QFileInfo::exists(filePath)) {
        const QImage cached(filePath);
        if (!cached.isNull() && cached.width() >= 96) {
            return QUrl::fromLocalFile(filePath).toString();
        }
    }

    QPixmap pixmap = extractFileIcon(exePath);
    if (pixmap.isNull()) {
        return {};
    }

    pixmap.save(filePath, "PNG");
    return QUrl::fromLocalFile(filePath).toString();
}

QString DockModel::externalIconUrlForPath(const QString& path) const
{
    if (path.trimmed().isEmpty()) {
        return {};
    }

    const QString appId = normalizeAppId(path);
    const QString iconKey = appId.isEmpty() ? QFileInfo(path).absoluteFilePath() : appId;
    return ensureIconFile(iconKey, path);
}

int DockModel::appItemCount() const
{
    int count = 0;
    for (const auto& entry : m_entries) {
        if (entry.kind == QLatin1String("app")) {
            ++count;
        }
    }
    return count;
}

QString DockModel::downloadsFolderPath() const
{
    PWSTR raw = nullptr;
    QString result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &raw)) && raw) {
        result = QString::fromWCharArray(raw);
    }
    if (raw) {
        CoTaskMemFree(raw);
    }
    if (result.isEmpty()) {
        result = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    return result;
}

bool DockModel::recycleBinHasItems() const
{
    SHQUERYRBINFO info = {};
    info.cbSize = sizeof(info);
    const HRESULT hr = SHQueryRecycleBinW(nullptr, &info);
    return SUCCEEDED(hr) && info.i64NumItems > 0;
}

QString DockModel::ensureStockIconFile(const QString& cacheKey, int stockIconId) const
{
    const QByteArray hash = QCryptographicHash::hash(cacheKey.toUtf8(), QCryptographicHash::Md5).toHex();
    const QString filePath = QDir(m_iconCacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral("_stock256.png"));

    if (QFileInfo::exists(filePath)) {
        const QImage cached(filePath);
        if (!cached.isNull() && cached.width() >= 96) {
            return QUrl::fromLocalFile(filePath).toString();
        }
    }

    // Resolve the stock icon's index in the system image list, then pull the 256 px
    // jumbo variant the same way extractFileIcon() does for app icons.
    SHSTOCKICONINFO sii = {};
    sii.cbSize = sizeof(sii);
    QPixmap pixmap;
    if (SUCCEEDED(SHGetStockIconInfo(static_cast<SHSTOCKICONID>(stockIconId), SHGSI_SYSICONINDEX, &sii))) {
        const int imageListIds[] = { SHIL_JUMBO, SHIL_EXTRALARGE };
        for (const int listId : imageListIds) {
            IImageList* imageList = nullptr;
            if (FAILED(SHGetImageList(listId, IID_IImageList, reinterpret_cast<void**>(&imageList))) || !imageList) {
                continue;
            }
            HICON hIcon = nullptr;
            const HRESULT hr = imageList->GetIcon(sii.iSysImageIndex, ILD_TRANSPARENT, &hIcon);
            imageList->Release();
            if (SUCCEEDED(hr) && hIcon) {
                pixmap = pixmapFromHicon(hIcon);
                if (!pixmap.isNull() && pixmap.width() >= 48) {
                    break;
                }
            }
        }
    }

    if (pixmap.isNull()) {
        SHSTOCKICONINFO siiSmall = {};
        siiSmall.cbSize = sizeof(siiSmall);
        if (SUCCEEDED(SHGetStockIconInfo(static_cast<SHSTOCKICONID>(stockIconId), SHGSI_ICON | SHGSI_LARGEICON, &siiSmall))
            && siiSmall.hIcon) {
            pixmap = pixmapFromHicon(siiSmall.hIcon);
        }
    }

    if (pixmap.isNull()) {
        return {};
    }

    pixmap.save(filePath, "PNG");
    return QUrl::fromLocalFile(filePath).toString();
}

void DockModel::appendTrailingShellItems()
{
    // Downloads stack.
    DockItemEntry downloads;
    downloads.kind = QStringLiteral("downloads");
    downloads.appId = QStringLiteral("shell.downloads");
    downloads.label = QStringLiteral("Загрузки");
    const QString downloadsPath = downloadsFolderPath();
    downloads.launchPath = downloadsPath;
    downloads.iconHint = QStringLiteral("⬇");
    downloads.pinned = true;
    downloads.pinnedOnly = true;
    if (!downloadsPath.isEmpty()) {
        const QString iconUrl = ensureIconFile(QStringLiteral("shell.downloads"), downloadsPath);
        if (!iconUrl.isEmpty()) {
            downloads.iconUrl = iconUrl;
        }
    }
    m_entries.push_back(downloads);

    // Recycle Bin (Trash). Icon reflects empty vs full state.
    const bool full = recycleBinHasItems();
    DockItemEntry trash;
    trash.kind = QStringLiteral("trash");
    trash.appId = QStringLiteral("shell.trash");
    trash.label = QStringLiteral("Корзина");
    trash.iconHint = QStringLiteral("🗑");
    trash.pinned = true;
    trash.pinnedOnly = true;
    if (m_trashIconStyle == QLatin1String("macos")) {
        trash.iconUrl = full
            ? QStringLiteral("qrc:/src/MacDockShell/icons/trash_macos_full.png")
            : QStringLiteral("qrc:/src/MacDockShell/icons/trash_macos_empty.png");
    } else {
        trash.iconUrl = ensureStockIconFile(
            full ? QStringLiteral("shell.trash.full") : QStringLiteral("shell.trash.empty"),
            full ? SIID_RECYCLERFULL : SIID_RECYCLER);
    }
    m_entries.push_back(trash);
}

QString DockModel::explorerIconStyle() const
{
    return m_explorerIconStyle;
}

QString DockModel::explorerMacIconUrl() const
{
    return QStringLiteral("qrc:/src/MacDockShell/icons/finder_macos.png");
}

QString DockModel::explorerDefaultIconUrl() const
{
    return ensureIconFile(QStringLiteral("microsoft.windows.explorer"), defaultExplorerExePath());
}

QString DockModel::explorerDisplayLabel() const
{
    return m_explorerIconStyle == QLatin1String("macos")
            ? QStringLiteral("Finder")
            : QStringLiteral("File Explorer");
}

QString DockModel::resolveExplorerIconUrl(const DockItemEntry& entry) const
{
    if (m_explorerIconStyle == QLatin1String("macos")) {
        return explorerMacIconUrl();
    }

    const QString appId = entry.appId.isEmpty() ? QStringLiteral("microsoft.windows.explorer") : entry.appId;
    const QString iconSource = entry.exePath.isEmpty()
            ? (entry.launchPath.isEmpty() ? defaultExplorerExePath() : entry.launchPath)
            : entry.exePath;
    return ensureIconFile(appId, iconSource);
}

void DockModel::applyExplorerPresentation(DockItemEntry& entry) const
{
    entry.iconUrl = resolveExplorerIconUrl(entry);
    if (entry.pinnedOnly || (entry.pinned && !entry.running)) {
        entry.label = explorerDisplayLabel();
    }
    entry.iconHint = m_explorerIconStyle == QLatin1String("macos") ? QString() : QStringLiteral("F");
}

void DockModel::setExplorerIconStyle(const QString& style)
{
    const QString normalized = style == QLatin1String("macos")
            ? QLatin1String("macos")
            : QLatin1String("default");
    if (m_explorerIconStyle == normalized) {
        return;
    }

    m_explorerIconStyle = normalized;
    m_existingIconUrls.remove(QStringLiteral("microsoft.windows.explorer"));
    emit explorerIconStyleChanged();
    refresh();
}

void DockModel::setTrashIconStyle(const QString& style)
{
    const QString normalized = style == QLatin1String("macos")
            ? QStringLiteral("macos")
            : QStringLiteral("windows");
    if (m_trashIconStyle == normalized) {
        return;
    }
    m_trashIconStyle = normalized;
    refresh();
}

QString DockModel::trashWindowsIconUrl() const
{
    return ensureStockIconFile(QStringLiteral("shell.trash.empty"), SIID_RECYCLER);
}

QString DockModel::trashMacIconUrl() const
{
    return QStringLiteral("qrc:/src/MacDockShell/icons/trash_macos_empty.png");
}

void DockModel::upsertEntry(const DockItemEntry& entry)
{
    DockItemEntry mutableEntry = entry;

    // Special shell links (e.g. File Explorer) have no exe path; pull the icon
    // straight from the .lnk, which resolves to the proper shortcut icon.
    const QString iconSource = mutableEntry.exePath.isEmpty() ? mutableEntry.launchPath : mutableEntry.exePath;
    QString iconUrl = m_existingIconUrls.value(mutableEntry.appId);
    if (iconUrl.isEmpty()) {
        iconUrl = ensureIconFile(mutableEntry.appId, iconSource);
        if (!iconUrl.isEmpty()) {
            m_existingIconUrls.insert(mutableEntry.appId, iconUrl);
        }
    }
    mutableEntry.iconUrl = iconUrl;
    if (isExplorerEntry(mutableEntry)) {
        applyExplorerPresentation(mutableEntry);
    }

    for (auto& existing : m_entries) {
        const bool sameAppId = existing.appId == mutableEntry.appId;
        const bool sameExePath = !existing.exePath.isEmpty() && !mutableEntry.exePath.isEmpty()
                && lowerPath(existing.exePath) == lowerPath(mutableEntry.exePath);
        const bool sameLaunchPath = !existing.launchPath.isEmpty() && !mutableEntry.launchPath.isEmpty()
                && lowerPath(existing.launchPath) == lowerPath(mutableEntry.launchPath);
        const bool sameExeName = !existing.exePath.isEmpty() && !mutableEntry.exePath.isEmpty()
                && fileNameKey(existing.exePath) == fileNameKey(mutableEntry.exePath);

        if (sameAppId || sameExePath || sameLaunchPath || sameExeName) {
            const QString existingBefore = existing.appId;
            existing.pinned = existing.pinned || mutableEntry.pinned;
            existing.running = existing.running || mutableEntry.running;
            existing.active = existing.active || mutableEntry.active;
            existing.minimized = mutableEntry.minimized;
            existing.pinnedOnly = existing.running ? false : (existing.pinned && mutableEntry.pinnedOnly);

            if (!mutableEntry.appId.isEmpty())
                existing.appId = mutableEntry.appId;
            if (existing.appUserModelId.isEmpty())
                existing.appUserModelId = mutableEntry.appUserModelId;
            if (existing.label.isEmpty())
                existing.label = mutableEntry.label;
            if (!mutableEntry.windowTitle.isEmpty() && (existing.windowTitle.isEmpty() || existing.windowTitle == existing.label))
                existing.windowTitle = mutableEntry.windowTitle;
            if (existing.exePath.isEmpty())
                existing.exePath = mutableEntry.exePath;
            if (existing.launchPath.isEmpty())
                existing.launchPath = mutableEntry.launchPath;
            if (existing.launchArguments.isEmpty())
                existing.launchArguments = mutableEntry.launchArguments;
            if (existing.workingDirectory.isEmpty())
                existing.workingDirectory = mutableEntry.workingDirectory;
            if (existing.iconHint.isEmpty())
                existing.iconHint = mutableEntry.iconHint;
            if (existing.iconUrl.isEmpty())
                existing.iconUrl = mutableEntry.iconUrl;
            if (mutableEntry.hwndValue != 0)
                existing.hwndValue = mutableEntry.hwndValue;
            if (existing.running) {
                existing.pinnedOnly = false;
                if (existing.launchPath.isEmpty())
                    existing.launchPath = existing.exePath;
            }

            if (isExplorerEntry(existing)) {
                applyExplorerPresentation(existing);
            }

            if (existingBefore != existing.appId) {
                emit logMessage(QStringLiteral("Merged dock item by fallback: before=%1 after=%2 exe=%3 launch=%4")
                                .arg(existingBefore, existing.appId, existing.exePath, existing.launchPath));
            }
            return;
        }
    }

    m_entries.push_back(mutableEntry);
}

void DockModel::emitActiveWindowState()
{
    QString activeTitle;
    QString activeLabel;
    bool active = false;

    for (const auto& entry : m_entries) {
        if (entry.running && entry.active) {
            activeTitle = entry.windowTitle;
            activeLabel = entry.label;
            active = true;
            break;
        }
    }

    if (activeTitle != m_lastActiveWindowTitle || activeLabel != m_lastActiveAppLabel || active != m_lastActiveState) {
        m_lastActiveWindowTitle = activeTitle;
        m_lastActiveAppLabel = activeLabel;
        m_lastActiveState = active;
        emit activeAppWindowChanged(activeTitle, activeLabel, active);
    }
}
