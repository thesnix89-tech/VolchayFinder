#include "DockDropTarget.h"

#include "DockModel.h"
#include "WindowEffects.h"

#include <QDir>
#include <QGuiApplication>
#include <QWindow>

#include <oleidl.h>
#include <shlobj.h>
#include <shellapi.h>

namespace {

QSize iconPixelSize(HICON icon)
{
    if (!icon) {
        return QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    }

    ICONINFO iconInfo = {};
    if (!GetIconInfo(icon, &iconInfo)) {
        DestroyIcon(icon);
        return QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    }

    BITMAP bitmap = {};
    const int bitmapSize = GetObject(iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask,
                                     sizeof(bitmap),
                                     &bitmap);
    if (iconInfo.hbmColor) {
        DeleteObject(iconInfo.hbmColor);
    }
    if (iconInfo.hbmMask) {
        DeleteObject(iconInfo.hbmMask);
    }
    DestroyIcon(icon);

    if (bitmapSize == 0 || bitmap.bmWidth <= 0 || bitmap.bmHeight <= 0) {
        return QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    }

    return QSize(bitmap.bmWidth, bitmap.bmHeight);
}

QSize iconSizeForPath(const QString& path)
{
    if (path.isEmpty()) {
        return QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    }

    const QString nativePath = QDir::toNativeSeparators(path);
    SHFILEINFOW fileInfo = {};
    const DWORD_PTR result = SHGetFileInfoW(reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                                            0,
                                            &fileInfo,
                                            sizeof(fileInfo),
                                            SHGFI_ICON | SHGFI_LARGEICON);
    if (result == 0 || !fileInfo.hIcon) {
        return QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    }

    return iconPixelSize(fileInfo.hIcon);
}

bool readDragImageInfo(IDataObject* dataObject, QSize& imageSize, QPoint& hotspot)
{
    if (!dataObject) {
        return false;
    }

    static const UINT format = RegisterClipboardFormatW(L"DragImageBits");
    if (format == 0) {
        return false;
    }

    FORMATETC formatEtc = {};
    formatEtc.cfFormat = static_cast<CLIPFORMAT>(format);
    formatEtc.dwAspect = DVASPECT_CONTENT;
    formatEtc.lindex = -1;
    formatEtc.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(dataObject->GetData(&formatEtc, &medium)) || !medium.hGlobal) {
        return false;
    }

    const auto* dragImage = static_cast<const SHDRAGIMAGE*>(GlobalLock(medium.hGlobal));
    if (!dragImage || dragImage->sizeDragImage.cx <= 0 || dragImage->sizeDragImage.cy <= 0) {
        if (dragImage) {
            GlobalUnlock(medium.hGlobal);
        }
        ReleaseStgMedium(&medium);
        return false;
    }

    imageSize = QSize(dragImage->sizeDragImage.cx, dragImage->sizeDragImage.cy);
    hotspot = QPoint(dragImage->ptOffset.x, dragImage->ptOffset.y);
    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return true;
}

QPoint iconCenterFromCursor(const QPoint& cursor,
                            const QSize& imageSize,
                            const QPoint& hotspot,
                            bool hasDragImageInfo)
{
    if (hasDragImageInfo && imageSize.width() > 0 && imageSize.height() > 0) {
        return cursor - hotspot + QPoint(imageSize.width() / 2, imageSize.height() / 2);
    }

    // Shell fallback: drag image top-left follows the cursor.
    return cursor + QPoint(imageSize.width() / 2, imageSize.height() / 2);
}

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
        const QString path = pathFromDataObject(dataObject);
        if (m_windowEffects && m_window) {
            updateDragImageInfo(dataObject, path);
            m_windowEffects->setDockExternalDragPath(m_window, path);
            emitPointer(point);
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
        emitPointer(point);
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

        const QPoint cursor(point.x, point.y);
        const QPoint center = iconCenterFromCursor(cursor, m_dragImageSize, m_dragHotspot, m_hasDragImageInfo);
        if (m_windowEffects && m_window) {
            m_windowEffects->setDockDropPointer(m_window, cursor.x(), cursor.y(), center.x(), center.y());
        }

        const int dropIndex = m_windowEffects->dockDropIndexForIconCenter(m_window, center.x(), center.y());
        m_windowEffects->notifyDockExternalDrop(m_window, path, dropIndex);

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
        if (!active) {
            m_windowEffects->setDockExternalDragPath(m_window, QString());
        }
        m_windowEffects->setDockDropHover(m_window, active);
    }

    void updateDragImageInfo(IDataObject* dataObject, const QString& path)
    {
        m_dragPath = path;
        m_hasDragImageInfo = readDragImageInfo(dataObject, m_dragImageSize, m_dragHotspot);
        if (!m_hasDragImageInfo) {
            m_dragImageSize = iconSizeForPath(path);
            m_dragHotspot = QPoint(0, 0);
        }
    }

    void emitPointer(const POINTL& point)
    {
        if (!m_windowEffects || !m_window) {
            return;
        }

        const QPoint cursor(point.x, point.y);
        const QPoint center = iconCenterFromCursor(cursor, m_dragImageSize, m_dragHotspot, m_hasDragImageInfo);
        m_windowEffects->setDockDropPointer(m_window, cursor.x(), cursor.y(), center.x(), center.y());
    }

    QPointer<QWindow> m_window;
    DockModel* m_dockModel = nullptr;
    WindowEffects* m_windowEffects = nullptr;
    bool m_dragAccepted = false;
    QString m_dragPath;
    QSize m_dragImageSize;
    QPoint m_dragHotspot;
    bool m_hasDragImageInfo = false;
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
