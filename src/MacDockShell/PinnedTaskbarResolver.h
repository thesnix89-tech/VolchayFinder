#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>

struct PinnedShortcutEntry
{
    QString shortcutPath;
    QString targetPath;
    QString arguments;
    QString workingDirectory;
    QString appUserModelId;
    QString iconLocation;
    int iconIndex = 0;
};

class PinnedTaskbarResolver : public QObject
{
    Q_OBJECT
public:
    explicit PinnedTaskbarResolver(QObject* parent = nullptr);

    QStringList resolvePinnedExecutablePaths() const;
    QList<PinnedShortcutEntry> resolvePinnedShortcuts() const;
    QList<PinnedShortcutEntry> resolvePinnedShortcutsForDockSync() const;
    QList<PinnedShortcutEntry> resolveDockPinnedShortcuts(const QStringList& explicitDockPinKeys) const;
    QList<PinnedShortcutEntry> resolveAllFolderShortcuts() const;
    QString createPinFromPath(const QString& sourcePath) const;

signals:
    void logMessage(const QString& message) const;

private:
    QString pinnedTaskbarDir() const;
    QList<PinnedShortcutEntry> collectFolderShortcuts() const;
    QList<PinnedShortcutEntry> filterPinnedShortcuts(const QList<PinnedShortcutEntry>& folderShortcuts) const;
    PinnedShortcutEntry resolveShortcut(const QString& shortcutPath) const;
    // Resolving a .lnk goes through COM (CoCreateInstance + IPersistFile::Load),
    // which is expensive and runs on the GUI thread every refresh. The pinned set
    // almost never changes, so cache each resolution keyed by the file's last-write
    // time and only re-run COM when the shortcut actually changed on disk.
    PinnedShortcutEntry resolveShortcutCached(const QString& shortcutPath) const;

    struct ResolvedShortcutCacheEntry {
        qint64 mtimeMs = 0;
        PinnedShortcutEntry entry;
    };
    mutable QHash<QString, ResolvedShortcutCacheEntry> m_shortcutCache;
    QString createShortcutForExecutable(const QString& exePath, const QString& destDir) const;
    QString copyShortcutToTaskbar(const QString& shortcutPath, const QString& destDir) const;
    QString uniqueShortcutPath(const QString& destDir, const QString& baseName) const;
};

