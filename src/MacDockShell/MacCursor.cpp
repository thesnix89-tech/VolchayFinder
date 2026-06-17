#include "MacCursor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// OCR_* may be omitted when WIN32_LEAN_AND_MEAN is set.
#ifndef OCR_NORMAL
#define OCR_NORMAL 32512
#define OCR_IBEAM 32513
#define OCR_WAIT 32514
#define OCR_CROSS 32515
#define OCR_SIZEWE 32644
#define OCR_SIZENS 32645
#define OCR_SIZENWSE 32642
#define OCR_SIZENESW 32643
#define OCR_SIZEALL 32646
#define OCR_NO 32648
#define OCR_HAND 32649
#define OCR_APPSTARTING 32650
#define OCR_HELP 32651
#endif
#endif

namespace {

QString scaleFolderForDpi(qreal devicePixelRatio)
{
    return devicePixelRatio >= 1.25 ? QStringLiteral("150") : QStringLiteral("100");
}

QString cursorFileName(MacCursor::Shape shape)
{
    switch (shape) {
    case MacCursor::Arrow:
        return QStringLiteral("arrow");
    case MacCursor::PointingHand:
        return QStringLiteral("pointing_hand");
    case MacCursor::ClosedHand:
        return QStringLiteral("closed_hand");
    case MacCursor::Forbidden:
        return QStringLiteral("forbidden");
    }
    return QStringLiteral("arrow");
}

QString cachedCursorPath(const QString& scaleFolder, const QString& fileName)
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/sierra_cursors/");
    return cacheRoot + scaleFolder + QLatin1Char('/') + fileName;
}

} // namespace

MacCursor::MacCursor(QObject* parent)
    : QObject(parent)
{
    qreal dpr = 1.0;
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        dpr = screen->devicePixelRatio();
    }

    m_scaleFolder = scaleFolderForDpi(dpr);
    const QList<Shape> shapes = { Arrow, PointingHand, ClosedHand, Forbidden };
    for (Shape shape : shapes) {
        const QString fileName = cursorFileName(shape);
        bool loaded = false;
        const QString diskPath = ensureCursorOnDisk(m_scaleFolder, fileName + QStringLiteral(".cur"), &loaded);
        if (loaded) {
            m_cursors.insert(shape, QCursor(diskPath));
        }
    }

    if (!m_cursors.contains(Arrow)) {
        m_cursors.insert(Arrow, QCursor(Qt::ArrowCursor));
    }
    if (!m_cursors.contains(PointingHand)) {
        m_cursors.insert(PointingHand, QCursor(Qt::PointingHandCursor));
    }
    if (!m_cursors.contains(ClosedHand)) {
        m_cursors.insert(ClosedHand, QCursor(Qt::ClosedHandCursor));
    }
    if (!m_cursors.contains(Forbidden)) {
        m_cursors.insert(Forbidden, QCursor(Qt::ForbiddenCursor));
    }
}

MacCursor::~MacCursor()
{
    restoreSystemCursors();
}

QString MacCursor::ensureCursorOnDisk(const QString& scaleFolder, const QString& fileName, bool* ok) const
{
    if (ok) {
        *ok = false;
    }

    const QString resourcePath = QStringLiteral(":/src/MacDockShell/cursors/%1/%2").arg(scaleFolder, fileName);

    QFile resourceFile(resourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray bytes = resourceFile.readAll();
    if (bytes.isEmpty()) {
        return {};
    }

    const QString diskPath = cachedCursorPath(scaleFolder, fileName);
    QDir().mkpath(QFileInfo(diskPath).absolutePath());

    QFile diskFile(diskPath);
    if (!diskFile.open(QIODevice::WriteOnly)) {
        return {};
    }
    diskFile.write(bytes);
    diskFile.close();

    if (ok) {
        *ok = true;
    }
    return diskPath;
}

bool MacCursor::applySystemCursorFromFile(const QString& diskPath, unsigned int ocrId) const
{
#ifdef Q_OS_WIN
    if (diskPath.isEmpty() || !QFile::exists(diskPath)) {
        return false;
    }

    const std::wstring nativePath = diskPath.toStdWString();
    HCURSOR loaded = static_cast<HCURSOR>(LoadImageW(nullptr,
                                                      nativePath.c_str(),
                                                      IMAGE_CURSOR,
                                                      0,
                                                      0,
                                                      LR_LOADFROMFILE | LR_DEFAULTSIZE));
    if (!loaded) {
        return false;
    }

    HCURSOR copy = CopyCursor(loaded);
    DestroyCursor(loaded);
    if (!copy) {
        return false;
    }

    if (!SetSystemCursor(copy, ocrId)) {
        DestroyCursor(copy);
        return false;
    }

    return true;
#else
    Q_UNUSED(diskPath);
    Q_UNUSED(ocrId);
    return false;
#endif
}

bool MacCursor::installSystemCursors()
{
#ifdef Q_OS_WIN
    if (m_systemCursorsInstalled) {
        return true;
    }

    struct CursorMapping {
        const char* fileName;
        unsigned int ocrId;
    };

    const CursorMapping mappings[] = {
        { "arrow.cur", OCR_NORMAL },
        { "pointing_hand.cur", OCR_HAND },
        { "closed_hand.cur", OCR_SIZEALL },
        { "forbidden.cur", OCR_NO },
        { "text.cur", OCR_IBEAM },
        { "horizontal_resize.cur", OCR_SIZEWE },
        { "vertical_resize.cur", OCR_SIZENS },
        { "diagonal_resize_1.cur", OCR_SIZENWSE },
        { "diagonal_resize_2.cur", OCR_SIZENESW },
        { "help.cur", OCR_HELP },
        { "precision.cur", OCR_CROSS },
        { "busy.ani", OCR_WAIT },
        { "working.ani", OCR_APPSTARTING },
    };

    int applied = 0;
    for (const CursorMapping& mapping : mappings) {
        bool loaded = false;
        const QString diskPath = ensureCursorOnDisk(m_scaleFolder, QString::fromUtf8(mapping.fileName), &loaded);
        if (!loaded) {
            continue;
        }
        if (applySystemCursorFromFile(diskPath, mapping.ocrId)) {
            ++applied;
        }
    }

    m_systemCursorsInstalled = applied > 0;
    return m_systemCursorsInstalled;
#else
    return false;
#endif
}

void MacCursor::restoreSystemCursors()
{
#ifdef Q_OS_WIN
    if (!m_systemCursorsInstalled) {
        return;
    }

    SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
    m_systemCursorsInstalled = false;
#endif
}

void MacCursor::apply(QWindow* window, Shape shape)
{
    if (!window) {
        return;
    }
    window->setCursor(cursorFor(shape));
}

void MacCursor::setArrow(QWindow* window)
{
    apply(window, Arrow);
}

void MacCursor::setPointingHand(QWindow* window)
{
    apply(window, PointingHand);
}

void MacCursor::setClosedHand(QWindow* window)
{
    apply(window, ClosedHand);
}

void MacCursor::setForbidden(QWindow* window)
{
    apply(window, Forbidden);
}

void MacCursor::installOnWindow(QWindow* window)
{
    if (!window) {
        return;
    }
    window->unsetCursor();
}

QCursor MacCursor::cursorFor(Shape shape) const
{
    return m_cursors.value(shape, m_cursors.value(Arrow));
}
