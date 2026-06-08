#include "PinnedTaskbarResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSettings>

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <objbase.h>

namespace {

QString normalizePathKey(const QString& path)
{
    return QDir::fromNativeSeparators(path).toLower();
}

QStringList extractUtf16Strings(const QByteArray& data)
{
    QStringList strings;
    QString current;
    auto flush = [&]() {
        const QString trimmed = current.trimmed();
        if (!trimmed.isEmpty()) {
            strings.append(trimmed);
        }
        current.clear();
    };

    for (int i = 0; i + 1 < data.size(); i += 2) {
        const auto byte0 = static_cast<uchar>(data.at(i));
        const auto byte1 = static_cast<uchar>(data.at(i + 1));
        const ushort code = static_cast<ushort>(byte0 | (static_cast<ushort>(byte1) << 8));
        if (code == 0) {
            flush();
            continue;
        }
        if (code < 0x20 && code != '\t') {
            flush();
            continue;
        }
        current.append(QChar(code));
    }
    flush();
    return strings;
}

bool looksLikePinnedShortcutPath(const QString& value)
{
    const QString lower = value.toLower();
    return lower.endsWith(QStringLiteral(".lnk"))
        && (lower.contains(QStringLiteral("user pinned\\taskbar"))
            || lower.contains(QStringLiteral("user pinned/implicitappshortcuts"))
            || lower.contains(QStringLiteral("implicitappshortcuts")));
}

bool looksLikeAppUserModelId(const QString& value)
{
    if (value.size() < 3 || value.size() > 180) {
        return false;
    }
    if (value.contains(QLatin1Char('\\')) || value.contains(QLatin1Char('/'))) {
        return false;
    }
    if (value.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive)
        || value.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)
        || value.endsWith(QStringLiteral(".lnk"), Qt::CaseInsensitive)) {
        return false;
    }
    if (value == QStringLiteral("Chrome")) {
        return true;
    }
    return value.contains(QLatin1Char('.'));
}

struct TaskbarFavoriteIndex
{
    QStringList fullShortcutPaths;
    QStringList shortcutFileNames;
    QStringList appUserModelIds;
    QStringList executablePaths;
    bool valid = false;
};

