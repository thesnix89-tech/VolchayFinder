#pragma once

#include <QImage>
#include <QPixmap>
#include <QString>

#include <windows.h>

QImage hiconToImage(HICON hIcon);
QPixmap pixmapFromHicon(HICON hIcon, bool destroyIcon = true);
bool isUsableExecutableIconPath(const QString& path);
QPixmap extractFileIcon(const QString& exePath);
QString ensureIconFileInCache(const QString& cacheDir, const QString& stableId, const QString& exePath);
QString ensurePixmapInCache(const QString& cacheDir, const QString& stableId, const QPixmap& pixmap);
