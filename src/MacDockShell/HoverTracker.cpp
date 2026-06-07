#include "HoverTracker.h"

#include <QCursor>

HoverTracker::HoverTracker(QObject* parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &HoverTracker::tick);
    m_timer.start(16);
    m_globalCursor = QCursor::pos();
}

void HoverTracker::tick()
{
    const QPoint next = QCursor::pos();
    if (next == m_globalCursor) {
        return;
    }

    m_globalCursor = next;
    emit globalCursorChanged();
}
