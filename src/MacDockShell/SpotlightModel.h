#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

class SpotlightModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        SubtitleRole,
        IconUrlRole,
        KindRole,
        ScoreRole
    };

    explicit SpotlightModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const;
    Q_INVOKABLE void setQuery(const QString& query);
    Q_INVOKABLE void refreshIndex();
    Q_INVOKABLE void activate(int index) const;

signals:
    void queryChanged();
    void logMessage(const QString& message) const;

private:
    struct Entry
    {
        QString title;
        QString subtitle;
        QString iconUrl;
        QString kind;
        QString launchPath;
        QString arguments;
        QString workingDirectory;
        QString searchText;
        int score = 0;
        int baseScore = 0;
    };

    void rebuildResults();
    int scoreEntryForQuery(const Entry& entry, const QString& query) const;
    void appendEntry(QVector<Entry>* entries, QHash<QString, bool>* seenKeys, const Entry& entry) const;
    QString iconUrlForPath(const QString& path, const QString& stableKey) const;

    QString m_query;
    QVector<Entry> m_index;
    QVector<Entry> m_results;
    QString m_iconCacheDir;
};
