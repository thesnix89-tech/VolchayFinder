#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <atomic>

#include <functional>

#include "TrayIconEnumerator.h"

class TrayIconModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY visibleCountChanged)
    Q_PROPERTY(int overflowCount READ overflowCount NOTIFY overflowCountChanged)
    Q_PROPERTY(int maxVisibleIcons READ maxVisibleIcons WRITE setMaxVisibleIcons NOTIFY maxVisibleIconsChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    enum Roles {
        StableIdRole = Qt::UserRole + 1,
        TooltipRole,
        IconUrlRole,
        IconHintRole,
        VisibleRole,
        InOverflowRole,
        OverflowIndexRole,
        IsSystemPromotedRole
    };

    explicit TrayIconModel(QObject* parent = nullptr);
    ~TrayIconModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void showMenu(int index);

    int visibleCount() const;
    int overflowCount() const;
    int maxVisibleIcons() const;
    void setMaxVisibleIcons(int count);
    bool enabled() const;
    void setEnabled(bool enabled);
    void setRefreshIntervalMs(int intervalMs);
    void setTrayOnScreenScope(TrayIconEnumerator::TrayOnScreenScope scope);
    void setTrayUiaBusyScope(std::function<void(bool)> scope);
    void setGuiInvoker(TrayIconEnumerator::GuiInvoker invoker);
    void resetOverflowBootstrap();

signals:
    void logMessage(const QString& message);
    void visibleCountChanged();
    void overflowCountChanged();
    void maxVisibleIconsChanged();
    void enabledChanged();

private:
    struct TrayEntry
    {
        TrayIconInfo info;
        QString iconUrl;
        QString iconHint;
        bool shownInBar = true;
        int overflowIndex = -1;
    };

    void scheduleRefresh();
    void applyEntries(const QVector<TrayIconInfo>& icons);
    QVector<TrayEntry> buildEntries(const QVector<TrayIconInfo>& icons, QStringList* iconSourceLog = nullptr) const;
    QString iconUrlForInfo(const TrayIconInfo& info, QString* sourceOut = nullptr) const;
    QString iconHintForInfo(const TrayIconInfo& info) const;
    bool entryAtModelIndex(int index, TrayEntry* entry, int* storageIndex = nullptr) const;
    QString manifestFilePath() const;
    void saveLastGoodIconsManifest(const QVector<TrayIconInfo>& icons) const;

    TrayIconEnumerator m_enumerator;
    QVector<TrayEntry> m_entries;
    mutable QHash<QString, QString> m_iconUrlByStableId;
    QString m_iconCacheDir;
    QTimer m_refreshTimer;
    std::atomic_bool m_refreshInFlight { false };
    mutable QMutex m_trayOpMutex;
    TrayIconEnumerator::TrayOnScreenScope m_trayOnScreenScope;
    TrayIconEnumerator::GuiInvoker m_guiInvoker;
    std::function<void(bool)> m_trayUiaBusyScope;
    int m_maxVisibleIcons = 8;
    int m_debugRefreshesRemaining = 0;
    bool m_enabled = false;
};
