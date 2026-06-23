#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <atomic>

class QTimer;

class TaskbarController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool taskbarHidden READ taskbarHidden NOTIFY taskbarHiddenChanged)
    Q_PROPERTY(bool dockAutoHidden READ dockAutoHidden NOTIFY dockAutoHiddenChanged)
    Q_PROPERTY(bool shellActive READ shellActive WRITE setShellActive NOTIFY shellActiveChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(int dockIconSize READ dockIconSize WRITE setDockIconSize NOTIFY dockIconSizeChanged)
    Q_PROPERTY(bool showTopBar READ showTopBar WRITE setShowTopBar NOTIFY showTopBarChanged)
    Q_PROPERTY(bool autoHideWindowsTaskbar READ autoHideWindowsTaskbar WRITE setAutoHideWindowsTaskbar NOTIFY autoHideWindowsTaskbarChanged)
    Q_PROPERTY(bool keepTaskbarAutoHideOnExit READ keepTaskbarAutoHideOnExit WRITE setKeepTaskbarAutoHideOnExit NOTIFY keepTaskbarAutoHideOnExitChanged)
    Q_PROPERTY(bool dockHoverBounce READ dockHoverBounce WRITE setDockHoverBounce NOTIFY dockHoverBounceChanged)
    Q_PROPERTY(bool dockDragFadeEnabled READ dockDragFadeEnabled WRITE setDockDragFadeEnabled NOTIFY dockDragFadeEnabledChanged)
    Q_PROPERTY(bool dockStaticIcons READ dockStaticIcons WRITE setDockStaticIcons NOTIFY dockStaticIconsChanged)
    Q_PROPERTY(bool dockSeparateTransientApps READ dockSeparateTransientApps WRITE setDockSeparateTransientApps NOTIFY dockSeparateTransientAppsChanged)
    Q_PROPERTY(QString appearanceMode READ appearanceMode WRITE setAppearanceMode NOTIFY appearanceModeChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(QString dockLightStyle READ dockLightStyle WRITE setDockLightStyle NOTIFY dockLightStyleChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY startWithWindowsChanged)
    Q_PROPERTY(QString menuBarAppName READ menuBarAppName NOTIFY menuBarAppNameChanged)
    Q_PROPERTY(QStringList menuBarItems READ menuBarItems NOTIFY menuBarItemsChanged)
    Q_PROPERTY(QString explorerIconStyle READ explorerIconStyle WRITE setExplorerIconStyle NOTIFY explorerIconStyleChanged)
    Q_PROPERTY(QString trashIconStyle READ trashIconStyle WRITE setTrashIconStyle NOTIFY trashIconStyleChanged)
    Q_PROPERTY(QString menuBarIconStyle READ menuBarIconStyle WRITE setMenuBarIconStyle NOTIFY menuBarIconStyleChanged)
    Q_PROPERTY(QString menuBarCustomIconPath READ menuBarCustomIconPath NOTIFY menuBarCustomIconPathChanged)
    Q_PROPERTY(bool showDownloadsInDock READ showDownloadsInDock WRITE setShowDownloadsInDock NOTIFY showDownloadsInDockChanged)
    Q_PROPERTY(bool showMenuBarExtras READ showMenuBarExtras WRITE setShowMenuBarExtras NOTIFY showMenuBarExtrasChanged)
    Q_PROPERTY(bool blackWhiteTrayIcons READ blackWhiteTrayIcons WRITE setBlackWhiteTrayIcons NOTIFY blackWhiteTrayIconsChanged)
    Q_PROPERTY(bool transparentNotificationCenter READ transparentNotificationCenter WRITE setTransparentNotificationCenter NOTIFY transparentNotificationCenterChanged)
    Q_PROPERTY(int trayExtrasRefreshMs READ trayExtrasRefreshMs WRITE setTrayExtrasRefreshMs NOTIFY trayExtrasRefreshMsChanged)
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool showDesktopActive READ showDesktopActive NOTIFY showDesktopActiveChanged)

public:
    explicit TaskbarController(QObject* parent = nullptr);
    ~TaskbarController() override;

    Q_INVOKABLE bool hideTaskbar();
    Q_INVOKABLE bool showTaskbar();
    Q_INVOKABLE void restoreShell();
    Q_INVOKABLE void quitApplication();
    Q_INVOKABLE void apply(bool autoHideWindowsTaskbar, bool keepTaskbarAutoHideOnExit, bool showTopBar, int iconSize, bool dockHoverBounce, bool dockDragFadeEnabled, bool dockStaticIcons, bool dockSeparateTransientApps, bool darkTheme, bool startWithWindows, const QString& explorerIconStyle, const QString& trashIconStyle, const QString& menuBarIconStyle, bool showDownloadsInDock, const QString& dockLightStyle, bool showMenuBarExtras, bool blackWhiteTrayIcons, bool transparentNotificationCenter);
    Q_INVOKABLE QString menuBarIconUrl(bool darkTheme) const;
    Q_INVOKABLE QString menuBarIconPreviewUrl(const QString& style, bool darkTheme = false) const;
    Q_INVOKABLE bool importCustomMenuBarIcon();
    Q_INVOKABLE void tryAutostartShell();
    Q_INVOKABLE void enforceTaskbarHidden();
    Q_INVOKABLE QStringList availableLanguages() const;
    Q_INVOKABLE QString languageDisplayName(const QString& code) const;
    Q_INVOKABLE void refreshMenuBar();
    Q_INVOKABLE void setAppearanceMode(const QString& mode);
    Q_INVOKABLE void setUiLanguage(const QString& language);
    Q_INVOKABLE bool toggleShowDesktop();
    Q_INVOKABLE QString controlCenterLayout() const;
    Q_INVOKABLE void setControlCenterLayout(const QString& layoutJson);

    bool taskbarHidden() const;
    bool dockAutoHidden() const;
    bool shellActive() const;
    void setShellActive(bool active);
    bool settingsVisible() const;
    void setSettingsVisible(bool visible);
    int dockIconSize() const;
    void setDockIconSize(int size);
    bool showTopBar() const;
    void setShowTopBar(bool show);
    bool autoHideWindowsTaskbar() const;
    void setAutoHideWindowsTaskbar(bool autoHide);
    bool keepTaskbarAutoHideOnExit() const;
    void setKeepTaskbarAutoHideOnExit(bool keep);
    bool dockHoverBounce() const;
    void setDockHoverBounce(bool enabled);
    bool dockDragFadeEnabled() const;
    void setDockDragFadeEnabled(bool enabled);
    bool dockStaticIcons() const;
    void setDockStaticIcons(bool enabled);
    bool dockSeparateTransientApps() const;
    void setDockSeparateTransientApps(bool enabled);
    QString appearanceMode() const;
    bool darkTheme() const;
    void setDarkTheme(bool enabled);
    QString dockLightStyle() const;
    void setDockLightStyle(const QString& style);
    bool startWithWindows() const;
    void setStartWithWindows(bool enabled);
    QString menuBarAppName() const;
    QStringList menuBarItems() const;
    QString explorerIconStyle() const;
    void setExplorerIconStyle(const QString& style);
    QString trashIconStyle() const;
    void setTrashIconStyle(const QString& style);
    QString menuBarIconStyle() const;
    void setMenuBarIconStyle(const QString& style);
    QString menuBarCustomIconPath() const;
    bool showDownloadsInDock() const;
    void setShowDownloadsInDock(bool show);
    bool showMenuBarExtras() const;
    void setShowMenuBarExtras(bool show);
    bool blackWhiteTrayIcons() const;
    void setBlackWhiteTrayIcons(bool enabled);
    bool transparentNotificationCenter() const;
    void setTransparentNotificationCenter(bool enabled);
    int trayExtrasRefreshMs() const;
    void setTrayExtrasRefreshMs(int intervalMs);
    QString uiLanguage() const;
    QString effectiveLanguage() const;
    void withTrayOnScreen(const std::function<void()>& action);
    void withTrayOnScreenOnGuiThread(const std::function<void()>& action);
    void setTrayUiaBusy(bool busy);
    bool trayUiaBusy() const;
    bool showDesktopActive() const;

signals:
    void taskbarHiddenChanged();
    void dockAutoHiddenChanged();
    void shellActionLogged(const QString& message);
    void shellActiveChanged();
    void settingsVisibleChanged();
    void dockIconSizeChanged();
    void showTopBarChanged();
    void autoHideWindowsTaskbarChanged();
    void keepTaskbarAutoHideOnExitChanged();
    void dockHoverBounceChanged();
    void dockDragFadeEnabledChanged();
    void dockStaticIconsChanged();
    void dockSeparateTransientAppsChanged();
    void appearanceModeChanged();
    void darkThemeChanged();
    void dockLightStyleChanged();
    void startWithWindowsChanged();
    void menuBarAppNameChanged();
    void menuBarItemsChanged();
    void explorerIconStyleChanged();
    void trashIconStyleChanged();
    void menuBarIconStyleChanged();
    void menuBarCustomIconPathChanged();
    void showDownloadsInDockChanged();
    void showMenuBarExtrasChanged();
    void blackWhiteTrayIconsChanged();
    void transparentNotificationCenterChanged();
    void trayExtrasRefreshMsChanged();
    void shellLayoutRestoreNeeded();
    void languageChanged();
    void showDesktopActiveChanged();

private:
    struct DesktopPeekWindow
    {
        quintptr hwndValue = 0;
        QByteArray placement;
    };

    QString normalizeMenuBarIconStyle(const QString& style) const;
    QString normalizeDockLightStyle(const QString& style) const;
    QString bundledMenuBarIconResource(const QString& style, bool darkTheme) const;
    QString menuBarIconsDirectory() const;
    void loadSettings();
    void saveSettings();
    bool setTaskbarVisible(bool visible);
    void capturePreShellTaskbarState();
    void showTaskbarWindows();
    void restoreTaskbarRegistrySettings();
    void setTaskbarRegistryAutoHide(bool enabled);
    void updateTaskbarVisibility();
    bool detectForegroundOccupiesScreen() const;
    void updateFullscreenState();
    void updateForegroundMenuBar();
    QStringList defaultMenuBarItems() const;
    QString normalizeAppearanceMode(const QString& mode) const;
    bool effectiveDarkTheme() const;
    bool isWindowsDarkMode() const;
    void updateEffectiveAppearance();
    QString normalizeUiLanguage(const QString& language) const;
    void syncWindowsStartup(bool enabled);
    void reconcileWindowsStartup();
    void relocateWindowOffScreen(quintptr hwndValue, bool force = false);
    void restoreWindowPosition(quintptr hwndValue);
    void restoreAllTaskbarPositions();
    void beginTrayOnScreenScope();
    void endTrayOnScreenScope();
    QVector<DesktopPeekWindow> collectDesktopPeekWindows() const;
    void restoreDesktopPeekWindows();
    bool minimizeDesktopPeekWindows(bool persistent);
    bool tryShellToggleDesktop();

    bool m_taskbarHidden = false;
    unsigned long m_originalTaskbarState = 0;
    bool m_hasOriginalTaskbarState = false;
    bool m_hasOriginalStuckRectsSettings = false;
    bool m_taskbarShellModified = false;
    QByteArray m_originalStuckRectsSettings;
    QString m_stuckRectsRegPath;
    bool m_shellActive = false;
    bool m_settingsVisible = true;
    int m_dockIconSize = 54;
    bool m_showTopBar = true;
    bool m_autoHideWindowsTaskbar = true;
    bool m_keepTaskbarAutoHideOnExit = false;
    bool m_dockHoverBounce = true;
    bool m_dockDragFadeEnabled = false;
    bool m_dockStaticIcons = false;
    bool m_dockSeparateTransientApps = true;
    QString m_appearanceMode = QStringLiteral("auto");
    bool m_cachedEffectiveDarkTheme = false;
    QString m_dockLightStyle = QStringLiteral("white");
    bool m_startWithWindows = false;
    bool m_dockAutoHidden = false;
    bool m_dockRevealed = false;
    QString m_menuBarAppName = QStringLiteral("Finder");
    QStringList m_menuBarItems;
    QString m_explorerIconStyle = QStringLiteral("default");
    QString m_trashIconStyle = QStringLiteral("windows");
    QString m_menuBarIconStyle = QStringLiteral("apple");
    QString m_menuBarCustomIconPath;
    bool m_showDownloadsInDock = true;
    bool m_showMenuBarExtras = true;
    bool m_blackWhiteTrayIcons = false;
    bool m_transparentNotificationCenter = false;
    int m_trayExtrasRefreshMs = 1500;
    QString m_uiLanguage = QStringLiteral("system");
    QHash<quintptr, QRect> m_savedTaskbarRects;
    QVector<DesktopPeekWindow> m_desktopPeekWindows;
    quintptr m_desktopPeekForeground = 0;
    qint64 m_lastTaskbarRelocateLogMs = 0;
    std::atomic_bool m_trayUiaBusy { false };
    bool m_trayScopeWasOffscreen = false;
    bool m_showDesktopActive = false;
    QTimer* m_fullscreenTimer = nullptr;
    QTimer* m_appearanceTimer = nullptr;
};
