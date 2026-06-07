#include "WindowEffects.h"

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

} // namespace

WindowEffects::WindowEffects(QObject* parent)
    : QObject(parent)
{
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
