#include "NotificationCenterModel.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMetaObject>
#include <QUrl>

#include <algorithm>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#include <winrt/Windows.UI.Notifications.h>

namespace {

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::UI::Notifications::Management;

QString toQString(const winrt::hstring& value)
{
    return QString::fromWCharArray(value.c_str(), static_cast<int>(value.size()));
}

QString statusToString(UserNotificationListenerAccessStatus status)
{
    switch (status) {
    case UserNotificationListenerAccessStatus::Allowed: return QStringLiteral("allowed");
    case UserNotificationListenerAccessStatus::Denied: return QStringLiteral("denied");
    case UserNotificationListenerAccessStatus::Unspecified: return QStringLiteral("unspecified");
    default: return QStringLiteral("unknown");
    }
}

QString timeAgoText(const DateTime& dateTime)
{
    const qint64 unixSeconds = winrt::clock::to_time_t(dateTime);
    const QDateTime created = QDateTime::fromSecsSinceEpoch(unixSeconds).toLocalTime();
    const qint64 seconds = created.secsTo(QDateTime::currentDateTime());
    if (seconds < 60) {
        return QObject::tr("Now");
    }
    if (seconds < 3600) {
        return QObject::tr("%1 min ago").arg(seconds / 60);
    }
    if (seconds < 86400) {
        return QObject::tr("%1 hr ago").arg(seconds / 3600);
    }
    return created.toString(QStringLiteral("dd.MM HH:mm"));
}

QByteArray readStreamBytes(const IRandomAccessStreamWithContentType& stream)
{
    if (!stream) {
        return {};
    }

    const uint64_t size = stream.Size();
    if (size == 0 || size > 4 * 1024 * 1024) {
        return {};
    }

    DataReader reader(stream);
    reader.InputStreamOptions(InputStreamOptions::Partial);
    const uint32_t requested = static_cast<uint32_t>(size);
    const uint32_t loaded = reader.LoadAsync(requested).get();
    if (loaded == 0) {
        return {};
    }

    QByteArray bytes;
    bytes.resize(static_cast<int>(loaded));
    reader.ReadBytes(winrt::array_view<uint8_t>(
        reinterpret_cast<uint8_t*>(bytes.data()),
        reinterpret_cast<uint8_t*>(bytes.data()) + bytes.size()));
    return bytes;
}

QByteArray appLogoBytes(const UserNotification& notification)
{
    try {
        const auto displayInfo = notification.AppInfo().DisplayInfo();
        const Size logoSize { 32.0f, 32.0f };
        const auto logo = displayInfo.GetLogo(logoSize);
        if (!logo) {
            return {};
        }
        return readStreamBytes(logo.OpenReadAsync().get());
    } catch (...) {
        return {};
    }
}

QStringList notificationTexts(const UserNotification& notification)
{
    QStringList texts;
    try {
        const NotificationBinding binding = notification.Notification()
            .Visual()
            .GetBinding(KnownNotificationBindings::ToastGeneric());
        if (!binding) {
            return texts;
        }

        for (const AdaptiveNotificationText& textElement : binding.GetTextElements()) {
            const QString text = toQString(textElement.Text()).trimmed();
            if (!text.isEmpty()) {
                texts << text;
            }
        }
    } catch (...) {
    }
    return texts;
}

NotificationCenterModel::Entry entryFromNotification(const UserNotification& notification,
                                                     const std::function<QString(const QString&, const QByteArray&)>& cacheIcon)
{
    NotificationCenterModel::Entry entry;
    entry.id = notification.Id();
    try {
        entry.appName = toQString(notification.AppInfo().DisplayInfo().DisplayName()).trimmed();
    } catch (...) {
    }
    if (entry.appName.isEmpty()) {
        entry.appName = QObject::tr("App");
    }

    const QStringList texts = notificationTexts(notification);
    entry.title = texts.value(0);
    if (entry.title.isEmpty()) {
        entry.title = entry.appName;
    }
    if (texts.size() > 1) {
        entry.body = texts.mid(1).join(QLatin1Char('\n'));
    }
    entry.timeText = timeAgoText(notification.CreationTime());

    const QByteArray logoBytes = appLogoBytes(notification);
    if (!logoBytes.isEmpty()) {
        entry.iconUrl = cacheIcon(QStringLiteral("%1:%2").arg(entry.appName).arg(entry.id), logoBytes);
    }
    return entry;
}

} // namespace

