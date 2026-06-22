#include "IconUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QCryptographicHash>
#include <QUrl>

#include <windows.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shellapi.h>
#include <shlobj.h>

QImage hiconToImage(HICON hIcon)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) {
        return {};
    }

    BITMAP bmp = {};
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    QImage image(bmp.bmWidth, bmp.bmHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, image.bits(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    return image;
}

QPixmap pixmapFromHicon(HICON hIcon, bool destroyIcon)
{
    if (!hIcon) {
        return {};
    }
    const QImage image = hiconToImage(hIcon);
    if (destroyIcon) {
        DestroyIcon(hIcon);
    }
    return image.isNull() ? QPixmap() : QPixmap::fromImage(image);
}

bool isUsableExecutableIconPath(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || info.isDir()) {
        return false;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QLatin1String("exe") || suffix == QLatin1String("dll");
}

QPixmap extractFileIcon(const QString& exePath)
{
    if (!isUsableExecutableIconPath(exePath)) {
        return {};
    }

    const QString nativePath = QDir::toNativeSeparators(exePath);
    if (nativePath.isEmpty()) {
        return {};
    }

    const LPCWSTR pathW = reinterpret_cast<LPCWSTR>(nativePath.utf16());

    SHFILEINFOW sfi = {};
    if (SHGetFileInfoW(pathW, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX)) {
        const int imageListIds[] = { SHIL_JUMBO, SHIL_EXTRALARGE };
        for (const int listId : imageListIds) {
            IImageList* imageList = nullptr;
            HRESULT hr = SHGetImageList(listId, IID_IImageList, reinterpret_cast<void**>(&imageList));
            if (FAILED(hr) || !imageList) {
                continue;
            }

            HICON hIcon = nullptr;
            hr = imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            imageList->Release();
            if (FAILED(hr) || !hIcon) {
                continue;
            }

            const QPixmap pixmap = pixmapFromHicon(hIcon);
            if (!pixmap.isNull() && pixmap.width() >= 48) {
                return pixmap;
            }
        }
    }

    HICON largeIcon = nullptr;
    const UINT extracted = ExtractIconExW(pathW, 0, &largeIcon, nullptr, 1);
    if (extracted > 0 && largeIcon) {
        const QPixmap pixmap = pixmapFromHicon(largeIcon);
        if (!pixmap.isNull()) {
            return pixmap;
        }
    }

    SHFILEINFOW sfiLarge = {};
    if (SHGetFileInfoW(pathW, 0, &sfiLarge, sizeof(sfiLarge), SHGFI_ICON | SHGFI_LARGEICON)) {
        return pixmapFromHicon(sfiLarge.hIcon);
    }

    return {};
}

QString ensurePixmapInCache(const QString& cacheDir, const QString& stableId, const QPixmap& pixmap)
{
    if (pixmap.isNull() || stableId.isEmpty()) {
        return {};
    }

    QDir().mkpath(cacheDir);
    const QByteArray hash = QCryptographicHash::hash(stableId.toUtf8(), QCryptographicHash::Md5).toHex();
    const QString filePath = QDir(cacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral(".png"));
    pixmap.save(filePath, "PNG");
    return QUrl::fromLocalFile(filePath).toString();
}

QString ensureIconFileInCache(const QString& cacheDir, const QString& stableId, const QString& exePath)
{
    if (stableId.isEmpty()) {
        return {};
    }

    QDir().mkpath(cacheDir);
    const QByteArray hash = QCryptographicHash::hash(stableId.toUtf8(), QCryptographicHash::Md5).toHex();
    const QString filePath = QDir(cacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral(".png"));

    if (QFileInfo::exists(filePath)) {
        const QImage cached(filePath);
        if (!cached.isNull() && cached.width() >= 16) {
            return QUrl::fromLocalFile(filePath).toString();
        }
    }

    if (!isUsableExecutableIconPath(exePath)) {
        return {};
    }

    const QPixmap pixmap = extractFileIcon(exePath);
    if (pixmap.isNull()) {
        return {};
    }

    return ensurePixmapInCache(cacheDir, stableId, pixmap);
}
