#include "DockModel.h"
#include "PinnedTaskbarResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QCryptographicHash>
#include <QUrl>
#include <QSet>

#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <commctrl.h>

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

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* context = reinterpret_cast<EnumContext*>(lParam);

    if (!IsWindowVisible(hwnd)) {
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

    const QString exeName = QFileInfo(exePath).fileName().toLower();
    if (exeName == "explorer.exe" || exeName == "shellexperiencehost.exe" || exeName == "searchhost.exe" || exeName == "startmenuexperiencehost.exe") {
        return TRUE;
    }

    const QString title = windowText(hwnd);

    DockItemEntry entry;
    entry.exePath = exePath;
    entry.launchPath = exePath;
    entry.appId = lowerPath(exePath);
    entry.label = QFileInfo(exePath).completeBaseName();
    entry.windowTitle = title.isEmpty() ? entry.label : title;
    entry.iconHint = QFileInfo(exePath).completeBaseName().left(1).toUpper();
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
    case ClickableRole: return item.running || item.pinned;
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
        { ClickableRole, "clickable" }
    };
}

void DockModel::refresh()
{
    beginResetModel();
    m_entries.clear();
    m_existingIconUrls.clear();
    loadPinnedApps();
    scanWindows();
    endResetModel();
    emitActiveWindowState();
    emit logMessage(QStringLiteral("DockModel refreshed. Items: %1").arg(m_entries.size()));
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

    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
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

    ShowWindow(hwnd, SW_MINIMIZE);
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

    HINSTANCE result = ShellExecuteW(nullptr,
                                     L"open",
                                     reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                     nativeArguments.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(nativeArguments.utf16()),
                                     nativeWorkingDirectory.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(nativeWorkingDirectory.utf16()),
                                     SW_SHOWNORMAL);
    const quintptr resultCode = reinterpret_cast<quintptr>(result);
    if (resultCode <= 32) {
        emit logMessage(QStringLiteral("launchIndex: failed for %1 code=%2").arg(launchPath).arg(resultCode));
        return;
    }

    emit logMessage(QStringLiteral("Launched pinned app: %1").arg(launchPath));
}

void DockModel::loadPinnedApps()
{
    const QList<PinnedShortcutEntry> shortcuts = m_pinnedResolver->resolvePinnedShortcuts();
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
        if (shortcut.targetPath.isEmpty()) {
            continue;
        }

        DockItemEntry entry = makePinnedEntry(shortcut);
        m_pinnedPaths << entry.exePath;
        upsertEntry(entry);
        if (pinnedDirty) {
            emit logMessage(QStringLiteral("Pinned app loaded: target=%1 launch=%2 aumid=%3 args=%4")
                            .arg(entry.exePath,
                                 entry.launchPath,
                                 entry.appUserModelId.isEmpty() ? QStringLiteral("<none>") : entry.appUserModelId,
                                 entry.launchArguments.isEmpty() ? QStringLiteral("<none>") : entry.launchArguments));
        }
    }
}

void DockModel::scanWindows()
{
    EnumContext context;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&context));
    for (const auto& entry : context.entries) {
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
    entry.appId = !shortcut.appUserModelId.isEmpty() ? shortcut.appUserModelId.toLower() : lowerPath(shortcut.targetPath);
    entry.label = QFileInfo(shortcut.targetPath).completeBaseName();
    if (entry.label.isEmpty())
        entry.label = QFileInfo(shortcut.shortcutPath).completeBaseName();
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

QPixmap DockModel::extractFileIcon(const QString& exePath) const
{
    SHFILEINFOW fileInfo = {};
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(exePath.utf16()), 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON)) {
        return {};
    }

    QImage image = hiconToImage(fileInfo.hIcon);
    DestroyIcon(fileInfo.hIcon);
    if (image.isNull()) {
        return {};
    }

    return QPixmap::fromImage(image);
}

QString DockModel::ensureIconFile(const QString& appId, const QString& exePath) const
{
    const QByteArray hash = QCryptographicHash::hash(appId.toUtf8(), QCryptographicHash::Md5).toHex();
    const QString filePath = QDir(m_iconCacheDir).filePath(QString::fromLatin1(hash) + ".png");

    if (QFileInfo::exists(filePath)) {
        return QUrl::fromLocalFile(filePath).toString();
    }

    QPixmap pixmap = extractFileIcon(exePath);
    if (pixmap.isNull()) {
        return {};
    }

    pixmap.save(filePath, "PNG");
    return QUrl::fromLocalFile(filePath).toString();
}

void DockModel::upsertEntry(const DockItemEntry& entry)
{
    DockItemEntry mutableEntry = entry;

    QString iconUrl = m_existingIconUrls.value(mutableEntry.appId);
    if (iconUrl.isEmpty()) {
        iconUrl = ensureIconFile(mutableEntry.appId, mutableEntry.exePath);
        if (!iconUrl.isEmpty()) {
            m_existingIconUrls.insert(mutableEntry.appId, iconUrl);
        }
    }
    mutableEntry.iconUrl = iconUrl;

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
