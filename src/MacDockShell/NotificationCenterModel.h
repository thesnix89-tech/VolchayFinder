#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

class NotificationCenterModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString accessStatus READ accessStatus NOTIFY accessStatusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        AppNameRole,
        TitleRole,
        BodyRole,
        TimeTextRole,
        IconUrlRole
    };

    explicit NotificationCenterModel(QObject* parent = nullptr);
    ~NotificationCenterModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString accessStatus() const;
    bool busy() const;
    int count() const;

    struct Entry
    {
        uint id = 0;
        QString appName;
        QString title;
        QString body;
        QString timeText;
        QString iconUrl;
    };

    Q_INVOKABLE void requestAccess();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void dismiss(uint id);
    Q_INVOKABLE void openWindowsNotificationSettings() const;

signals:
    void accessStatusChanged();
    void busyChanged();
    void countChanged();
    void logMessage(const QString& message) const;

private:
    void setAccessStatus(const QString& status);
    void setBusy(bool busy);
    void replaceEntries(const QVector<Entry>& entries);
    void subscribeToChanges();
    void clearSubscription();
    QString cacheIconData(const QString& stableKey, const QByteArray& bytes) const;

    QVector<Entry> m_entries;
    QString m_accessStatus = QStringLiteral("unknown");
    bool m_busy = false;
    QString m_iconCacheDir;
    qint64 m_notificationChangedToken = 0;
};
