#include "AppBarController.h"

#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QMetaObject>

#include <windows.h>
#include <shellapi.h>

namespace {

constexpr wchar_t kAppBarMessageName[] = L"MacDockShell_AppBarMessage";

class AppBarNativeFilter : public QAbstractNativeEventFilter
{
public:
    AppBarNativeFilter(HWND hwnd, UINT callbackMessage, AppBarController* controller)
        : m_hwnd(hwnd)
        , m_callbackMessage(callbackMessage)
        , m_controller(controller)
    {
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override
    {
        if (eventType != "windows_generic_MSG" || !message || !m_controller) {
            return false;
        }

        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->hwnd != m_hwnd || msg->message != m_callbackMessage) {
            return false;
        }

        switch (msg->wParam) {
        case ABN_STATECHANGE:
        case ABN_POSCHANGED:
        case ABN_WINDOWARRANGE:
            QMetaObject::invokeMethod(m_controller,
                                      "handleShellLayoutChange",
                                      Qt::QueuedConnection);
            break;
        default:
            break;
        }

        return false;
    }

private:
    HWND m_hwnd = nullptr;
    UINT m_callbackMessage = 0;
    AppBarController* m_controller = nullptr;
};

} // namespace

AppBarController::AppBarController(QObject* parent)
    : QObject(parent)
{
    m_callbackMessage = RegisterWindowMessageW(kAppBarMessageName);
}

AppBarController::~AppBarController()
{
    unregisterTopBar();
}

bool AppBarController::registerTopBar(void* hwnd, int height)
{
    if (!hwnd) {
        emit logMessage(QStringLiteral("AppBar register failed: hwnd is null."));
        return false;
    }

    unregisterTopBar();

    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = static_cast<HWND>(hwnd);
    abd.uCallbackMessage = m_callbackMessage;

    SHAppBarMessage(ABM_NEW, &abd);
    m_registered = true;
    m_hwnd = hwnd;
    m_topBarHeight = height;
    installNativeFilter(hwnd);
    emit logMessage(QStringLiteral("AppBar registered."));
    updateTopBarRect(hwnd, height);
    return true;
}

void AppBarController::updateTopBarRect(void* hwnd, int height)
{
    if (!m_registered || !hwnd) {
        return;
    }

    m_topBarHeight = height;

    RECT screenRect = {};
    screenRect.right = GetSystemMetrics(SM_CXSCREEN);
    screenRect.bottom = GetSystemMetrics(SM_CYSCREEN);

    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = static_cast<HWND>(hwnd);
    abd.uEdge = ABE_TOP;
    abd.rc.left = 0;
    abd.rc.top = 0;
    abd.rc.right = screenRect.right;
    abd.rc.bottom = height;

    SHAppBarMessage(ABM_QUERYPOS, &abd);
    abd.rc.bottom = abd.rc.top + height;
    SHAppBarMessage(ABM_SETPOS, &abd);

    SetWindowPos(static_cast<HWND>(hwnd), HWND_TOPMOST,
                 abd.rc.left, abd.rc.top,
                 abd.rc.right - abd.rc.left,
                 abd.rc.bottom - abd.rc.top,
                 SWP_NOACTIVATE);

    emit logMessage(QStringLiteral("AppBar top rect updated."));
}

void AppBarController::unregisterTopBar()
{
    removeNativeFilter();

    if (!m_registered || !m_hwnd) {
        return;
    }

    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = static_cast<HWND>(m_hwnd);
    SHAppBarMessage(ABM_REMOVE, &abd);

    m_registered = false;
    m_hwnd = nullptr;
    m_topBarHeight = 0;
    emit logMessage(QStringLiteral("AppBar unregistered."));
}

void AppBarController::handleShellLayoutChange()
{
    if (!m_registered || !m_hwnd) {
        return;
    }

    updateTopBarRect(m_hwnd, m_topBarHeight);
    emit layoutChanged();
}

void AppBarController::installNativeFilter(void* hwnd)
{
    removeNativeFilter();
    m_nativeFilter = std::make_unique<AppBarNativeFilter>(
        static_cast<HWND>(hwnd), m_callbackMessage, this);
    QGuiApplication::instance()->installNativeEventFilter(m_nativeFilter.get());
}

void AppBarController::removeNativeFilter()
{
    if (!m_nativeFilter) {
        return;
    }

    if (QGuiApplication::instance()) {
        QGuiApplication::instance()->removeNativeEventFilter(m_nativeFilter.get());
    }
    m_nativeFilter.reset();
}
