#include "ShelfController.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <objbase.h>

ShelfController::ShelfController(QObject* parent)
    : QObject(parent)
{
}

QString ShelfController::downloadsFolderPath() const
{
    PWSTR raw = nullptr;
    QString result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &raw)) && raw) {
        result = QString::fromWCharArray(raw);
    }
    if (raw) {
        CoTaskMemFree(raw);
    }
    if (result.isEmpty()) {
        result = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    }
    return result;
}

QVariantList ShelfController::recentDownloads(int limit) const
{
    QVariantList items;
    const QString path = downloadsFolderPath();
    if (path.isEmpty()) {
        return items;
    }

    QDir dir(path);
    if (!dir.exists()) {
        return items;
    }

    // Files only, newest first. Skip the browser .crdownload/.part temp files.
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot,
                                                   QDir::Time);
    for (const QFileInfo& info : entries) {
        const QString suffix = info.suffix().toLower();
        if (suffix == QLatin1String("crdownload") || suffix == QLatin1String("part")
            || suffix == QLatin1String("tmp")) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), info.fileName());
        entry.insert(QStringLiteral("path"), QDir::toNativeSeparators(info.absoluteFilePath()));
        items.append(entry);
        if (items.size() >= limit) {
            break;
        }
    }
    return items;
}

void ShelfController::openPath(const QString& path) const
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    const QString nativePath = QDir::toNativeSeparators(path);
    ShellExecuteW(nullptr, L"open",
                  reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                  nullptr, nullptr, SW_SHOWNORMAL);
    emit logMessage(QStringLiteral("openPath: %1").arg(nativePath));
}

void ShelfController::openDownloadsFolder() const
{
    openPath(downloadsFolderPath());
}

void ShelfController::openRecycleBin() const
{
    ShellExecuteW(nullptr, L"open", L"shell:RecycleBinFolder",
                  nullptr, nullptr, SW_SHOWNORMAL);
    emit logMessage(QStringLiteral("Opened Recycle Bin."));
}

void ShelfController::emptyRecycleBin()
{
    // Shows the standard Windows confirmation/progress dialog.
    const HRESULT hr = SHEmptyRecycleBinW(nullptr, nullptr, 0);
    // S_OK on success; 0x8000FFFF (E_UNEXPECTED) is returned when already empty.
    emit logMessage(QStringLiteral("emptyRecycleBin: hr=0x%1")
                        .arg(QString::number(static_cast<quint32>(hr), 16)));
    emit recycleBinChanged();
}
