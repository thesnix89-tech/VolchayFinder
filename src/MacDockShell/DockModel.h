#pragma once

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QVector>
#include <QPixmap>
#include <QHash>

struct PinnedShortcutEntry;
class PinnedTaskbarResolver;

struct DockItemEntry
{
    QString appId;
    QString label;
    QString windowTitle;
    QString exePath;
    QString launchPath;
    QString launchArguments;
    QString workingDirectory;
    QString appUserModelId;
    QString iconHint;
    QString iconUrl;
    qint64 hwndValue = 0;
    bool running = false;
    bool active = false;
    bool pinned = false;
    bool minimized = false;
    bool pinnedOnly = false;
};

class DockModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        AppIdRole = Qt::UserRole + 1,
        LabelRole,
        WindowTitleRole,
        ExePathRole,
        IconHintRole,
        IconUrlRole,
        RunningRole,
        ActiveRole,
        PinnedRole,
        MinimizedRole,
        ClickableRole
    };

    explicit DockModel(QObject* parent = nullptr);
    ~DockModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void activateIndex(int index);
    Q_INVOKABLE void minimizeIndex(int index);
    Q_INVOKABLE void toggleIndex(int index);
    Q_INVOKABLE void launchIndex(int index);

signals:
    void logMessage(const QString& message);
    void activeAppWindowChanged(const QString& title, const QString& appLabel, bool active);

private:
    void loadPinnedApps();
    void scanWindows();
    DockItemEntry makePinnedEntry(const PinnedShortcutEntry& shortcut) const;
    QString normalizeAppId(const QString& path) const;
    QString labelFromPath(const QString& path) const;
    void upsertEntry(const DockItemEntry& entry);
    QPixmap extractFileIcon(const QString& exePath) const;
    QString ensureIconFile(const QString& appId, const QString& exePath) const;
    void emitActiveWindowState();

    QVector<DockItemEntry> m_entries;
    QStringList m_pinnedPaths;
    QString m_iconCacheDir;
    QHash<QString, QString> m_existingIconUrls;
    PinnedTaskbarResolver* m_pinnedResolver = nullptr;
    QString m_lastActiveWindowTitle;
    QString m_lastActiveAppLabel;
    bool m_lastActiveState = false;
    QStringList m_lastPinnedShortcutSnapshot;
};
