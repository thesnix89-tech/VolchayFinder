#pragma once

#include <QObject>

#include <memory>

class QAbstractNativeEventFilter;

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
    void layoutChanged();

private slots:
    void handleShellLayoutChange();

private:
    void installNativeFilter(void* hwnd);
    void removeNativeFilter();

    bool m_registered = false;
    void* m_hwnd = nullptr;
    int m_topBarHeight = 0;
    unsigned int m_callbackMessage = 0;
    std::unique_ptr<QAbstractNativeEventFilter> m_nativeFilter;
};
