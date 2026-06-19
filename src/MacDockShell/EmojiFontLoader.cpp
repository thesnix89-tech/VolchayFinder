#include "EmojiFontLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStandardPaths>

namespace {

constexpr auto kEmojiFileName = "AppleColorEmoji-Windows.ttf";
constexpr auto kDefaultEmojiFamily = "Segoe UI Emoji";

QString gLoadedEmojiFamily = QString::fromLatin1(kDefaultEmojiFamily);

QStringList candidateEmojiFontPaths()
{
    QStringList paths;

    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataDir.isEmpty()) {
        paths.push_back(QDir(appDataDir).filePath(QStringLiteral("fonts/emoji/") + QLatin1String(kEmojiFileName)));
    }

    const QString exeDir = QCoreApplication::applicationDirPath();
    paths.push_back(QDir(exeDir).filePath(QStringLiteral("fonts/emoji/") + QLatin1String(kEmojiFileName)));

    const QDir buildDir(exeDir);
    const QString fromBuild = buildDir.filePath(QStringLiteral("../src/MacDockShell/fonts/emoji/") + QLatin1String(kEmojiFileName));
    paths.push_back(QFileInfo(fromBuild).absoluteFilePath());

    const QDir cwd(QDir::currentPath());
    paths.push_back(cwd.filePath(QStringLiteral("src/MacDockShell/fonts/emoji/") + QLatin1String(kEmojiFileName)));

    QStringList unique;
    for (const QString& path : paths) {
        const QString normalized = QFileInfo(path).absoluteFilePath();
        if (!unique.contains(normalized)) {
            unique.push_back(normalized);
        }
    }
    return unique;
}

QString resolveEmojiFontPath()
{
    for (const QString& path : candidateEmojiFontPaths()) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

void wireEmojiFallback(const QString& family)
{
    QFont appFont = QGuiApplication::font();
    QStringList families = appFont.families();
    families.removeAll(family);
    families.append(family);
    appFont.setFamilies(families);
    appFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        appFont.styleStrategy() | QFont::ContextFontMerging));
    QGuiApplication::setFont(appFont);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QFontDatabase::addApplicationFallbackFontFamily(QChar::Script_Common, family);
#endif
}

} // namespace

namespace EmojiFontLoader {

QString emojiFontFamilyName()
{
    return gLoadedEmojiFamily;
}

bool loadAppleEmojiFont(QString* loadedPathOut, QString* familyOut)
{
    const QString path = resolveEmojiFontPath();
    if (path.isEmpty()) {
        return false;
    }

    const int fontId = QFontDatabase::addApplicationFont(path);
    if (fontId < 0) {
        return false;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        return false;
    }

    const QString family = families.first();
    gLoadedEmojiFamily = family;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    QFontDatabase::addApplicationEmojiFontFamily(family);
#else
    wireEmojiFallback(family);
#endif

    if (loadedPathOut) {
        *loadedPathOut = path;
    }
    if (familyOut) {
        *familyOut = family;
    }
    return true;
}

} // namespace EmojiFontLoader
