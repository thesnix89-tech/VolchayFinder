#pragma once

#include <QObject>
#include <QRect>

#include <memory>

class QAbstractNativeEventFilter;

class AppBarController : public QObject
{
    Q_OBJECT

public:
    explicit AppBarController(QObject* parent = nullptr);
    ~AppBarController() override;

    bool registerTopBar(void* hwnd, int height);
    bool updateTopBarRect(void* hwnd, int height);
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
    // Last rect we actually pushed via ABM_SETPOS. ABM_SETPOS makes the shell
    // broadcast ABN_POSCHANGED back to every appbar (including us), which would
    // re-enter updateTopBarRect and set the same position again — a self-feeding
    // loop of synchronous SHAppBarMessage/SetWindowPos calls that stalls the GUI
    // thread. Skipping the work when the rect is unchanged breaks the loop.
    QRect m_lastAppliedRect;
    bool m_hasAppliedRect = false;
    unsigned int m_callbackMessage = 0;
    std::unique_ptr<QAbstractNativeEventFilter> m_nativeFilter;
};
