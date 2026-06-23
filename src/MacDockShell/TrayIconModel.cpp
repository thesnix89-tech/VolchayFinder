#include "TrayIconModel.h"

#include "IconUtils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>

#include <objbase.h>
#include <windows.h>

namespace {

struct CoInitScope
{
    CoInitScope() { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
    ~CoInitScope() { CoUninitialize(); }
};

constexpr int kTrayIconCacheVersion = 5;
constexpr int kInitialTrayRefreshDelayMs = 500;
constexpr int kBlackWhiteLuminanceThreshold = 128;

struct TrayUiaBusyScope
{
    explicit TrayUiaBusyScope(const std::function<void(bool)>& scope)
        : m_scope(scope)
    {
        if (m_scope) {
            m_scope(true);
        }
    }

    ~TrayUiaBusyScope()
    {
        if (m_scope) {
            m_scope(false);
        }
    }

    std::function<void(bool)> m_scope;
};

QPixmap blackWhitePixmap(const QPixmap& source)
{
    if (source.isNull()) {
        return {};
    }

    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            if (qAlpha(pixel) == 0) {
                line[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            const int value = qGray(pixel) >= kBlackWhiteLuminanceThreshold ? 255 : 0;
            line[x] = qRgba(value, value, value, 255);
        }
    }
    return QPixmap::fromImage(image);
}

bool canQueryIconFromHwnd(const TrayIconInfo& info)
{
    if (info.nativeWindowHandle == 0) {
        return false;
    }
    if (info.inOverflow) {
        return false;
    }
    if (info.automationId.compare(QStringLiteral("NotifyItemIcon"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(info.nativeWindowHandle);
    wchar_t className[256] = {};
    if (GetClassNameW(hwnd, className, 256) == 0) {
        return false;
    }
    const QString windowClass = QString::fromWCharArray(className);

    static const QStringList kBlockedClasses = {
        QStringLiteral("Windows.UI.Core.CoreWindow"),
        QStringLiteral("TopLevelWindowForOverflowXamlIsland"),
        QStringLiteral("NotifyIconOverflowWindow"),
        QStringLiteral("XamlExplorerHostIslandWindow"),
    };
    for (const QString& blocked : kBlockedClasses) {
        if (windowClass.contains(blocked, Qt::CaseInsensitive)) {
            return false;
        }
    }

    static const QStringList kAllowedClasses = {
        QStringLiteral("Electron_NotifyIconHostWindow"),
        QStringLiteral("Chrome_StatusTrayWindow"),
        QStringLiteral("Qt51519TrayIconMessageWindowClass"),
        QStringLiteral("Qt6TrayIconMessageWindowClass"),
    };
    for (const QString& allowed : kAllowedClasses) {
        if (windowClass.contains(allowed, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return info.automationId.compare(QStringLiteral("SystemTrayIcon"), Qt::CaseInsensitive) == 0;
}

QJsonObject trayIconInfoToJson(const TrayIconInfo& info)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("stableId"), info.stableId);
    obj.insert(QStringLiteral("tooltip"), info.tooltip);
    obj.insert(QStringLiteral("automationId"), info.automationId);
    obj.insert(QStringLiteral("className"), info.className);
    obj.insert(QStringLiteral("exePath"), info.exePath);
    obj.insert(QStringLiteral("inOverflow"), info.inOverflow);
    obj.insert(QStringLiteral("isSystemPromoted"), info.isSystemPromoted);
    obj.insert(QStringLiteral("nativeWindowHandle"),
               QString::number(info.nativeWindowHandle));
    obj.insert(QStringLiteral("processId"), static_cast<qint64>(info.processId));
    return obj;
}

TrayIconInfo trayIconInfoFromJson(const QJsonObject& obj)
{
    TrayIconInfo info;
    info.stableId = obj.value(QStringLiteral("stableId")).toString();
    info.tooltip = obj.value(QStringLiteral("tooltip")).toString();
    info.automationId = obj.value(QStringLiteral("automationId")).toString();
    info.className = obj.value(QStringLiteral("className")).toString();
    info.exePath = obj.value(QStringLiteral("exePath")).toString();
    info.inOverflow = obj.value(QStringLiteral("inOverflow")).toBool(true);
    info.isSystemPromoted = obj.value(QStringLiteral("isSystemPromoted")).toBool(false);
    info.nativeWindowHandle = obj.value(QStringLiteral("nativeWindowHandle")).toString().toULongLong();
    info.processId = static_cast<quint32>(obj.value(QStringLiteral("processId")).toInteger());
    return info;
}

} // namespace

TrayIconModel::TrayIconModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_iconCacheDir = QDir(QCoreApplication::applicationDirPath())
                         .filePath(QStringLiteral("traycache_v%1").arg(kTrayIconCacheVersion));
    QDir().mkpath(m_iconCacheDir);

    m_refreshTimer.setInterval(1500);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        refresh();
    });
}

TrayIconModel::~TrayIconModel() = default;

int TrayIconModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant TrayIconModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const TrayEntry& entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TooltipRole:
        return entry.info.tooltip;
    case StableIdRole:
        return entry.info.stableId;
    case IconUrlRole:
        return entry.iconUrl;
    case IconHintRole:
        return entry.iconHint;
    case VisibleRole:
        return true;
    case InOverflowRole:
        return false;
    case OverflowIndexRole:
        return -1;
    case IsSystemPromotedRole:
        return entry.info.isSystemPromoted;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrayIconModel::roleNames() const
{
    return {
        { StableIdRole, "stableId" },
        { TooltipRole, "tooltip" },
        { IconUrlRole, "iconUrl" },
        { IconHintRole, "iconHint" },
        { VisibleRole, "visibleInBar" },
        { InOverflowRole, "inOverflow" },
        { OverflowIndexRole, "overflowIndex" },
        { IsSystemPromotedRole, "isSystemPromoted" },
    };
}

int TrayIconModel::visibleCount() const
{
    return m_entries.size();
}

int TrayIconModel::overflowCount() const
{
    return 0;
}

int TrayIconModel::maxVisibleIcons() const
{
    return m_maxVisibleIcons;
}

void TrayIconModel::setMaxVisibleIcons(int count)
{
    const int clamped = qBound(1, count, 32);
    if (m_maxVisibleIcons == clamped) {
        return;
    }
    m_maxVisibleIcons = clamped;
    emit maxVisibleIconsChanged();
}

bool TrayIconModel::enabled() const
{
    return m_enabled;
}

void TrayIconModel::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (m_enabled) {
        m_debugRefreshesRemaining = 5;
        resetOverflowBootstrap();
        m_refreshTimer.start();
        QTimer::singleShot(kInitialTrayRefreshDelayMs, this, [this]() {
            if (m_enabled) {
                refresh();
            }
        });
    } else {
        m_refreshTimer.stop();
        m_debugRefreshesRemaining = 0;
        resetOverflowBootstrap();
        if (!m_entries.isEmpty()) {
            beginResetModel();
            m_entries.clear();
            endResetModel();
            emit visibleCountChanged();
            emit overflowCountChanged();
        }
    }
    emit enabledChanged();
}

void TrayIconModel::setRefreshIntervalMs(int intervalMs)
{
    const int clamped = qBound(500, intervalMs, 10000);
    m_refreshTimer.setInterval(clamped);
}

void TrayIconModel::setBlackWhiteIcons(bool enabled)
{
    if (m_blackWhiteIcons == enabled) {
        return;
    }

    m_blackWhiteIcons = enabled;
    m_iconUrlByStableId.clear();
    if (m_enabled) {
        refresh();
    }
}

void TrayIconModel::setTrayOnScreenScope(TrayIconEnumerator::TrayOnScreenScope scope)
{
    m_trayOnScreenScope = std::move(scope);
    m_enumerator.setTrayOnScreenScope(m_trayOnScreenScope);
}

void TrayIconModel::setTrayUiaBusyScope(std::function<void(bool)> scope)
{
    m_trayUiaBusyScope = std::move(scope);
}

void TrayIconModel::setGuiInvoker(TrayIconEnumerator::GuiInvoker invoker)
{
    m_guiInvoker = std::move(invoker);
    m_enumerator.setGuiInvoker(m_guiInvoker);
}

void TrayIconModel::resetOverflowBootstrap()
{
    // Visible-only tray mode intentionally does not bootstrap or retry through the
    // native Windows overflow panel.
}

QString TrayIconModel::iconHintForInfo(const TrayIconInfo& info) const
{
    const QString source = !info.tooltip.isEmpty() ? info.tooltip : info.automationId;
    if (source.isEmpty()) {
        return QStringLiteral("?");
    }
    return source.left(1).toUpper();
}

QString TrayIconModel::iconUrlForInfo(const TrayIconInfo& info, QString* sourceOut) const
{
    auto setSource = [sourceOut](const QString& source) {
        if (sourceOut) {
            *sourceOut = source;
        }
    };

    const QString cacheKey = m_blackWhiteIcons
        ? info.stableId + QStringLiteral("|bw")
        : info.stableId;
    const QString stableCacheId = m_blackWhiteIcons
        ? info.stableId + QStringLiteral("-bw")
        : info.stableId;

    if (m_iconUrlByStableId.contains(cacheKey)) {
        const QString cached = m_iconUrlByStableId.value(cacheKey);
        setSource(cached.isEmpty() ? QStringLiteral("hint") : QStringLiteral("cache"));
        return cached;
    }

    QString iconUrl;
    if (canQueryIconFromHwnd(info)) {
        const HWND hwnd = reinterpret_cast<HWND>(info.nativeWindowHandle);
        HICON hIcon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_SMALL2, 0));
        if (!hIcon) {
            hIcon = reinterpret_cast<HICON>(SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0));
        }
        if (hIcon) {
            QPixmap pixmap = pixmapFromHicon(hIcon);
            if (!pixmap.isNull()) {
                if (m_blackWhiteIcons) {
                    pixmap = blackWhitePixmap(pixmap);
                }
                iconUrl = ensurePixmapInCache(m_iconCacheDir, stableCacheId, pixmap);
                if (!iconUrl.isEmpty()) {
                    setSource(m_blackWhiteIcons ? QStringLiteral("hwnd-bw") : QStringLiteral("hwnd"));
                }
            }
        }
    }

    if (iconUrl.isEmpty() && isUsableExecutableIconPath(info.exePath)) {
        if (m_blackWhiteIcons) {
            const QPixmap pixmap = blackWhitePixmap(extractFileIcon(info.exePath));
            iconUrl = ensurePixmapInCache(m_iconCacheDir, stableCacheId, pixmap);
        } else {
            iconUrl = ensureIconFileInCache(m_iconCacheDir, stableCacheId, info.exePath);
        }
        if (!iconUrl.isEmpty()) {
            setSource(m_blackWhiteIcons ? QStringLiteral("exe-bw") : QStringLiteral("exe"));
        }
    }

    if (iconUrl.isEmpty()) {
        setSource(QStringLiteral("hint"));
    }

    if (!iconUrl.isEmpty()) {
        m_iconUrlByStableId.insert(cacheKey, iconUrl);
    }
    return iconUrl;
}

