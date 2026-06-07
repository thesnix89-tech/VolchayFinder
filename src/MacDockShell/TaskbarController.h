#pragma once

#include <QObject>
#include <QString>

class TaskbarController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool taskbarHidden READ taskbarHidden NOTIFY taskbarHiddenChanged)

public:
    explicit TaskbarController(QObject* parent = nullptr);
    ~TaskbarController() override;

    Q_INVOKABLE bool hideTaskbar();
    Q_INVOKABLE bool showTaskbar();
    Q_INVOKABLE void restoreShell();
    Q_INVOKABLE void quitApplication();

    bool taskbarHidden() const;

signals:
    void taskbarHiddenChanged();
    void shellActionLogged(const QString& message);

private:
    bool setTaskbarVisible(bool visible);

    bool m_taskbarHidden = false;
};
