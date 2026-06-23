#include "SpotlightModel.h"

#include "IconUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <iterator>

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <objbase.h>

namespace {

constexpr int kMaxResults = 8;

struct ShortcutDetails
{
    QString targetPath;
    QString arguments;
    QString workingDirectory;
};

QString normalizedKey(const QString& value)
{
    return QDir::fromNativeSeparators(value).toLower();
}

QString knownFolderPath(REFKNOWNFOLDERID folderId)
{
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(folderId, 0, nullptr, &raw)) || !raw) {
        return {};
    }

    const QString path = QString::fromWCharArray(raw);
    CoTaskMemFree(raw);
    return path;
}

QStringList startMenuDirectories()
{
    QStringList dirs;
    const QString userPrograms = knownFolderPath(FOLDERID_Programs);
    const QString commonPrograms = knownFolderPath(FOLDERID_CommonPrograms);
    if (!userPrograms.isEmpty()) {
        dirs << userPrograms;
    }
    if (!commonPrograms.isEmpty()) {
        dirs << commonPrograms;
    }
    return dirs;
}

QStringList desktopDirectories()
{
    QStringList dirs;
    const QString userDesktop = knownFolderPath(FOLDERID_Desktop);
    const QString publicDesktop = knownFolderPath(FOLDERID_PublicDesktop);
    if (!userDesktop.isEmpty()) {
        dirs << userDesktop;
    }
    if (!publicDesktop.isEmpty()) {
        dirs << publicDesktop;
    }
    return dirs;
}

QStringList userSearchRoots()
{
    QStringList roots;
    const KNOWNFOLDERID folders[] = {
        FOLDERID_Desktop,
        FOLDERID_Downloads,
        FOLDERID_Documents,
        FOLDERID_Pictures,
        FOLDERID_Music,
        FOLDERID_Videos,
    };

    for (const KNOWNFOLDERID& folder : folders) {
        const QString path = knownFolderPath(folder);
        if (!path.isEmpty() && QDir(path).exists() && !roots.contains(path, Qt::CaseInsensitive)) {
            roots << path;
        }
    }
    return roots;
}

QString cleanTitleFromShortcut(const QString& shortcutPath)
{
    QString title = QFileInfo(shortcutPath).completeBaseName();
    title.remove(QRegularExpression(QStringLiteral("\\s+-\\s+Shortcut$"), QRegularExpression::CaseInsensitiveOption));
    return title.trimmed();
}

ShortcutDetails resolveShortcut(const QString& shortcutPath)
{
    ShortcutDetails details;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool mustUninit = SUCCEEDED(hr);
    const bool comReady = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comReady) {
        return details;
    }

    IShellLinkW* shellLink = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr) || !shellLink) {
        if (mustUninit) {
            CoUninitialize();
        }
        return details;
    }

    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (FAILED(hr) || !persistFile) {
        shellLink->Release();
        if (mustUninit) {
            CoUninitialize();
        }
        return details;
    }

    hr = persistFile->Load(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), STGM_READ);
    if (SUCCEEDED(hr)) {
        wchar_t target[MAX_PATH * 4] = {};
        WIN32_FIND_DATAW findData = {};
        hr = shellLink->GetPath(target, static_cast<int>(std::size(target)), &findData, SLGP_RAWPATH);
        if (SUCCEEDED(hr) && target[0] != L'\0') {
            details.targetPath = QString::fromWCharArray(target);
        }

        wchar_t arguments[MAX_PATH * 4] = {};
        hr = shellLink->GetArguments(arguments, static_cast<int>(std::size(arguments)));
        if (SUCCEEDED(hr) && arguments[0] != L'\0') {
            details.arguments = QString::fromWCharArray(arguments);
        }

        wchar_t workdir[MAX_PATH * 4] = {};
        hr = shellLink->GetWorkingDirectory(workdir, static_cast<int>(std::size(workdir)));
        if (SUCCEEDED(hr) && workdir[0] != L'\0') {
            details.workingDirectory = QString::fromWCharArray(workdir);
        }
    }

    persistFile->Release();
    shellLink->Release();
    if (mustUninit) {
        CoUninitialize();
    }
    return details;
}

QPixmap systemIconForPath(const QString& path)
{
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    const QString nativePath = QDir::toNativeSeparators(path);
    DWORD attributes = info.isDir() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
    if (!info.exists()) {
        flags |= SHGFI_USEFILEATTRIBUTES;
    }

    SHFILEINFOW sfi = {};
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), attributes, &sfi, sizeof(sfi), flags) || !sfi.hIcon) {
        return {};
    }

    return pixmapFromHicon(sfi.hIcon);
}

QString kindForFile(const QFileInfo& info)
{
    if (info.isDir()) {
        return QStringLiteral("folder");
    }
    if (info.suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("app");
    }
    return QStringLiteral("file");
}