QVector<TrayIconModel::TrayEntry> TrayIconModel::buildEntries(const QVector<TrayIconInfo>& icons,
                                                              QStringList* iconSourceLog) const
{
    QVector<TrayEntry> next;
    next.reserve(icons.size());

    for (const TrayIconInfo& info : icons) {
        TrayEntry entry;
        entry.info = info;
        QString source;
        entry.iconUrl = iconUrlForInfo(entry.info, &source);
        entry.iconHint = iconHintForInfo(entry.info);
        entry.shownInBar = true;
        entry.overflowIndex = -1;
        next.push_back(entry);

        if (iconSourceLog) {
            const QString label = !info.tooltip.isEmpty() ? info.tooltip.section(QLatin1Char('\n'), 0, 0).trimmed()
                                                          : info.automationId;
            iconSourceLog->push_back(QStringLiteral("%1=%2").arg(label, source));
        }
    }
    return next;
}

void TrayIconModel::applyEntries(const QVector<TrayIconInfo>& icons)
{
    QStringList iconSourceLog;
    const QVector<TrayEntry> next = buildEntries(icons, icons.isEmpty() ? nullptr : &iconSourceLog);

    const bool sameSize = next.size() == m_entries.size();
    bool sameContent = sameSize;
    if (sameContent) {
        for (int i = 0; i < next.size(); ++i) {
            const TrayEntry& a = next.at(i);
            const TrayEntry& b = m_entries.at(i);
            if (a.info.stableId != b.info.stableId
                || a.info.tooltip != b.info.tooltip
                || a.iconUrl != b.iconUrl) {
                sameContent = false;
                break;
            }
        }
    }

    if (sameContent) {
        return;
    }

    if (!icons.isEmpty()) {
        saveLastGoodIconsManifest(icons);
        if (!iconSourceLog.isEmpty()) {
            emit logMessage(QStringLiteral("Tray icon sources: %1").arg(iconSourceLog.join(QStringLiteral(", "))));
        }
    } else if (!m_entries.isEmpty()) {
        emit logMessage(QStringLiteral("Tray refresh returned 0 visible icons; clearing menu bar extras."));
    }

    beginResetModel();
    m_entries = next;
    endResetModel();
    emit visibleCountChanged();
    emit overflowCountChanged();
}

