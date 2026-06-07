#include "AppBarController.h"

#include <windows.h>
#include <shellapi.h>

namespace {
constexpr wchar_t kAppBarMessageName[] = L"MacDockShell_AppBarMessage";
}

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

    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = static_cast<HWND>(hwnd);
    abd.uCallbackMessage = m_callbackMessage;

    SHAppBarMessage(ABM_NEW, &abd);
    m_registered = true;
    m_hwnd = hwnd;
    emit logMessage(QStringLiteral("AppBar registered."));
    updateTopBarRect(hwnd, height);
    return true;
}

void AppBarController::updateTopBarRect(void* hwnd, int height)
{
    if (!m_registered || !hwnd) {
        return;
    }

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
    if (!m_registered || !m_hwnd) {
        return;
    }

    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd = static_cast<HWND>(m_hwnd);
    SHAppBarMessage(ABM_REMOVE, &abd);

    m_registered = false;
    m_hwnd = nullptr;
    emit logMessage(QStringLiteral("AppBar unregistered."));
}