bool shouldSkipFile(const QFileInfo& info)
{
    const QString name = info.fileName();
    if (name.startsWith(QLatin1Char('.'))) {
        return true;
    }
    if (name.compare(QStringLiteral("desktop.ini"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (info.isSymLink()) {
        return true;
    }
    return false;
}

QString windowsSearchUri(const QString& query)
{
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(query));
    return QStringLiteral("search-ms:query=%1").arg(encoded);
}

} // namespace

SpotlightModel::SpotlightModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_iconCacheDir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("spotlightcache_v1")))
{
    QDir().mkpath(m_iconCacheDir);
    refreshIndex();
}

int SpotlightModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_results.size();
}

QVariant SpotlightModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }

    const Entry& item = m_results.at(index.row());
    switch (role) {
    case TitleRole: return item.title;
    case SubtitleRole: return item.subtitle;
    case IconUrlRole: return item.iconUrl;
    case KindRole: return item.kind;
    case ScoreRole: return item.score;
    default: return {};
    }
}

QHash<int, QByteArray> SpotlightModel::roleNames() const
{
    return {
        { TitleRole, "title" },
        { SubtitleRole, "subtitle" },
        { IconUrlRole, "iconUrl" },
        { KindRole, "kind" },
        { ScoreRole, "score" },
    };
}

QString SpotlightModel::query() const
{
    return m_query;
}

void SpotlightModel::setQuery(const QString& query)
{
    const QString normalized = query.simplified();
    if (m_query == normalized) {
        return;
    }
    m_query = normalized;
    emit queryChanged();
    rebuildResults();
}

void SpotlightModel::refreshIndex()
{
    emit logMessage(QStringLiteral("Spotlight index refresh started."));

    QVector<Entry> next;
    QHash<QString, bool> seenKeys;

    const auto appendShortcut = [this, &next, &seenKeys](const QString& shortcutPath, int baseScore) {
        const QFileInfo shortcutInfo(shortcutPath);
        if (!shortcutInfo.exists() || !shortcutInfo.isFile()) {
            return;
        }

        const ShortcutDetails details = resolveShortcut(shortcutPath);
        Entry entry;
        entry.title = cleanTitleFromShortcut(shortcutPath);
        entry.subtitle = details.targetPath.isEmpty()
            ? QDir::toNativeSeparators(shortcutInfo.absolutePath())
            : QDir::toNativeSeparators(details.targetPath);
        entry.iconUrl = iconUrlForPath(shortcutPath, QStringLiteral("shortcut:") + normalizedKey(shortcutPath));
        entry.kind = QStringLiteral("app");
        entry.launchPath = shortcutPath;
        entry.searchText = QStringLiteral("%1 %2 %3").arg(entry.title, entry.subtitle, shortcutPath);
        entry.baseScore = baseScore;
        appendEntry(&next, &seenKeys, entry);
    };

    for (const QString& dirPath : startMenuDirectories()) {
        QDirIterator it(dirPath, QStringList() << QStringLiteral("*.lnk"), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            appendShortcut(it.next(), 220);
        }
    }

    for (const QString& dirPath : desktopDirectories()) {
        QDirIterator it(dirPath, QStringList() << QStringLiteral("*.lnk"), QDir::Files, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            appendShortcut(it.next(), 260);
        }
    }

    for (const QString& rootPath : userSearchRoots()) {
        const QFileInfo rootInfo(rootPath);
        if (!rootInfo.exists()) {
            continue;
        }

        Entry rootEntry;
        rootEntry.title = rootInfo.fileName();
        rootEntry.subtitle = QDir::toNativeSeparators(rootInfo.absoluteFilePath());
        rootEntry.iconUrl = iconUrlForPath(rootInfo.absoluteFilePath(), QStringLiteral("folder-root:") + normalizedKey(rootInfo.absoluteFilePath()));
        rootEntry.kind = QStringLiteral("folder");
        rootEntry.launchPath = rootInfo.absoluteFilePath();
        rootEntry.searchText = QStringLiteral("%1 %2").arg(rootEntry.title, rootEntry.subtitle);
        rootEntry.baseScore = 90;
        appendEntry(&next, &seenKeys, rootEntry);

        const QDir rootDir(rootPath);
        const QFileInfoList entries = rootDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
                                                            QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& info : entries) {
            if (shouldSkipFile(info)) {
                continue;
            }

            Entry entry;
            entry.title = info.fileName();
            entry.subtitle = QDir::toNativeSeparators(info.absolutePath());
            entry.iconUrl = iconUrlForPath(info.absoluteFilePath(), QStringLiteral("path:") + normalizedKey(info.absoluteFilePath()));
            entry.kind = kindForFile(info);
            entry.launchPath = info.absoluteFilePath();
            entry.searchText = QStringLiteral("%1 %2").arg(entry.title, entry.subtitle);
            entry.baseScore = info.isDir() ? 40 : 20;
            appendEntry(&next, &seenKeys, entry);
        }
    }

    m_index = next;
    const qsizetype appCount = std::count_if(m_index.cbegin(), m_index.cend(), [](const Entry& entry) {
        return entry.kind == QLatin1String("app");
    });
    emit logMessage(QStringLiteral("Spotlight index refreshed: %1 items, %2 apps").arg(m_index.size()).arg(appCount));
    rebuildResults();
}

