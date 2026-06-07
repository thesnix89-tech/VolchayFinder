#include "AppIconProvider.h"

#include <QMutexLocker>
#include <QPainter>

AppIconProvider::AppIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap AppIconProvider::requestPixmap(const QString& id, QSize* size, const QSize& requestedSize)
{
    QMutexLocker locker(&m_mutex);
    QPixmap pixmap = m_icons.value(id);
    if (pixmap.isNull()) {
        pixmap = QPixmap(64, 64);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor("#D7DDE7"));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(pixmap.rect().adjusted(4, 4, -4, -4), 10, 10);
        painter.setPen(QColor("#1C222B"));
        QFont font = painter.font();
        font.setPixelSize(28);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, id.left(1).toUpper());
    }

    if (requestedSize.isValid()) {
        pixmap = pixmap.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}

void AppIconProvider::setIconForApp(const QString& appId, const QPixmap& pixmap)
{
    QMutexLocker locker(&m_mutex);
    m_icons.insert(appId, pixmap);
}

bool AppIconProvider::hasIcon(const QString& appId) const
{
    QMutexLocker locker(&m_mutex);
    return m_icons.contains(appId) && !m_icons.value(appId).isNull();
}
