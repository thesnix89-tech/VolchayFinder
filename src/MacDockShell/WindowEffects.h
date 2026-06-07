#pragma once

#include <QObject>

#include <memory>

class QWindow;
class QAbstractNativeEventFilter;

class WindowEffects : public QObject
{
    Q_OBJECT

public:
    explicit WindowEffects(QObject* parent = nullptr);
    ~WindowEffects() override;

    Q_INVOKABLE void applyDockGlass(QWindow* window);
    Q_INVOKABLE void enableHoverTracking(QWindow* window);

private:
    std::unique_ptr<QAbstractNativeEventFilter> m_topBarHoverFilter;
};