void TrayIconModel::scheduleRefresh()
{
    if (!m_enabled) {
        return;
    }
    if (m_refreshInFlight.exchange(true)) {
        return;
    }

    const bool debugEnabled = m_debugRefreshesRemaining > 0;
    if (debugEnabled) {
        --m_debugRefreshesRemaining;
    }

    QThreadPool::globalInstance()->start([this, debugEnabled]() {
        QMutexLocker lock(&m_trayOpMutex);
        CoInitScope coInit;
        TrayUiaBusyScope busyScope(m_trayUiaBusyScope);

        TrayIconEnumerator enumerator;
        enumerator.setTrayOnScreenScope(m_trayOnScreenScope);
        enumerator.setGuiInvoker(m_guiInvoker);
        enumerator.setDebugEnabled(debugEnabled);
        const QVector<TrayIconInfo> icons = enumerator.enumerate(false);
        const QString logLine = enumerator.lastEnumerateLog();
        const QString debugLog = enumerator.lastDebugLog();

        QMetaObject::invokeMethod(this, [this, icons, logLine, debugLog]() {
            m_refreshInFlight = false;
            if (!logLine.isEmpty()) {
                emit logMessage(logLine);
            }
            if (!debugLog.isEmpty()) {
                for (const QString& line : debugLog.split(QLatin1Char('\n'))) {
                    emit logMessage(line);
                }
            }
            applyEntries(icons);
        }, Qt::QueuedConnection);
    });
}

