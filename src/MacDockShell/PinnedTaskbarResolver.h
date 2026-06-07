#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

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

signals:
    void logMessage(const QString& message) const;

private:
    QString pinnedTaskbarDir() const;
    PinnedShortcutEntry resolveShortcut(const QString& shortcutPath) const;
};

