#pragma once

#include <QObject>
#include <QPoint>
#include <QTimer>

class HoverTracker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPoint globalCursor READ globalCursor NOTIFY globalCursorChanged)

public:
    explicit HoverTracker(QObject* parent = nullptr);

    QPoint globalCursor() const { return m_globalCursor; }

signals:
    void globalCursorChanged();

private:
    void tick();

    QTimer m_timer;
    QPoint m_globalCursor;
};