NotificationCenterModel::NotificationCenterModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_iconCacheDir(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("notificationcache_v1")))
{
    QDir().mkpath(m_iconCacheDir);
    try {
        const auto listener = UserNotificationListener::Current();
        setAccessStatus(statusToString(listener.GetAccessStatus()));
        subscribeToChanges();
    } catch (const winrt::hresult_error& error) {
        setAccessStatus(QStringLiteral("unavailable"));
        emit logMessage(QStringLiteral("Notification listener unavailable: 0x%1 %2")
                            .arg(static_cast<uint32_t>(error.code()), 8, 16, QLatin1Char('0'))
                            .arg(toQString(error.message())));
    } catch (...) {
        setAccessStatus(QStringLiteral("unavailable"));
        emit logMessage(QStringLiteral("Notification listener unavailable."));
    }
}

NotificationCenterModel::~NotificationCenterModel()
{
    clearSubscription();
}

int NotificationCenterModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant NotificationCenterModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const Entry& item = m_entries.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case AppNameRole: return item.appName;
    case TitleRole: return item.title;
    case BodyRole: return item.body;
    case TimeTextRole: return item.timeText;
    case IconUrlRole: return item.iconUrl;
    default: return {};
    }
}

QHash<int, QByteArray> NotificationCenterModel::roleNames() const
{
    return {
        { IdRole, "notificationId" },
        { AppNameRole, "appName" },
        { TitleRole, "title" },
        { BodyRole, "body" },
        { TimeTextRole, "timeText" },
        { IconUrlRole, "iconUrl" },
    };
}

QString NotificationCenterModel::accessStatus() const
{
    return m_accessStatus;
}

bool NotificationCenterModel::busy() const
{
    return m_busy;
}

int NotificationCenterModel::count() const
{
    return m_entries.size();
}

void NotificationCenterModel::requestAccess()
{
    setBusy(true);
    try {
        const auto listener = UserNotificationListener::Current();
        const auto operation = listener.RequestAccessAsync();
        operation.Completed([this](const IAsyncOperation<UserNotificationListenerAccessStatus>& op, AsyncStatus status) {
            QString nextStatus = QStringLiteral("unknown");
            if (status == AsyncStatus::Completed) {
                try {
                    nextStatus = statusToString(op.GetResults());
                } catch (...) {
                    nextStatus = QStringLiteral("unavailable");
                }
            } else {
                nextStatus = QStringLiteral("unavailable");
            }

            QMetaObject::invokeMethod(this, [this, nextStatus]() {
                setAccessStatus(nextStatus);
                setBusy(false);
                subscribeToChanges();
                refresh();
            }, Qt::QueuedConnection);
        });
    } catch (const winrt::hresult_error& error) {
        setBusy(false);
        setAccessStatus(QStringLiteral("unavailable"));
        emit logMessage(QStringLiteral("Request notification access failed: 0x%1 %2")
                            .arg(static_cast<uint32_t>(error.code()), 8, 16, QLatin1Char('0'))
                            .arg(toQString(error.message())));
    } catch (...) {
        setBusy(false);
        setAccessStatus(QStringLiteral("unavailable"));
        emit logMessage(QStringLiteral("Request notification access failed."));
    }
}

