#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

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
    Q_PROPERTY(bool dockHoverBounce READ dockHoverBounce WRITE setDockHoverBounce NOTIFY dockHoverBounceChanged)
    Q_PROPERTY(bool dockStaticIcons READ dockStaticIcons WRITE setDockStaticIcons NOTIFY dockStaticIconsChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY startWithWindowsChanged)
    Q_PROPERTY(QString menuBarAppName READ menuBarAppName NOTIFY menuBarAppNameChanged)
    Q_PROPERTY(QStringList menuBarItems READ menuBarItems NOTIFY menuBarItemsChanged)
    Q_PROPERTY(QString explorerIconStyle READ explorerIconStyle WRITE setExplorerIconStyle NOTIFY explorerIconStyleChanged)

public:
    explicit TaskbarController(QObject* parent = nullptr);
    ~TaskbarController() override;

    Q_INVOKABLE bool hideTaskbar();
    Q_INVOKABLE bool showTaskbar();
    Q_INVOKABLE void restoreShell();
    Q_INVOKABLE void quitApplication();
    Q_INVOKABLE void apply(bool autoHideWindowsTaskbar, bool showTopBar, int iconSize, bool dockHoverBounce, bool dockStaticIcons, bool darkTheme, bool startWithWindows, const QString& explorerIconStyle);
    Q_INVOKABLE void tryAutostartShell();

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
    bool dockHoverBounce() const;
    void setDockHoverBounce(bool enabled);
    bool dockStaticIcons() const;
    void setDockStaticIcons(bool enabled);
    bool darkTheme() const;
    void setDarkTheme(bool enabled);
    bool startWithWindows() const;
    void setStartWithWindows(bool enabled);
    QString menuBarAppName() const;
    QStringList menuBarItems() const;
    QString explorerIconStyle() const;
    void setExplorerIconStyle(const QString& style);

signals:
    void taskbarHiddenChanged();
    void dockAutoHiddenChanged();
    void shellActionLogged(const QString& message);
    void shellActiveChanged();
    void settingsVisibleChanged();
    void dockIconSizeChanged();
    void showTopBarChanged();
    void autoHideWindowsTaskbarChanged();
    void dockHoverBounceChanged();
    void dockStaticIconsChanged();
    void darkThemeChanged();
    void startWithWindowsChanged();
    void menuBarAppNameChanged();
    void menuBarItemsChanged();
    void explorerIconStyleChanged();

private:
    void loadSettings();
    void saveSettings();
    bool setTaskbarVisible(bool visible);
    void updateTaskbarVisibility();
    bool detectForegroundOccupiesScreen() const;
    void updateFullscreenState();
    void updateForegroundMenuBar();
    void syncWindowsStartup(bool enabled);
    void reconcileWindowsStartup();

    bool m_taskbarHidden = false;
    unsigned long m_originalTaskbarState = 0;
    bool m_hasOriginalTaskbarState = false;
    bool m_shellActive = false;
    bool m_settingsVisible = true;
    int m_dockIconSize = 54;
    bool m_showTopBar = true;
    bool m_autoHideWindowsTaskbar = true;
    bool m_dockHoverBounce = true;
    bool m_dockStaticIcons = false;
    bool m_darkTheme = false;
    bool m_startWithWindows = false;
    bool m_dockAutoHidden = false;
    bool m_dockRevealed = false;
    QString m_menuBarAppName = QStringLiteral("Finder");
    QStringList m_menuBarItems;
    QString m_explorerIconStyle = QStringLiteral("default");
    QTimer* m_fullscreenTimer = nullptr;
};