void TrayIconModel::refresh()
{
    scheduleRefresh();
}

QString TrayIconModel::manifestFilePath() const
{
    return QDir(m_iconCacheDir).filePath(QStringLiteral("manifest.json"));
}

void TrayIconModel::saveLastGoodIconsManifest(const QVector<TrayIconInfo>& icons) const
{
    if (icons.isEmpty()) {
        return;
    }

    QJsonArray iconArray;
    for (const TrayIconInfo& info : icons) {
        iconArray.append(trayIconInfoToJson(info));
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kTrayIconCacheVersion);
    root.insert(QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("icons"), iconArray);

    QFile file(manifestFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool TrayIconModel::entryAtModelIndex(int index, TrayEntry* entry, int* storageIndex) const
{
    if (!entry || index < 0 || index >= m_entries.size()) {
        return false;
    }
    *entry = m_entries.at(index);
    if (storageIndex) {
        *storageIndex = index;
    }
    return true;
}

void TrayIconModel::activate(int index)
{
    TrayEntry entry;
    if (!entryAtModelIndex(index, &entry)) {
        return;
    }

    const QString label = !entry.info.tooltip.isEmpty()
        ? entry.info.tooltip.section(QLatin1Char('\n'), 0, 0).trimmed()
        : entry.info.automationId;
    const TrayIconInfo info = entry.info;

    QThreadPool::globalInstance()->start([this, info, label]() {
        QMutexLocker lock(&m_trayOpMutex);
        CoInitScope coInit;
        TrayUiaBusyScope busyScope(m_trayUiaBusyScope);
        TrayIconEnumerator enumerator;
        enumerator.setTrayOnScreenScope(m_trayOnScreenScope);
        enumerator.setGuiInvoker(m_guiInvoker);
        const bool ok = enumerator.activate(info);
        const QString detail = enumerator.lastInteractionDetail();
        QMetaObject::invokeMethod(this, [this, ok, label, detail]() {
            if (ok) {
                emit logMessage(QStringLiteral("Tray activate \"%1\": %2").arg(label, detail));
            } else {
                emit logMessage(QStringLiteral("Tray activate \"%1\": failed (%2)").arg(label, detail));
            }
        }, Qt::QueuedConnection);
    });
}

void TrayIconModel::showMenu(int index)
{
    TrayEntry entry;
    if (!entryAtModelIndex(index, &entry)) {
        return;
    }

    const QString label = !entry.info.tooltip.isEmpty()
        ? entry.info.tooltip.section(QLatin1Char('\n'), 0, 0).trimmed()
        : entry.info.automationId;
    const TrayIconInfo info = entry.info;

    QThreadPool::globalInstance()->start([this, info, label]() {
        QMutexLocker lock(&m_trayOpMutex);
        CoInitScope coInit;
        TrayUiaBusyScope busyScope(m_trayUiaBusyScope);
        TrayIconEnumerator enumerator;
        enumerator.setTrayOnScreenScope(m_trayOnScreenScope);
        enumerator.setGuiInvoker(m_guiInvoker);
        const bool ok = enumerator.showContextMenu(info);
        const QString detail = enumerator.lastInteractionDetail();
        QMetaObject::invokeMethod(this, [this, ok, label, detail]() {
            if (ok) {
                emit logMessage(QStringLiteral("Tray menu \"%1\": %2").arg(label, detail));
            } else {
                emit logMessage(QStringLiteral("Tray menu \"%1\": failed (%2)").arg(label, detail));
            }
        }, Qt::QueuedConnection);
    });
}
