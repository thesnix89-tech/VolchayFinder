#pragma once

#include <QObject>
#include <QCursor>
#include <QHash>
#include <QString>

class QWindow;

class MacCursor : public QObject
{
    Q_OBJECT

public:
    enum Shape {
        Arrow = 0,
        PointingHand,
        ClosedHand,
        Forbidden
    };
    Q_ENUM(Shape)

    explicit MacCursor(QObject* parent = nullptr);
    ~MacCursor() override;

    Q_INVOKABLE void apply(QWindow* window, Shape shape);
    Q_INVOKABLE void setArrow(QWindow* window);
    Q_INVOKABLE void setPointingHand(QWindow* window);
    Q_INVOKABLE void setClosedHand(QWindow* window);
    Q_INVOKABLE void setForbidden(QWindow* window);
    Q_INVOKABLE void installOnWindow(QWindow* window);

    bool installSystemCursors();
    void restoreSystemCursors();

private:
    QCursor cursorFor(Shape shape) const;
    QString ensureCursorOnDisk(const QString& scaleFolder, const QString& fileName, bool* ok = nullptr) const;
    bool applySystemCursorFromFile(const QString& diskPath, unsigned int ocrId) const;

    QHash<Shape, QCursor> m_cursors;
    QString m_scaleFolder;
    bool m_systemCursorsInstalled = false;
};