void appendUnique(QStringList* list, const QString& value)
{
    if (!list || value.isEmpty()) {
        return;
    }
    for (const QString& existing : *list) {
        if (existing.compare(value, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    list->append(value);
}

TaskbarFavoriteIndex buildTaskbarFavoriteIndex(const QByteArray& favorites)
{
    TaskbarFavoriteIndex index;
    for (const QString& string : extractUtf16Strings(favorites)) {
        const QString trimmed = string.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (trimmed.endsWith(QStringLiteral(".lnk"), Qt::CaseInsensitive)) {
            appendUnique(&index.shortcutFileNames, trimmed);
            if (looksLikePinnedShortcutPath(trimmed)) {
                appendUnique(&index.fullShortcutPaths,
                              QDir::toNativeSeparators(QDir::fromNativeSeparators(trimmed)));
            }
            continue;
        }

        if (trimmed.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            appendUnique(&index.executablePaths,
                          QDir::toNativeSeparators(QFileInfo(trimmed).absoluteFilePath()));
            continue;
        }

        if (trimmed == QStringLiteral("Chrome")) {
            appendUnique(&index.appUserModelIds, trimmed);
            appendUnique(&index.shortcutFileNames, QStringLiteral("Google Chrome.lnk"));
            continue;
        }

        if (looksLikeAppUserModelId(trimmed)) {
            appendUnique(&index.appUserModelIds, trimmed);
        }
    }

    index.valid = !index.shortcutFileNames.isEmpty()
            || !index.fullShortcutPaths.isEmpty()
            || !index.appUserModelIds.isEmpty()
            || !index.executablePaths.isEmpty();
    return index;
}

TaskbarFavoriteIndex readTaskbarFavoriteIndex()
{
    QSettings taskband(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband"),
                       QSettings::NativeFormat);
    QByteArray favorites = taskband.value(QStringLiteral("FavoritesResolve")).toByteArray();
    if (favorites.isEmpty()) {
        favorites = taskband.value(QStringLiteral("Favorites")).toByteArray();
    }

    TaskbarFavoriteIndex primary = buildTaskbarFavoriteIndex(favorites);
    if (primary.valid) {
        return primary;
    }

    const QByteArray fallback = taskband.value(QStringLiteral("Favorites")).toByteArray();
    return buildTaskbarFavoriteIndex(fallback);
}

} // namespace

PinnedTaskbarResolver::PinnedTaskbarResolver(QObject* parent)
    : QObject(parent)
{
}

QString PinnedTaskbarResolver::pinnedTaskbarDir() const
{
    const QString appData = qEnvironmentVariable("APPDATA");
    return QDir(appData).filePath("Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar");
}

PinnedShortcutEntry PinnedTaskbarResolver::resolveShortcut(const QString& shortcutPath) const
{
    PinnedShortcutEntry entry;
    entry.shortcutPath = shortcutPath;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool mustUninit = SUCCEEDED(hr);
    const bool comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comReady) {
        return entry;
    }

    IShellLinkW* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr) || !shellLink) {
        if (mustUninit)
            CoUninitialize();
        return entry;
    }

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (FAILED(hr) || !persistFile) {
        shellLink->Release();
        if (mustUninit)
            CoUninitialize();
        return entry;
    }

    hr = persistFile->Load(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), STGM_READ);
    if (FAILED(hr)) {
        const QString shortcutName = QFileInfo(shortcutPath).completeBaseName();
        if (shortcutName.compare(QStringLiteral("File Explorer"), Qt::CaseInsensitive) == 0) {
            entry.appUserModelId = QStringLiteral("Microsoft.Windows.Explorer");
            entry.iconLocation = QStringLiteral("%windir%\\explorer.exe,0");
        }
        persistFile->Release();
        shellLink->Release();
        if (mustUninit)
            CoUninitialize();
        return entry;
    }

    wchar_t target[MAX_PATH * 4] = {};
    WIN32_FIND_DATAW findData = {};
    hr = shellLink->GetPath(target, static_cast<int>(std::size(target)), &findData, SLGP_RAWPATH);
    if (SUCCEEDED(hr) && target[0] != L'\0') {
        entry.targetPath = QString::fromWCharArray(target);
    }

    wchar_t arguments[MAX_PATH * 4] = {};
    hr = shellLink->GetArguments(arguments, static_cast<int>(std::size(arguments)));
    if (SUCCEEDED(hr) && arguments[0] != L'\0') {
        entry.arguments = QString::fromWCharArray(arguments);
    }

    wchar_t workdir[MAX_PATH * 4] = {};
    hr = shellLink->GetWorkingDirectory(workdir, static_cast<int>(std::size(workdir)));
    if (SUCCEEDED(hr) && workdir[0] != L'\0') {
        entry.workingDirectory = QString::fromWCharArray(workdir);
    }

    wchar_t iconPath[MAX_PATH * 4] = {};
    int iconIndex = 0;
    hr = shellLink->GetIconLocation(iconPath, static_cast<int>(std::size(iconPath)), &iconIndex);
    if (SUCCEEDED(hr) && iconPath[0] != L'\0') {
        entry.iconLocation = QString::fromWCharArray(iconPath);
        entry.iconIndex = iconIndex;
    }

    IPropertyStore* propertyStore = nullptr;
    hr = shellLink->QueryInterface(IID_PPV_ARGS(&propertyStore));
    if (SUCCEEDED(hr) && propertyStore) {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(propertyStore->GetValue(PKEY_AppUserModel_ID, &value))) {
            if (value.vt == VT_LPWSTR && value.pwszVal) {
                entry.appUserModelId = QString::fromWCharArray(value.pwszVal);
            }
        }
        PropVariantClear(&value);
        propertyStore->Release();
    }

    persistFile->Release();
    shellLink->Release();
    if (mustUninit)
        CoUninitialize();
    return entry;
}

