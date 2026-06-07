#pragma once

#include <QObject>
#include <QString>

class TaskbarController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool taskbarHidden READ taskbarHidden NOTIFY taskbarHiddenChanged)
    Q_PROPERTY(bool shellActive READ shellActive WRITE setShellActive NOTIFY shellActiveChanged)
    Q_PROPERTY(bool settingsVisible READ settingsVisible WRITE setSettingsVisible NOTIFY settingsVisibleChanged)
    Q_PROPERTY(int dockIconSize READ dockIconSize WRITE setDockIconSize NOTIFY dockIconSizeChanged)
    Q_PROPERTY(bool showTopBar READ showTopBar WRITE setShowTopBar NOTIFY showTopBarChanged)
    Q_PROPERTY(bool autoHideWindowsTaskbar READ autoHideWindowsTaskbar WRITE setAutoHideWindowsTaskbar NOTIFY autoHideWindowsTaskbarChanged)
    Q_PROPERTY(bool macOSSelectionStyle READ macOSSelectionStyle WRITE setMacOSSelectionStyle NOTIFY macOSSelectionStyleChanged)

public:
    explicit TaskbarController(QObject* parent = nullptr);
    ~TaskbarController() override;

    Q_INVOKABLE bool hideTaskbar();
    Q_INVOKABLE bool showTaskbar();
    Q_INVOKABLE void restoreShell();
    Q_INVOKABLE void quitApplication();
    Q_INVOKABLE void apply(bool autoHideWindowsTaskbar, bool showTopBar, bool macOSSelectionStyle, int iconSize);
    Q_INVOKABLE void restartExplorer();

    bool taskbarHidden() const;
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
    bool macOSSelectionStyle() const;
    void setMacOSSelectionStyle(bool enable);

signals:
    void taskbarHiddenChanged();
    void shellActionLogged(const QString& message);
    void shellActiveChanged();
    void settingsVisibleChanged();
    void dockIconSizeChanged();
    void showTopBarChanged();
    void autoHideWindowsTaskbarChanged();
    void macOSSelectionStyleChanged();

private:
    bool setTaskbarVisible(bool visible);
    void updateTaskbarVisibility();

    bool m_taskbarHidden = false;
    unsigned long m_originalTaskbarState = 0;
    bool m_hasOriginalTaskbarState = false;
    bool m_shellActive = false;
    bool m_settingsVisible = true;
    int m_dockIconSize = 54;
    bool m_showTopBar = true;
    bool m_autoHideWindowsTaskbar = true;
    bool m_macOSSelectionStyle = false;
};
