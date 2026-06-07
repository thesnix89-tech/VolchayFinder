#include "WindowEffects.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QQuickWindow>

#include <windows.h>
#include <dwmapi.h>

namespace {

enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19
};

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

struct ACCENT_POLICY {
    DWORD AccentState = ACCENT_DISABLED;
    DWORD AccentFlags = 0;
    DWORD GradientColor = 0;
    DWORD AnimationId = 0;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib = WCA_ACCENT_POLICY;
    PVOID pvData = nullptr;
    SIZE_T cbData = 0;
};

using SetWindowCompositionAttributePtr = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

void applyAccentPolicy(HWND hwnd, DWORD accentState, DWORD gradientColorAbgr)
{
    ACCENT_POLICY policy = {};
    policy.AccentState = accentState;
    policy.GradientColor = gradientColorAbgr;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &policy;
    data.cbData = sizeof(policy);

    auto setWindowCompositionAttribute = reinterpret_cast<SetWindowCompositionAttributePtr>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (setWindowCompositionAttribute) {
        setWindowCompositionAttribute(hwnd, &data);
    }
}

void clearSystemBackdrop(HWND hwnd)
{
    const DWORD backdrop = DWMSBT_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    applyAccentPolicy(hwnd, ACCENT_DISABLED, 0);

    const MARGINS margins = { 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

class TopBarHoverFilter final : public QAbstractNativeEventFilter
{
public:
    explicit TopBarHoverFilter(HWND hwnd)
        : m_hwnd(hwnd)
    {
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override
    {
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }

        const auto* msg = static_cast<const MSG*>(message);
        if (!msg || msg->hwnd != m_hwnd) {
            return false;
        }

        switch (msg->message) {
        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE:
            if (!m_tracking) {
                armHoverLeave(msg->hwnd);
                m_tracking = true;
            }
            break;
        case WM_MOUSELEAVE:
            m_tracking = false;
            break;
        default:
            break;
        }

        return false;
    }

private:
    static void armHoverLeave(HWND hwnd)
    {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hwnd;
        TrackMouseEvent(&track);
    }

    HWND m_hwnd = nullptr;
    bool m_tracking = false;
};

} // namespace

WindowEffects::WindowEffects(QObject* parent)
    : QObject(parent)
{
}

WindowEffects::~WindowEffects()
{
    if (m_topBarHoverFilter) {
        QGuiApplication::instance()->removeNativeEventFilter(m_topBarHoverFilter.get());
    }
}

void WindowEffects::applyDockGlass(QWindow* window)
{
    if (!window) {
        return;
    }

    if (auto* quickWindow = qobject_cast<QQuickWindow*>(window)) {
        quickWindow->setColor(Qt::transparent);
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    // System acrylic on the whole dock HWND painted a grey band across the screen.
    // Keep the window fully transparent; only the QML pill draws the dock surface.
    clearSystemBackdrop(hwnd);
}

void WindowEffects::enableHoverTracking(QWindow* window)
{
    if (!window) {
        return;
    }

    const auto armWindow = [window]() {
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (!hwnd) {
            return;
        }

        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE | TME_HOVER;
        track.hwndTrack = hwnd;
        track.dwHoverTime = 1;
        TrackMouseEvent(&track);
    };

    QObject::connect(window, &QWindow::visibleChanged, window, [window, armWindow]() {
        if (window->isVisible()) {
            armWindow();
        }
    });

    if (window->isVisible()) {
        armWindow();
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    if (m_topBarHoverFilter) {
        QGuiApplication::instance()->removeNativeEventFilter(m_topBarHoverFilter.get());
    }

    m_topBarHoverFilter = std::make_unique<TopBarHoverFilter>(hwnd);
    QGuiApplication::instance()->installNativeEventFilter(m_topBarHoverFilter.get());
}
