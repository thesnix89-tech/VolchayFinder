#pragma once

#include <QObject>
#include <QRect>

class AppBarController : public QObject
{
    Q_OBJECT

public:
    explicit AppBarController(QObject* parent = nullptr);
    ~AppBarController() override;

    bool registerTopBar(void* hwnd, int height);
    void updateTopBarRect(void* hwnd, int height);
    void unregisterTopBar();

signals:
    void logMessage(const QString& message);

private:
    bool m_registered = false;
    void* m_hwnd = nullptr;
    unsigned int m_callbackMessage = 0;
};