bool matchesTaskbarFavorite(const PinnedShortcutEntry& entry, const TaskbarFavoriteIndex& favorites)
{
    if (!favorites.valid) {
        return true;
    }

    const QString shortcutPath = QDir::toNativeSeparators(QFileInfo(entry.shortcutPath).absoluteFilePath());
    const QString shortcutKey = normalizePathKey(shortcutPath);
    const QString shortcutFile = QFileInfo(shortcutPath).fileName();

    for (const QString& favoritePath : favorites.fullShortcutPaths) {
        if (normalizePathKey(favoritePath) == shortcutKey) {
            return true;
        }
    }

    for (const QString& favoriteFile : favorites.shortcutFileNames) {
        if (favoriteFile.compare(shortcutFile, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    if (!entry.appUserModelId.isEmpty()) {
        for (const QString& favoriteId : favorites.appUserModelIds) {
            if (entry.appUserModelId.compare(favoriteId, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    const QString targetPath = QDir::toNativeSeparators(QFileInfo(entry.targetPath).absoluteFilePath());
    const QString targetKey = normalizePathKey(targetPath);
    for (const QString& favoriteExe : favorites.executablePaths) {
        if (normalizePathKey(favoriteExe) == targetKey) {
            return true;
        }
    }

    return false;
}

QList<PinnedShortcutEntry> PinnedTaskbarResolver::collectFolderShortcuts() const
{
    QList<PinnedShortcutEntry> results;
    const QString appData = qEnvironmentVariable("APPDATA");
    const QString basePinnedPath = QDir(appData).filePath("Microsoft/Internet Explorer/Quick Launch/User Pinned");

    QStringList targetDirs;
    targetDirs << QDir(basePinnedPath).filePath("TaskBar");
    targetDirs << QDir(basePinnedPath).filePath("ImplicitAppShortcuts");

    auto appendShortcut = [&results, this](const QString& filePath) {
        if (filePath.isEmpty()) {
            return;
        }
        const QString nativePath = QDir::toNativeSeparators(filePath);
        const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            return;
        }
        for (const PinnedShortcutEntry& existing : results) {
            if (existing.shortcutPath.compare(filePath, Qt::CaseInsensitive) == 0) {
                return;
            }
        }
        results.push_back(resolveShortcut(filePath));
    };

    for (const QString& dirPath : targetDirs) {
        if (!QDir(dirPath).exists()) {
            continue;
        }

        const QFileInfo dirInfo(dirPath);
        if (dirInfo.fileName().compare(QStringLiteral("TaskBar"), Qt::CaseInsensitive) == 0) {
            const QDir taskBarDir(dirPath);
            const QStringList lnkFiles = taskBarDir.entryList(QStringList() << QStringLiteral("*.lnk"),
                                                                QDir::Files,
                                                                QDir::Name);
            for (const QString& fileName : lnkFiles) {
                appendShortcut(taskBarDir.absoluteFilePath(fileName));
            }
            appendShortcut(taskBarDir.absoluteFilePath(QStringLiteral("File Explorer.lnk")));
            continue;
        }

        const QDirIterator::IteratorFlags flags = QDirIterator::Subdirectories;
        QDirIterator it(dirPath, QStringList() << QStringLiteral("*.lnk"), QDir::Files, flags);
        while (it.hasNext()) {
            appendShortcut(it.next());
        }
    }

    return results;
}

QList<PinnedShortcutEntry> PinnedTaskbarResolver::resolveAllFolderShortcuts() const
{
    return collectFolderShortcuts();
}

QList<PinnedShortcutEntry> PinnedTaskbarResolver::resolvePinnedShortcuts() const
{
    const QList<PinnedShortcutEntry> folderShortcuts = collectFolderShortcuts();
    const TaskbarFavoriteIndex favoriteIndex = readTaskbarFavoriteIndex();
    if (!favoriteIndex.valid) {
        emit logMessage(QStringLiteral("Taskband Favorites unavailable; falling back to TaskBar folder scan."));
        QList<PinnedShortcutEntry> taskBarOnly;
        taskBarOnly.reserve(folderShortcuts.size());
        for (const PinnedShortcutEntry& entry : folderShortcuts) {
            if (entry.shortcutPath.contains(QStringLiteral("TaskBar"), Qt::CaseInsensitive)) {
                taskBarOnly.push_back(entry);
            }
        }
        return taskBarOnly;
    }

    QList<PinnedShortcutEntry> filtered;
    filtered.reserve(folderShortcuts.size());
    for (const PinnedShortcutEntry& entry : folderShortcuts) {
        if (matchesTaskbarFavorite(entry, favoriteIndex)) {
            filtered.push_back(entry);
        }
    }

    emit logMessage(QStringLiteral("Taskbar pins resolved: %1 active of %2 folder shortcuts.")
                        .arg(filtered.size())
                        .arg(folderShortcuts.size()));
    return filtered;
}

QString PinnedTaskbarResolver::uniqueShortcutPath(const QString& destDir, const QString& baseName) const
{
    QString sanitized = baseName.trimmed();
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("Pinned App");
    }

    QString candidate = QDir(destDir).filePath(sanitized + QStringLiteral(".lnk"));
    int suffix = 2;
    while (QFileInfo::exists(candidate)) {
        candidate = QDir(destDir).filePath(QStringLiteral("%1 (%2).lnk").arg(sanitized).arg(suffix++));
    }
    return candidate;
}

QString PinnedTaskbarResolver::copyShortcutToTaskbar(const QString& shortcutPath, const QString& destDir) const
{
    const QString nativeShortcut = QDir::toNativeSeparators(QFileInfo(shortcutPath).absoluteFilePath());
    const QString taskbarDir = QDir::toNativeSeparators(QDir(destDir).absolutePath());
    if (nativeShortcut.startsWith(taskbarDir, Qt::CaseInsensitive)) {
        return nativeShortcut;
    }

    const QString destPath = uniqueShortcutPath(destDir, QFileInfo(shortcutPath).completeBaseName());
    if (!CopyFileW(reinterpret_cast<LPCWSTR>(nativeShortcut.utf16()),
                   reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(destPath).utf16()),
                   FALSE)) {
        emit logMessage(QStringLiteral("Failed to copy shortcut to taskbar folder: %1").arg(shortcutPath));
        return {};
    }
    return destPath;
}

QString PinnedTaskbarResolver::createShortcutForExecutable(const QString& exePath, const QString& destDir) const
{
    const QString nativeExe = QDir::toNativeSeparators(QFileInfo(exePath).absoluteFilePath());
    const QString destPath = uniqueShortcutPath(destDir, QFileInfo(exePath).completeBaseName());

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool mustUninit = SUCCEEDED(hr);
    const bool comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comReady) {
        return {};
    }

    IShellLinkW* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr) || !shellLink) {
        if (mustUninit) {
            CoUninitialize();
        }
        return {};
    }

    shellLink->SetPath(reinterpret_cast<LPCWSTR>(nativeExe.utf16()));
    shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(nativeExe.utf16()), 0);
    const QString workDir = QFileInfo(nativeExe).absolutePath();
    shellLink->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(workDir).utf16()));

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (FAILED(hr) || !persistFile) {
        shellLink->Release();
        if (mustUninit) {
            CoUninitialize();
        }
        return {};
    }

    const std::wstring nativeDest = QDir::toNativeSeparators(destPath).toStdWString();
    hr = persistFile->Save(nativeDest.c_str(), TRUE);
    persistFile->Release();
    shellLink->Release();
    if (mustUninit) {
        CoUninitialize();
    }

    if (FAILED(hr)) {
        emit logMessage(QStringLiteral("Failed to create shortcut for executable: %1").arg(exePath));
        return {};
    }

    return destPath;
}