void SpotlightModel::activate(int index) const
{
    if (index < 0 || index >= m_results.size()) {
        return;
    }

    const Entry& item = m_results.at(index);
    QString launchPath = item.launchPath;
    if (item.kind == QLatin1String("windows-search")) {
        launchPath = windowsSearchUri(item.launchPath);
    }
    if (launchPath.isEmpty()) {
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(launchPath);
    const QString nativeWorkingDirectory = item.workingDirectory.isEmpty()
        ? QString()
        : QDir::toNativeSeparators(item.workingDirectory);

    const HINSTANCE result = ShellExecuteW(nullptr,
                                           L"open",
                                           reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                           item.arguments.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(item.arguments.utf16()),
                                           nativeWorkingDirectory.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(nativeWorkingDirectory.utf16()),
                                           SW_SHOWNORMAL);
    const quintptr resultCode = reinterpret_cast<quintptr>(result);
    if (resultCode <= 32) {
        emit logMessage(QStringLiteral("Spotlight launch failed: %1 code=%2").arg(launchPath).arg(resultCode));
        return;
    }
    emit logMessage(QStringLiteral("Spotlight launched: %1").arg(launchPath));
}

void SpotlightModel::rebuildResults()
{
    QVector<Entry> next;
    if (m_query.isEmpty()) {
        next.reserve(kMaxResults);
        for (Entry item : m_index) {
            if (item.kind != QLatin1String("app")) {
                continue;
            }
            item.score = item.baseScore;
            next.push_back(item);
        }

        std::sort(next.begin(), next.end(), [](const Entry& a, const Entry& b) {
            if (a.baseScore != b.baseScore) {
                return a.baseScore > b.baseScore;
            }
            return a.title.localeAwareCompare(b.title) < 0;
        });

        if (next.size() > kMaxResults) {
            next.resize(kMaxResults);
        }

        if (next.isEmpty()) {
            emit logMessage(QStringLiteral("Spotlight default app results: 0 (no indexed apps)"));
        } else {
            emit logMessage(QStringLiteral("Spotlight default app results: %1").arg(next.size()));
        }
    } else {
        next.reserve(kMaxResults + 1);
        for (Entry item : m_index) {
            item.score = scoreEntryForQuery(item, m_query);
            if (item.score >= 0) {
                next.push_back(item);
            }
        }

        std::sort(next.begin(), next.end(), [](const Entry& a, const Entry& b) {
            if (a.score != b.score) {
                return a.score > b.score;
            }
            return a.title.localeAwareCompare(b.title) < 0;
        });

        if (next.size() > kMaxResults) {
            next.resize(kMaxResults);
        }

        Entry fallback;
        fallback.title = QStringLiteral("Search Windows for \"%1\"").arg(m_query);
        fallback.subtitle = QStringLiteral("Open native Windows Search");
        fallback.kind = QStringLiteral("windows-search");
        fallback.launchPath = m_query;
        fallback.searchText = fallback.title;
        fallback.score = 1;
        next.push_back(fallback);
    }

    beginResetModel();
    m_results = next;
    endResetModel();
}

int SpotlightModel::scoreEntryForQuery(const Entry& entry, const QString& query) const
{
    const QString normalizedQuery = query.trimmed().toLower();
    if (normalizedQuery.isEmpty()) {
        return -1;
    }

    const QString title = entry.title.toLower();
    const QString search = entry.searchText.toLower();
    const QStringList tokens = normalizedQuery.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        if (!search.contains(token)) {
            return -1;
        }
    }

    int score = entry.baseScore;
    if (title == normalizedQuery) {
        score += 2200;
    } else if (title.startsWith(normalizedQuery)) {
        score += 1700;
    } else if (title.contains(normalizedQuery)) {
        score += 1200;
    } else {
        score += 650;
    }

    if (entry.kind == QLatin1String("app")) {
        score += 180;
    } else if (entry.kind == QLatin1String("folder")) {
        score += 60;
    }
    return score;
}

void SpotlightModel::appendEntry(QVector<Entry>* entries, QHash<QString, bool>* seenKeys, const Entry& entry) const
{
    if (!entries || !seenKeys || entry.title.isEmpty() || entry.launchPath.isEmpty()) {
        return;
    }

    const QString key = normalizedKey(entry.launchPath);
    if (seenKeys->contains(key)) {
        return;
    }

    seenKeys->insert(key, true);
    entries->push_back(entry);
}

QString SpotlightModel::iconUrlForPath(const QString& path, const QString& stableKey) const
{
    if (path.isEmpty() || stableKey.isEmpty()) {
        return {};
    }

    const QPixmap pixmap = systemIconForPath(path);
    if (pixmap.isNull()) {
        return {};
    }
    return ensurePixmapInCache(m_iconCacheDir, stableKey, pixmap);
}
