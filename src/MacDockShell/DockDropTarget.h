#pragma once

#include <QObject>
#include <QPointer>

class QWindow;
class DockModel;
class WindowEffects;
struct IDropTarget;

class DockDropTarget : public QObject
{
    Q_OBJECT

public:
    explicit DockDropTarget(DockModel* dockModel, WindowEffects* windowEffects, QObject* parent = nullptr);
    ~DockDropTarget() override;

    Q_INVOKABLE bool registerDockWindow(QWindow* window);
    void unregisterDockWindow();

signals:
    void logMessage(const QString& message);

private:
    IDropTarget* m_dropTarget = nullptr;
    QPointer<QWindow> m_window;
    DockModel* m_dockModel = nullptr;
    WindowEffects* m_windowEffects = nullptr;
};
