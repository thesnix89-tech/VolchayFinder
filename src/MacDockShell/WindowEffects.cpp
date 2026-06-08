#include "WindowEffects.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QPoint>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include <memory>

#include <windows.h>
#include <windowsx.h>
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

BOOL CALLBACK collectChildWindows(HWND hwnd, LPARAM lParam)
{
    auto* hwnds = reinterpret_cast<QSet<HWND>*>(lParam);
    hwnds->insert(hwnd);
    return TRUE;
}

QSet<HWND> collectWindowTree(HWND root)
{
    QSet<HWND> hwnds;
    if (!root) {
        return hwnds;
    }
    hwnds.insert(root);
    EnumChildWindows(root, collectChildWindows, reinterpret_cast<LPARAM>(&hwnds));
    return hwnds;
}

HRGN buildClientRegion(HWND hwnd, const QList<QRect>& screenRegions)
{
    HRGN combined = nullptr;
    for (const QRect& screenRect : screenRegions) {
        POINT topLeft = { screenRect.left(), screenRect.top() };
        POINT bottomRight = { screenRect.right() + 1, screenRect.bottom() + 1 };
        ScreenToClient(hwnd, &topLeft);
        ScreenToClient(hwnd, &bottomRight);

        HRGN piece = CreateRectRgn(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
        if (!piece) {
            continue;
        }

        if (!combined) {
            combined = piece;
        } else {
            const int merge = CombineRgn(combined, combined, piece, RGN_OR);
            DeleteObject(piece);
            if (merge == ERROR) {
                DeleteObject(combined);
                return nullptr;
            }
        }
    }
    return combined;
}

void applyClientRegion(HWND hwnd, HRGN region)
{
    if (!hwnd) {
        return;
    }
    // SetWindowRgn takes ownership of the region handle on success.
    SetWindowRgn(hwnd, region, TRUE);
}

void clearClientRegion(HWND hwnd)
{
    if (!hwnd) {
        return;
    }
    SetWindowRgn(hwnd, nullptr, TRUE);
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

struct DockWindowHitData
{
    QPointer<QWindow> window;
    HWND rootHwnd = nullptr;
    QList<QRect> clipRegions;
    QList<QRect> hitRegions;
    QSet<HWND> hwndTree;
    bool dropHoverActive = false;
    bool dropCaptureActive = false;
};

class DockClickThroughState
{
public:
    QHash<HWND, std::shared_ptr<DockWindowHitData>> dataByRoot;

    std::shared_ptr<DockWindowHitData> dataForHwnd(HWND hwnd) const
    {
        if (!hwnd) {
            return {};
        }
        for (auto it = dataByRoot.constBegin(); it != dataByRoot.constEnd(); ++it) {
            const HWND root = it.key();
            for (HWND walk = hwnd; walk; walk = GetParent(walk)) {
                if (walk == root) {
                    return it.value();
                }
            }
        }
        return {};
    }

    bool containsScreenPoint(const QList<QRect>& regions, const QPoint& screenPoint) const
    {
        for (const QRect& region : regions) {
            if (region.contains(screenPoint)) {
                return true;
            }
        }
        return false;
    }
};

class DockHitTestFilter final : public QAbstractNativeEventFilter
{
public:
    explicit DockHitTestFilter(DockClickThroughState* state)
        : m_state(state)
    {
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
    {
        if (!m_state || !result) {
            return false;
        }

        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }

        const auto* msg = static_cast<const MSG*>(message);
        if (!msg || msg->message != WM_NCHITTEST) {
            return false;
        }

        const auto data = m_state->dataForHwnd(reinterpret_cast<HWND>(msg->hwnd));
        if (!data) {
            return false;
        }

        const int x = GET_X_LPARAM(msg->lParam);
        const int y = GET_Y_LPARAM(msg->lParam);
        const QPoint screenPoint(x, y);

        if ((data->dropHoverActive || data->dropCaptureActive) && data->window) {
            const QRect windowRect = data->window->geometry();
            if (windowRect.contains(screenPoint)) {
                *result = HTCLIENT;
                return true;
            }
        }

        if (data->window) {
            const QRect windowRect = data->window->geometry();
            if (windowRect.contains(screenPoint)) {
                const int dropBandHeight = qMax(72, windowRect.height() / 3);
                const int dropBandTop = windowRect.bottom() - dropBandHeight;
                if (screenPoint.y() >= dropBandTop) {
                    return false;
                }
            }
        }

        // Pass-through only outside pill/icons. Inside → don't intercept (Qt handles dock).
        if (!m_state->containsScreenPoint(data->hitRegions, screenPoint)) {
            *result = HTTRANSPARENT;
            return true;
        }
        return false;
    }

private:
    DockClickThroughState* m_state = nullptr;
};

static void applyRegionsToWindowTree(const std::shared_ptr<DockWindowHitData>& data)
{
    if (!data || !data->rootHwnd) {
        return;
    }

    const QSet<HWND> nextTree = collectWindowTree(data->rootHwnd);
    for (HWND hwnd : data->hwndTree) {
        if (!nextTree.contains(hwnd)) {
            clearClientRegion(hwnd);
        }
    }
    data->hwndTree = nextTree;

    for (HWND hwnd : data->hwndTree) {
        HRGN region = buildClientRegion(hwnd, data->clipRegions);
        if (!region) {
            region = CreateRectRgn(0, 0, 0, 0);
        }
        applyClientRegion(hwnd, region);
    }
}

static void clearRegionsFromWindowTree(const std::shared_ptr<DockWindowHitData>& data)
{
    if (!data) {
        return;
    }

    for (HWND hwnd : std::as_const(data->hwndTree)) {
        clearClientRegion(hwnd);
    }
    data->hwndTree.clear();
}

QList<QRect> variantListToRects(const QVariantList& list)
{
    QList<QRect> rects;
    rects.reserve(list.size());
    for (const QVariant& entry : list) {
        if (entry.canConvert<QRectF>()) {
            rects.append(entry.toRectF().toRect());
        } else if (entry.canConvert<QRect>()) {
            rects.append(entry.toRect());
        }
    }
    return rects;
}

QList<QRect> localRectsToScreen(QWindow* window, const QList<QRect>& localRects)
{
    QList<QRect> screen;
    if (!window) {
        return localRects;
    }
    screen.reserve(localRects.size());
    for (const QRect& local : localRects) {
        const QPoint topLeft = window->mapToGlobal(local.topLeft());
        const QPoint bottomRight = window->mapToGlobal(local.bottomRight());
        screen.append(QRect(topLeft, bottomRight).normalized());
    }
    return screen;
}

static void ensureHwndResyncHook(QWindow* window, DockClickThroughState* state)
{
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    if (!quickWindow || !state) {
        return;
    }

    static QSet<QWindow*> hooked;
    if (hooked.contains(window)) {
        return;
    }
    hooked.insert(window);

    auto* resyncTimer = new QTimer(window);
    resyncTimer->setInterval(80);
    resyncTimer->setSingleShot(true);

    QObject::connect(resyncTimer, &QTimer::timeout, window, [state, window]() {
        const HWND root = reinterpret_cast<HWND>(window->winId());
        if (!root) {
            return;
        }
        const auto data = state->dataByRoot.value(root);
        if (data) {
            applyRegionsToWindowTree(data);
        }
    });

    QObject::connect(quickWindow, &QQuickWindow::frameSwapped, window, [resyncTimer]() {
        resyncTimer->start();
    });
}

WindowEffects::WindowEffects(QObject* parent)
    : QObject(parent)
    , m_dockClickThroughState(std::make_unique<DockClickThroughState>())
    , m_dockHitTestFilter(std::make_unique<DockHitTestFilter>(m_dockClickThroughState.get()))
{
    QGuiApplication::instance()->installNativeEventFilter(m_dockHitTestFilter.get());
}

WindowEffects::~WindowEffects()
{
    if (m_dockHitTestFilter) {
        QGuiApplication::instance()->removeNativeEventFilter(m_dockHitTestFilter.get());
    }
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

void WindowEffects::enableDockClickThrough(QWindow* window)
{
    if (!window || !m_dockClickThroughState) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    if (!m_dockClickThroughState->dataByRoot.contains(hwnd)) {
        auto data = std::make_shared<DockWindowHitData>();
        data->window = window;
        data->rootHwnd = hwnd;
        m_dockClickThroughState->dataByRoot.insert(hwnd, data);
    }

    ensureHwndResyncHook(window, m_dockClickThroughState.get());
}

void WindowEffects::updateDockHitRegions(QWindow* window, const QVariantList& clipRegions,
                                         const QVariantList& hitRegions)
{
    if (!window || !m_dockClickThroughState) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    auto data = m_dockClickThroughState->dataByRoot.value(hwnd);
    if (!data) {
        data = std::make_shared<DockWindowHitData>();
        data->window = window;
        data->rootHwnd = hwnd;
        m_dockClickThroughState->dataByRoot.insert(hwnd, data);
    }

    const QList<QRect> clipLocal = variantListToRects(clipRegions);
    const QList<QRect> hitLocal = variantListToRects(hitRegions);
    data->clipRegions = localRectsToScreen(window, clipLocal);
    data->hitRegions = localRectsToScreen(window, hitLocal);
    applyRegionsToWindowTree(data);
}

void WindowEffects::clearDockHitRegions(QWindow* window)
{
    if (!window || !m_dockClickThroughState) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    const auto data = m_dockClickThroughState->dataByRoot.take(hwnd);
    if (data) {
        clearRegionsFromWindowTree(data);
    }
}

void WindowEffects::applyDockDropHover(QWindow* window, bool active)
{
    if (!window) {
        return;
    }

    const bool previous = m_dropHoverActive.value(window, false);
    if (previous == active) {
        return;
    }

    if (active) {
        m_dropHoverActive.insert(window, true);
    } else {
        m_dropHoverActive.remove(window);
    }

    if (m_dockClickThroughState) {
        const HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (hwnd) {
            const auto data = m_dockClickThroughState->dataByRoot.value(hwnd);
            if (data) {
                data->dropHoverActive = active;
            }
        }
    }

    emit dockDropHoverChanged(window, active);
}

void WindowEffects::setDockDropHover(QWindow* window, bool active)
{
    if (!window) {
        return;
    }

    QPointer<QTimer>& leaveTimer = m_dropHoverLeaveTimers[window];
    if (!leaveTimer) {
        leaveTimer = new QTimer(window);
        leaveTimer->setSingleShot(true);
        leaveTimer->setInterval(200);
        QObject::connect(leaveTimer, &QTimer::timeout, this, [this, window]() {
            applyDockDropHover(window, false);
        });
    }

    if (active) {
        leaveTimer->stop();
        applyDockDropHover(window, true);
        return;
    }

    leaveTimer->start();
}

void WindowEffects::setDockDropCapture(QWindow* window, bool active)
{
    if (!window) {
        return;
    }

    if (active) {
        m_dropCaptureActive.insert(window, true);
    } else {
        m_dropCaptureActive.remove(window);
    }

    if (m_dockClickThroughState) {
        const HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (hwnd) {
            const auto data = m_dockClickThroughState->dataByRoot.value(hwnd);
            if (data) {
                data->dropCaptureActive = active;
            }
        }
    }
}

void WindowEffects::setDockDropPointer(QWindow* window, int globalX, int globalY)
{
    if (!window) {
        return;
    }

    const bool tracked = m_dropHoverActive.value(window, false)
            || m_dropCaptureActive.value(window, false);
    if (!tracked) {
        return;
    }

    emit dockDropPointerChanged(window, globalX, globalY);
}

void WindowEffects::setDockExternalDragPath(QWindow* window, const QString& path)
{
    if (!window) {
        return;
    }

    const bool tracked = m_dropHoverActive.value(window, false)
            || m_dropCaptureActive.value(window, false);
    if (!tracked) {
        return;
    }

    emit dockExternalDragPathChanged(window, path);
}

void WindowEffects::notifyDockExternalDrop(QWindow* window, const QString& path, int index)
{
    if (!window || path.isEmpty()) {
        return;
    }

    if (QPointer<QTimer> leaveTimer = m_dropHoverLeaveTimers.value(window)) {
        leaveTimer->stop();
    }
    applyDockDropHover(window, false);
    setDockDropCapture(window, false);

    emit dockExternalDrop(window, path, index);
}

void WindowEffects::updateDockDropLayout(QWindow* window, qreal pillLocalLeft, int stride, int iconSize, int itemCount)
{
    if (!window) {
        return;
    }

    DockDropLayout layout;
    layout.pillLocalLeft = pillLocalLeft;
    layout.stride = stride;
    layout.iconSize = iconSize;
    layout.itemCount = qMax(0, itemCount);
    m_dropLayouts.insert(window, layout);
}

int WindowEffects::dockDropIndexForGlobalPoint(QWindow* window, int globalX, int globalY) const
{
    if (!window) {
        return -1;
    }

    const DockDropLayout layout = m_dropLayouts.value(window);
    if (layout.stride <= 0 || layout.iconSize <= 0) {
        return -1;
    }

    const QPoint local = window->mapFromGlobal(QPoint(globalX, globalY));
    if (local.x() < 0 || local.x() > window->width()) {
        return -1;
    }

    const qreal centerX = local.x() - layout.pillLocalLeft - layout.iconSize / 2.0;
    const int insertionCount = layout.itemCount + 1;
    int candidate = 0;
    for (int slot = 1; slot < insertionCount; ++slot) {
        const qreal boundary = ((slot - 1) * layout.stride + layout.iconSize / 2.0
                                + slot * layout.stride + layout.iconSize / 2.0) / 2.0;
        if (centerX >= boundary) {
            candidate = slot;
        }
    }

    return qBound(0, candidate, qMax(0, layout.itemCount));
}
