#pragma once

#include <QQuickImageProvider>
#include <QHash>
#include <QPixmap>
#include <QString>
#include <QMutex>

class AppIconProvider : public QQuickImageProvider
{
public:
    AppIconProvider();

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;

    void setIconForApp(const QString& appId, const QPixmap& pixmap);
    bool hasIcon(const QString& appId) const;

private:
    mutable QMutex m_mutex;
    QHash<QString, QPixmap> m_icons;
};
