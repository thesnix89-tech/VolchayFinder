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
    Q_INVOKABLE void closeIndex(int index);
    Q_INVOKABLE void revealIndex(int index);
    Q_INVOKABLE void moveItem(int from, int to);
    Q_INVOKABLE void setReorderActive(bool active);

signals:
    void logMessage(const QString& message);
    void activeAppWindowChanged(const QString& title, const QString& appLabel, bool active);

private:
    void loadPinnedApps();
    void scanWindows();
    void applyCustomOrder();
    void ensureExplorerPin();
    QString entryOrderKey(const DockItemEntry& entry) const;
    void loadOrder();
    void saveOrder();
    DockItemEntry makePinnedEntry(const PinnedShortcutEntry& shortcut) const;
    QString normalizeAppId(const QString& path) const;
    QString labelFromPath(const QString& path) const;
    void upsertEntry(const DockItemEntry& entry);
    void scheduleRunningStateRefresh();
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
    // Set while launching/activating/minimizing. Those Win32 calls pump a nested
    // message loop; this guard stops the refresh timer from resetting the model
    // (and destroying delegates) reentrantly during that loop.
    bool m_actionInProgress = false;
    // User-defined dock order (stable keys). Keeps icons from reshuffling on app
    // switches and persists drag-and-drop customization across sessions.
    QStringList m_customOrder;
    // Set while the user is dragging an icon; suspends refresh so the drag is not
    // interrupted by the periodic model reset.
    bool m_reorderActive = false;
};