QString PinnedTaskbarResolver::createPinFromPath(const QString& sourcePath) const
{
    const QFileInfo info(sourcePath);
    if (!info.exists()) {
        emit logMessage(QStringLiteral("Pin rejected, path does not exist: %1").arg(sourcePath));
        return {};
    }

    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    const QString extension = info.suffix().toLower();
    const QString destDir = pinnedTaskbarDir();
    QDir().mkpath(destDir);

    const QList<PinnedShortcutEntry> existing = resolvePinnedShortcuts();
    for (const PinnedShortcutEntry& entry : existing) {
        if (entry.shortcutPath.compare(nativePath, Qt::CaseInsensitive) == 0) {
            return entry.shortcutPath;
        }
        if (!entry.targetPath.isEmpty()
            && entry.targetPath.compare(nativePath, Qt::CaseInsensitive) == 0) {
            return entry.shortcutPath;
        }
    }

    if (extension == QLatin1String("lnk")) {
        return copyShortcutToTaskbar(nativePath, destDir);
    }

    if (extension == QLatin1String("exe")
        || extension == QLatin1String("bat")
        || extension == QLatin1String("cmd")) {
        return createShortcutForExecutable(nativePath, destDir);
    }

    emit logMessage(QStringLiteral("Pin rejected, unsupported file type: %1").arg(sourcePath));
    return {};
}

QStringList PinnedTaskbarResolver::resolvePinnedExecutablePaths() const
{
    QStringList results;
    const QList<PinnedShortcutEntry> shortcuts = resolvePinnedShortcuts();
    for (const PinnedShortcutEntry& shortcut : shortcuts) {
        if (!shortcut.targetPath.isEmpty() && QFileInfo::exists(shortcut.targetPath)) {
            results << shortcut.targetPath;
        }
    }

    results.removeDuplicates();
    return results;
}
