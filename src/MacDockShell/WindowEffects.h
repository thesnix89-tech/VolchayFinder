#pragma once

#include <QObject>
#include <QRect>
#include <QHash>
#include <QVariant>

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

private:
    std::unique_ptr<QAbstractNativeEventFilter> m_topBarHoverFilter;
    std::unique_ptr<DockClickThroughState> m_dockClickThroughState;
    std::unique_ptr<QAbstractNativeEventFilter> m_dockHitTestFilter;
};
