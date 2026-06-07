#include "DockDropTarget.h"

#include "DockModel.h"
#include "WindowEffects.h"

#include <QGuiApplication>
#include <QWindow>

#include <oleidl.h>
#include <shlobj.h>

namespace {

QString pathFromDataObject(IDataObject* dataObject)
{
    if (!dataObject) {
        return {};
    }

    FORMATETC format = {};
    format.cfFormat = CF_HDROP;
    format.dwAspect = DVASPECT_CONTENT;
    format.lindex = -1;
    format.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(dataObject->GetData(&format, &medium)) || !medium.hGlobal) {
        return {};
    }

    const HDROP drop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
    if (!drop) {
        ReleaseStgMedium(&medium);
        return {};
    }

    QString result;
    if (DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) > 0) {
        wchar_t buffer[MAX_PATH * 4] = {};
        if (DragQueryFileW(drop, 0, buffer, static_cast<UINT>(std::size(buffer))) > 0) {
            result = QString::fromWCharArray(buffer);
        }
    }

    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return result;
}

struct DropTargetImpl final : public IDropTarget
{
    DropTargetImpl(QWindow* window, DockModel* dockModel, WindowEffects* windowEffects)
        : m_window(window)
        , m_dockModel(dockModel)
        , m_windowEffects(windowEffects)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *object = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++m_refCount;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG next = --m_refCount;
        if (next == 0) {
            delete this;
        }
        return next;
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* dataObject, DWORD keyState, POINTL point, DWORD* effect) override
    {
        Q_UNUSED(keyState)
        m_dragAccepted = acceptsDataObject(dataObject);
        if (!effect || !m_dragAccepted) {
            if (effect) {
                *effect = DROPEFFECT_NONE;
            }
            setHover(false);
            return S_OK;
        }

        setHover(true);
        if (m_windowEffects && m_window) {
            m_windowEffects->setDockDropPointer(m_window, point.x, point.y);
        }
        *effect = DROPEFFECT_COPY;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD keyState, POINTL point, DWORD* effect) override
    {
        Q_UNUSED(keyState)
        if (!effect) {
            return E_POINTER;
        }

        if (!m_window || !m_dockModel || !m_dragAccepted) {
            *effect = DROPEFFECT_NONE;
            return S_OK;
        }

        setHover(true);
        if (m_windowEffects && m_window) {
            m_windowEffects->setDockDropPointer(m_window, point.x, point.y);
        }
        *effect = DROPEFFECT_COPY;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        setHover(false);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* dataObject, DWORD keyState, POINTL point, DWORD* effect) override
    {
        Q_UNUSED(keyState)
        setHover(false);

        if (!effect) {
            return E_POINTER;
        }

        *effect = DROPEFFECT_NONE;
        if (!m_window || !m_dockModel || !m_windowEffects) {
            return S_OK;
        }

        if (!m_dragAccepted && !acceptsDataObject(dataObject)) {
            return S_OK;
        }

        const QString path = pathFromDataObject(dataObject);
        if (path.isEmpty()) {
            m_dragAccepted = false;
            return S_OK;
        }

        const QString extension = path.section(QLatin1Char('.'), -1).toLower();
        if (extension != QLatin1String("lnk")
            && extension != QLatin1String("exe")
            && extension != QLatin1String("bat")
            && extension != QLatin1String("cmd")) {
            return S_OK;
        }

        m_windowEffects->notifyDockExternalDrop(m_window, path, 0);

        *effect = DROPEFFECT_COPY;
        m_dragAccepted = false;
        return S_OK;
    }

private:
    bool acceptsDataObject(IDataObject* dataObject) const
    {
        if (!dataObject) {
            return false;
        }

        FORMATETC format = {};
        format.cfFormat = CF_HDROP;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.tymed = TYMED_HGLOBAL;
        return SUCCEEDED(dataObject->QueryGetData(&format));
    }

    void setHover(bool active)
    {
        if (!m_windowEffects || !m_window) {
            return;
        }
        m_windowEffects->setDockDropHover(m_window, active);
    }

    QPointer<QWindow> m_window;
    DockModel* m_dockModel = nullptr;
    WindowEffects* m_windowEffects = nullptr;
    bool m_dragAccepted = false;
    ULONG m_refCount = 1;
};

} // namespace

DockDropTarget::DockDropTarget(DockModel* dockModel, WindowEffects* windowEffects, QObject* parent)
    : QObject(parent)
    , m_dockModel(dockModel)
    , m_windowEffects(windowEffects)
{
}

DockDropTarget::~DockDropTarget()
{
    unregisterDockWindow();
}

bool DockDropTarget::registerDockWindow(QWindow* window)
{
    unregisterDockWindow();
    if (!window || !window->winId()) {
        return false;
    }

    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        emit logMessage(QStringLiteral("Dock drop register failed: HWND is null."));
        return false;
    }

    RevokeDragDrop(hwnd);

    auto* target = new DropTargetImpl(window, m_dockModel, m_windowEffects);
    HRESULT hr = RegisterDragDrop(hwnd, target);
    if (hr == DRAGDROP_E_ALREADYREGISTERED) {
        RevokeDragDrop(hwnd);
        hr = RegisterDragDrop(hwnd, target);
    }
    if (FAILED(hr)) {
        target->Release();
        m_dropTarget = nullptr;
        emit logMessage(QStringLiteral("RegisterDragDrop failed: 0x%1").arg(QString::number(static_cast<quint32>(hr), 16)));
        return false;
    }

    m_dropTarget = target;
    m_window = window;
    emit logMessage(QStringLiteral("Dock OLE drop target registered."));
    return true;
}

void DockDropTarget::unregisterDockWindow()
{
    if (m_window && m_window->winId()) {
        RevokeDragDrop(reinterpret_cast<HWND>(m_window->winId()));
    }

    if (m_dropTarget) {
        m_dropTarget->Release();
        m_dropTarget = nullptr;
    }

    if (m_windowEffects && m_window) {
        m_windowEffects->setDockDropHover(m_window, false);
    }

    m_window.clear();
}
