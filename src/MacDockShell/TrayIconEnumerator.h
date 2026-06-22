#pragma once

#include <QString>
#include <QSet>
#include <QVector>
#include <QRect>

#include <functional>

struct IUIAutomationElement;
struct IUIAutomation;

struct TrayIconInfo
{
    QString stableId;
    QString tooltip;
    QString automationId;
    QString className;
    QRect boundingRect;
    quintptr nativeWindowHandle = 0;
    quint32 processId = 0;
    QString exePath;
    bool inOverflow = false;
    bool isSystemPromoted = false;
    QVector<int> runtimeId;
};

struct MirroredTrayMenuItem
{
    QString text;
    bool separator = false;
    bool enabled = true;
    bool checked = false;
    bool hasSubmenu = false;
    bool nativeFallback = false;
    QVector<MirroredTrayMenuItem> children;
};

class TrayIconEnumerator
{
public:
    using TrayOnScreenScope = std::function<void(const std::function<void()>&)>;
    using GuiInvoker = std::function<void(const std::function<void()>&)>;

    TrayIconEnumerator();
    ~TrayIconEnumerator();

    void setTrayOnScreenScope(TrayOnScreenScope scope);
    void setGuiInvoker(GuiInvoker invoker);
    void setDebugEnabled(bool enabled);

    QVector<TrayIconInfo> enumerate(bool includeOverflow = false);
    QVector<TrayIconInfo> enumerateOverflow();
    bool activate(const TrayIconInfo& icon);
    bool showContextMenu(const TrayIconInfo& icon);
    QVector<MirroredTrayMenuItem> captureContextMenu(const TrayIconInfo& icon);
    bool invokeMirroredItem(const TrayIconInfo& icon, const QVector<int>& path);
    QString lastEnumerateLog() const;
    QString lastDebugLog() const;
    QString lastInteractionDetail() const;
    QString lastMirrorDetail() const;

private:
    struct TrayUiaState;

    bool ensureAutomation();
    void releaseAutomation();
    IUIAutomationElement* trayRootElement();
    IUIAutomationElement* desktopRootElement();
    QVector<TrayIconInfo> enumerateTraySubtree(bool includeOverflow);
    QVector<TrayIconInfo> enumerateWin32TrayIcons();
    QVector<TrayIconInfo> enumerateWin11SystemTrayIcons(IUIAutomationElement* trayRoot);
    QVector<TrayIconInfo> enumerateWin11OverflowIcons();
    QVector<TrayIconInfo> enumerateAreaInSubtree(IUIAutomationElement* root,
                                                   const wchar_t* areaName,
                                                   bool isSystemPromoted,
                                                   bool inOverflow);
    bool clickAtScreenPoint(int x, int y, bool rightButton);
    QString processExePath(quint32 processId) const;
    QString resolveExePathForIcon(const TrayIconInfo& info) const;
    QVector<TrayIconInfo> collectOverflowIcons(QString* detailLog);
    bool openOverflowPanel(bool allowMouseFallback = true);
    bool openOverflowChevron(IUIAutomation* automation, bool allowMouseFallback);
    bool openOverflowViaKeyboard(IUIAutomation* automation, IUIAutomationElement* chevron);
    bool waitForOverflowPanel(int maxMs);
    void closeOverflowPanel();
    bool sendShiftF10ContextMenu();
    bool clickTaskbarChevronFallback();
    IUIAutomationElement* findOverflowIconElement(const TrayIconInfo& icon);
    IUIAutomationElement* findVisibleTrayIconElement(const TrayIconInfo& icon);
    bool interactWithElement(IUIAutomationElement* element, bool rightButton, bool inOverflow);
    bool interactWithIcon(const TrayIconInfo& icon, bool rightButton);
    QVector<MirroredTrayMenuItem> captureOpenMenuLevel(int depth, QSet<quintptr>* visitedMenus);
    bool replayOpenMenuPath(const QVector<int>& path);

    TrayUiaState* m_uia = nullptr;
    TrayOnScreenScope m_trayOnScreenScope;
    GuiInvoker m_guiInvoker;
    QString m_lastEnumerateLog;
    QString m_lastDebugLog;
    QString m_overflowEnumerateDetail;
    QString m_lastInteractionDetail;
    QString m_lastMirrorDetail;
    bool m_debugEnabled = false;
    bool m_lastChevronLocated = false;
    bool m_lastOverflowPanelOpen = false;
    int m_lastWin32IconCount = 0;
};
