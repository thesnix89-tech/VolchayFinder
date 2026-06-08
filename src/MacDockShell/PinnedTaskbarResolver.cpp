#include "PinnedTaskbarResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QSettings>
#include <QSet>
#include <QRegularExpression>

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

QString extractLnkFileName(const QString& value)
{
    static const QRegularExpression trailingLnk(
        QStringLiteral("([\\w \\.\\(\\)\\-~]{2,80}\\.lnk)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = trailingLnk.match(value.trimmed());
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QFileInfo(value).fileName();
}

void appendFavoriteLnkName(QStringList* names, const QString& rawValue)
{
    if (!names || rawValue.isEmpty()) {
        return;
    }

    const QString fileName = extractLnkFileName(rawValue);
    if (!fileName.endsWith(QStringLiteral(".lnk"), Qt::CaseInsensitive)) {
        return;
    }
    if (fileName.size() < 5 || !fileName.at(0).isLetterOrNumber()) {
        return;
    }

    for (const QString& existing : *names) {
        if (existing.compare(fileName, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    names->append(fileName);
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

void mergeTaskbarFavoriteIndex(TaskbarFavoriteIndex* target, const TaskbarFavoriteIndex& other)
{
    if (!target) {
        return;
    }

    for (const QString& path : other.fullShortcutPaths) {
        appendUnique(&target->fullShortcutPaths, path);
    }
    for (const QString& name : other.shortcutFileNames) {
        appendFavoriteLnkName(&target->shortcutFileNames, name);
    }
    for (const QString& id : other.appUserModelIds) {
        appendUnique(&target->appUserModelIds, id);
    }
    for (const QString& exe : other.executablePaths) {
        appendUnique(&target->executablePaths, exe);
    }

    target->valid = !target->shortcutFileNames.isEmpty()
            || !target->fullShortcutPaths.isEmpty()
            || !target->appUserModelIds.isEmpty()
            || !target->executablePaths.isEmpty();
}

void appendShortcutNamesFromWideBlob(const QByteArray& favorites, TaskbarFavoriteIndex* index)
{
    if (!index || favorites.isEmpty()) {
        return;
    }

    QString current;
    auto flush = [&]() {
        appendFavoriteLnkName(&index->shortcutFileNames, current);
        current.clear();
    };

    for (int i = 0; i + 1 < favorites.size(); i += 2) {
        const auto byte0 = static_cast<uchar>(favorites.at(i));
        const auto byte1 = static_cast<uchar>(favorites.at(i + 1));
        const ushort code = static_cast<ushort>(byte0 | (static_cast<ushort>(byte1) << 8));
        const QChar ch(code);
        if (ch.isLetterOrNumber() || ch == QLatin1Char(' ') || ch == QLatin1Char('.')
            || ch == QLatin1Char('-') || ch == QLatin1Char('_') || ch == QLatin1Char('(')
            || ch == QLatin1Char(')') || ch == QLatin1Char('~')) {
            current.append(ch);
        } else {
            flush();
        }
    }
    flush();
}

void appendLnkNamesFromWideTextScan(const QByteArray& favorites, TaskbarFavoriteIndex* index)
{
    if (!index || favorites.size() < 4) {
        return;
    }

    const auto* utf16 = reinterpret_cast<const char16_t*>(favorites.constData());
    const int charCount = favorites.size() / static_cast<int>(sizeof(char16_t));
    const QString wide = QString::fromUtf16(utf16, charCount);
    static const QRegularExpression lnkToken(
        QStringLiteral("[\\w \\.\\(\\)\\-~]{2,80}\\.lnk"),
        QRegularExpression::CaseInsensitiveOption);
    auto it = lnkToken.globalMatch(wide);
    while (it.hasNext()) {
        appendFavoriteLnkName(&index->shortcutFileNames, it.next().captured(0));
    }

    static const QRegularExpression exeToken(
        QStringLiteral("[A-Za-z]:\\\\[^\\x00\\n\\r]{2,220}\\.exe"),
        QRegularExpression::CaseInsensitiveOption);
    auto exeIt = exeToken.globalMatch(wide);
    while (exeIt.hasNext()) {
        const QString exePath = QDir::toNativeSeparators(exeIt.next().captured(0).trimmed());
        appendUnique(&index->executablePaths, exePath);
    }
}

TaskbarFavoriteIndex buildTaskbarFavoriteIndex(const QByteArray& favorites)
{
    TaskbarFavoriteIndex index;
    appendLnkNamesFromWideTextScan(favorites, &index);
    appendShortcutNamesFromWideBlob(favorites, &index);
    for (const QString& string : extractUtf16Strings(favorites)) {
        const QString trimmed = string.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (trimmed.endsWith(QStringLiteral(".lnk"), Qt::CaseInsensitive)) {
            appendFavoriteLnkName(&index.shortcutFileNames, trimmed);
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
            appendFavoriteLnkName(&index.shortcutFileNames, QStringLiteral("Google Chrome.lnk"));
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

QByteArray readTaskbandBinaryValue(const wchar_t* valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband",
                      0,
                      KEY_READ,
                      &key) != ERROR_SUCCESS) {
        return {};
    }

    DWORD type = 0;
    DWORD size = 0;
    const LSTATUS querySize = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &size);
    if (querySize != ERROR_SUCCESS || type != REG_BINARY || size == 0) {
        RegCloseKey(key);
        return {};
    }

    QByteArray data;
    data.resize(static_cast<int>(size));
    DWORD copiedSize = size;
    const LSTATUS queryData = RegQueryValueExW(key,
                                               valueName,
                                               nullptr,
                                               &type,
                                               reinterpret_cast<LPBYTE>(data.data()),
                                               &copiedSize);
    RegCloseKey(key);
    if (queryData != ERROR_SUCCESS) {
        return {};
    }
    data.resize(static_cast<int>(copiedSize));
    return data;
}

TaskbarFavoriteIndex readTaskbarFavoriteIndex()
{
    TaskbarFavoriteIndex merged;
    const QByteArray favoritesResolve = readTaskbandBinaryValue(L"FavoritesResolve");
    const QByteArray favorites = readTaskbandBinaryValue(L"Favorites");
    mergeTaskbarFavoriteIndex(&merged, buildTaskbarFavoriteIndex(favoritesResolve));
    mergeTaskbarFavoriteIndex(&merged, buildTaskbarFavoriteIndex(favorites));

    if (!merged.valid && !favoritesResolve.isEmpty()) {
        merged = buildTaskbarFavoriteIndex(favoritesResolve);
    }
    if (!merged.valid && !favorites.isEmpty()) {
        merged = buildTaskbarFavoriteIndex(favorites);
    }

    return merged;
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

bool matchesExplicitDockPinKeys(const PinnedShortcutEntry& entry, const QStringList& explicitDockPinKeys)
{
    if (explicitDockPinKeys.isEmpty()) {
        return false;
    }

    QStringList entryKeys;
    if (!entry.shortcutPath.isEmpty()) {
        entryKeys << normalizePathKey(entry.shortcutPath);
        entryKeys << QFileInfo(entry.shortcutPath).fileName().toLower();
    }
    if (!entry.targetPath.isEmpty()) {
        entryKeys << normalizePathKey(entry.targetPath);
    }
    if (!entry.appUserModelId.isEmpty()) {
        entryKeys << entry.appUserModelId.toLower();
    }
    entryKeys.removeDuplicates();

    for (const QString& key : explicitDockPinKeys) {
        const QString normalizedKey = key.toLower();
        for (const QString& entryKey : entryKeys) {
            if (entryKey == normalizedKey) {
                return true;
            }
        }
    }
    return false;
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
        const QString normalizedFavorite = extractLnkFileName(favoriteFile);
        if (normalizedFavorite.compare(shortcutFile, Qt::CaseInsensitive) == 0) {
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

QList<PinnedShortcutEntry> PinnedTaskbarResolver::resolveDockPinnedShortcuts(const QStringList& explicitDockPinKeys) const
{
    const QList<PinnedShortcutEntry> windowsPins = resolvePinnedShortcuts();
    if (explicitDockPinKeys.isEmpty()) {
        return windowsPins;
    }

    QList<PinnedShortcutEntry> merged = windowsPins;
    QSet<QString> seenShortcutPaths;
    seenShortcutPaths.reserve(windowsPins.size());
    for (const PinnedShortcutEntry& entry : windowsPins) {
        seenShortcutPaths.insert(normalizePathKey(entry.shortcutPath));
    }

    for (const PinnedShortcutEntry& entry : collectFolderShortcuts()) {
        const QString shortcutKey = normalizePathKey(entry.shortcutPath);
        if (seenShortcutPaths.contains(shortcutKey)) {
            continue;
        }
        if (!matchesExplicitDockPinKeys(entry, explicitDockPinKeys)) {
            continue;
        }
        merged.push_back(entry);
        seenShortcutPaths.insert(shortcutKey);
    }

    return merged;
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
    filtered.reserve(favoriteIndex.shortcutFileNames.size());
    QSet<QString> matchedFileNames;

    for (const QString& favoriteFile : favoriteIndex.shortcutFileNames) {
        const QString favoriteName = extractLnkFileName(favoriteFile);
        if (favoriteName.isEmpty() || matchedFileNames.contains(favoriteName.toLower())) {
            continue;
        }

        for (const PinnedShortcutEntry& entry : folderShortcuts) {
            if (QFileInfo(entry.shortcutPath).fileName().compare(favoriteName, Qt::CaseInsensitive) != 0) {
                continue;
            }
            filtered.push_back(entry);
            matchedFileNames.insert(favoriteName.toLower());
            break;
        }
    }

    for (const PinnedShortcutEntry& entry : folderShortcuts) {
        const QString fileName = QFileInfo(entry.shortcutPath).fileName().toLower();
        if (matchedFileNames.contains(fileName)) {
            continue;
        }
        if (matchesTaskbarFavorite(entry, favoriteIndex)) {
            filtered.push_back(entry);
            matchedFileNames.insert(fileName);
        }
    }

    QStringList favoritePreview = favoriteIndex.shortcutFileNames;
    if (favoritePreview.size() > 6) {
        favoritePreview = favoritePreview.mid(0, 6);
        favoritePreview.append(QStringLiteral("..."));
    }

    emit logMessage(QStringLiteral("Taskbar pins resolved: %1 active of %2 folder shortcuts (%3 registry names: %4)")
                        .arg(filtered.size())
                        .arg(folderShortcuts.size())
                        .arg(favoriteIndex.shortcutFileNames.size())
                        .arg(favoritePreview.join(QStringLiteral(", "))));
    return filtered;
}

bool isNumberedDuplicateLnk(const QString& fileName)
{
    static const QRegularExpression numbered(
        QStringLiteral(" \\(\\d+\\)\\.lnk$"),
        QRegularExpression::CaseInsensitiveOption);
    return numbered.match(fileName).hasMatch();
}

QList<PinnedShortcutEntry> PinnedTaskbarResolver::resolvePinnedShortcutsForDockSync() const
{
    QList<PinnedShortcutEntry> merged = resolvePinnedShortcuts();
    QSet<QString> seenPaths;
    seenPaths.reserve(merged.size());
    for (const PinnedShortcutEntry& entry : merged) {
        seenPaths.insert(normalizePathKey(entry.shortcutPath));
    }

    // Some Windows pins (e.g. Spotify) exist as TaskBar .lnk files but are missing from
    // the Taskband registry blob — include canonical TaskBar shortcuts on manual sync.
    for (const PinnedShortcutEntry& entry : collectFolderShortcuts()) {
        if (!entry.shortcutPath.contains(QStringLiteral("TaskBar"), Qt::CaseInsensitive)) {
            continue;
        }

        const QString pathKey = normalizePathKey(entry.shortcutPath);
        if (seenPaths.contains(pathKey)) {
            continue;
        }

        const QString fileName = QFileInfo(entry.shortcutPath).fileName();
        if (isNumberedDuplicateLnk(fileName)) {
            continue;
        }

        merged.push_back(entry);
        seenPaths.insert(pathKey);
    }

    emit logMessage(QStringLiteral("Taskbar sync scan: %1 pins (registry + TaskBar folder).")
                        .arg(merged.size()));
    return merged;
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

    const QList<PinnedShortcutEntry> existing = collectFolderShortcuts();
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
