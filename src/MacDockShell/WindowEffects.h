#pragma once

#include <QObject>
#include <QRect>
#include <QHash>
#include <QVariant>
#include <QWindow>

#include <memory>

class QWindow;
class QAbstractNativeEventFilter;

class DockClickThroughState;

class WindowEffects : public QObject
{
    Q_OBJECT

public:
    explicit WindowEffects(QObject* parent = nullptr);
    ~WindowEffects() override;

    Q_INVOKABLE void applyDockGlass(QWindow* window);
    Q_INVOKABLE void enableHoverTracking(QWindow* window);
    Q_INVOKABLE void enableDockClickThrough(QWindow* window);
    // clipRegions → SetWindowRgn (draw / invisible wall). hitRegions → click targets only.
    Q_INVOKABLE void updateDockHitRegions(QWindow* window, const QVariantList& clipRegions,
                                          const QVariantList& hitRegions);
    Q_INVOKABLE void clearDockHitRegions(QWindow* window);
    Q_INVOKABLE void setDockDropHover(QWindow* window, bool active);
    Q_INVOKABLE void updateDockDropLayout(QWindow* window, qreal pillLocalLeft, int stride, int iconSize, int itemCount);
    Q_INVOKABLE int dockDropIndexForGlobalPoint(QWindow* window, int globalX, int globalY) const;

signals:
    void dockDropHoverChanged(QWindow* window, bool active);

private:
    struct DockDropLayout
    {
        qreal pillLocalLeft = 0;
        int stride = 0;
        int iconSize = 0;
        int itemCount = 0;
    };

    QHash<QWindow*, DockDropLayout> m_dropLayouts;
    QHash<QWindow*, bool> m_dropHoverActive;
    std::unique_ptr<QAbstractNativeEventFilter> m_topBarHoverFilter;
    std::unique_ptr<DockClickThroughState> m_dockClickThroughState;
    std::unique_ptr<QAbstractNativeEventFilter> m_dockHitTestFilter;
};
