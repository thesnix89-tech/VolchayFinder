#pragma once

#include <QObject>

class QWindow;

class WindowEffects : public QObject
{
    Q_OBJECT

public:
    explicit WindowEffects(QObject* parent = nullptr);

    Q_INVOKABLE void applyDockGlass(QWindow* window);
};