void NotificationCenterModel::refresh()
{
    setBusy(true);
    try {
        const auto listener = UserNotificationListener::Current();
        const QString currentStatus = statusToString(listener.GetAccessStatus());
        setAccessStatus(currentStatus);
        if (currentStatus != QLatin1String("allowed")) {
            replaceEntries({});
            setBusy(false);
            return;
        }

        const auto operation = listener.GetNotificationsAsync(NotificationKinds::Toast);
        operation.Completed([this](const IAsyncOperation<winrt::Windows::Foundation::Collections::IVectorView<UserNotification>>& op, AsyncStatus status) {
            QVector<Entry> entries;
            QString detail;
            if (status == AsyncStatus::Completed) {
                try {
                    const auto notifications = op.GetResults();
                    entries.reserve(static_cast<int>(notifications.Size()));
                    for (const UserNotification& notification : notifications) {
                        try {
                            entries.push_back(entryFromNotification(notification, [this](const QString& stableKey, const QByteArray& bytes) {
                                return cacheIconData(stableKey, bytes);
                            }));
                        } catch (...) {
                        }
                    }
                    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
                        return a.id > b.id;
                    });
                } catch (const winrt::hresult_error& error) {
                    detail = QStringLiteral("0x%1 %2")
                        .arg(static_cast<uint32_t>(error.code()), 8, 16, QLatin1Char('0'))
                        .arg(toQString(error.message()));
                } catch (...) {
                    detail = QStringLiteral("unknown error");
                }
            } else {
                detail = QStringLiteral("async status %1").arg(static_cast<int>(status));
            }

            QMetaObject::invokeMethod(this, [this, entries, detail]() {
                if (!detail.isEmpty()) {
                    emit logMessage(QStringLiteral("Notification refresh failed: %1").arg(detail));
                }
                replaceEntries(entries);
                setBusy(false);
                emit logMessage(QStringLiteral("Notification center refreshed: %1 items").arg(entries.size()));
            }, Qt::QueuedConnection);
        });
    } catch (const winrt::hresult_error& error) {
        replaceEntries({});
        setBusy(false);
        setAccessStatus(QStringLiteral("unavailable"));
        emit logMessage(QStringLiteral("Notification refresh failed: 0x%1 %2")
                            .arg(static_cast<uint32_t>(error.code()), 8, 16, QLatin1Char('0'))
                            .arg(toQString(error.message())));
    } catch (...) {
        replaceEntries({});
        setBusy(false);
        setAccessStatus(QStringLiteral("unavailable"));
        emit logMessage(QStringLiteral("Notification refresh failed."));
    }
}

void NotificationCenterModel::dismiss(uint id)
{
    try {
        UserNotificationListener::Current().RemoveNotification(id);
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries.at(i).id == id) {
                beginRemoveRows(QModelIndex(), i, i);
                m_entries.removeAt(i);
                endRemoveRows();
                emit countChanged();
                emit logMessage(QStringLiteral("Notification dismissed: %1").arg(id));
                return;
            }
        }
    } catch (const winrt::hresult_error& error) {
        emit logMessage(QStringLiteral("Dismiss notification failed: 0x%1 %2")
                            .arg(static_cast<uint32_t>(error.code()), 8, 16, QLatin1Char('0'))
                            .arg(toQString(error.message())));
    } catch (...) {
        emit logMessage(QStringLiteral("Dismiss notification failed."));
    }
}

void NotificationCenterModel::openWindowsNotificationSettings() const
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:notifications")));
}

void NotificationCenterModel::setAccessStatus(const QString& status)
{
    if (m_accessStatus == status) {
        return;
    }
    m_accessStatus = status;
    emit accessStatusChanged();
}

void NotificationCenterModel::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void NotificationCenterModel::replaceEntries(const QVector<Entry>& entries)
{
    const int oldCount = m_entries.size();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (oldCount != m_entries.size()) {
        emit countChanged();
    }
}

void NotificationCenterModel::subscribeToChanges()
{
    if (m_notificationChangedToken != 0 || m_accessStatus != QLatin1String("allowed")) {
        return;
    }

    try {
        const auto listener = UserNotificationListener::Current();
        const auto token = listener.NotificationChanged([this](const UserNotificationListener&, const UserNotificationChangedEventArgs&) {
            QMetaObject::invokeMethod(this, [this]() {
                refresh();
            }, Qt::QueuedConnection);
        });
        m_notificationChangedToken = token.value;
    } catch (...) {
        emit logMessage(QStringLiteral("Notification changed subscription failed."));
    }
}

void NotificationCenterModel::clearSubscription()
{
    if (m_notificationChangedToken == 0) {
        return;
    }

    try {
        const auto listener = UserNotificationListener::Current();
        listener.NotificationChanged(winrt::event_token { m_notificationChangedToken });
    } catch (...) {
    }
    m_notificationChangedToken = 0;
}

QString NotificationCenterModel::cacheIconData(const QString& stableKey, const QByteArray& bytes) const
{
    if (stableKey.isEmpty() || bytes.isEmpty()) {
        return {};
    }

    const QImage image = QImage::fromData(bytes);
    if (image.isNull()) {
        return {};
    }

    QDir().mkpath(m_iconCacheDir);
    const QByteArray hash = QCryptographicHash::hash(stableKey.toUtf8(), QCryptographicHash::Md5).toHex();
    const QString filePath = QDir(m_iconCacheDir).filePath(QString::fromLatin1(hash) + QStringLiteral(".png"));
    if (!QFileInfo::exists(filePath)) {
        image.save(filePath, "PNG");
    }
    return QUrl::fromLocalFile(filePath).toString();
}
