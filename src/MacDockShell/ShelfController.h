#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// Backend for the macOS-style dock right section (Downloads stack + Trash).
// Pure Windows Shell operations; icons are provided by DockModel's icon cache.
class ShelfController : public QObject
{
    Q_OBJECT

public:
    explicit ShelfController(QObject* parent = nullptr);

    // Recent files in the user's Downloads folder, newest first.
    // Each element is a map { "name": QString, "path": QString }.
    Q_INVOKABLE QVariantList recentDownloads(int limit = 12) const;
    Q_INVOKABLE void openPath(const QString& path) const;
    Q_INVOKABLE void openDownloadsFolder() const;
    Q_INVOKABLE void openRecycleBin() const;
    Q_INVOKABLE void emptyRecycleBin();

signals:
    void logMessage(const QString& message) const;
    // Emitted after the recycle bin is emptied so the dock can refresh the trash icon.
    void recycleBinChanged();

private:
    QString downloadsFolderPath() const;
};
