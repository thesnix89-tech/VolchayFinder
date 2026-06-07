#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QMessageLogContext>
#include <QFileInfo>
#include <QDirIterator>
#include <QQmlContext>
#include <QWindow>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>

#include "TaskbarController.h"
#include "DockModel.h"
#include "AppBarController.h"
#include "WindowEffects.h"
#include "MacCursor.h"

namespace {

QFile* gLogFile = nullptr;
QtMessageHandler gPreviousHandler = nullptr;

QString logFilePathFallback()
{
    return QStringLiteral("./MacDockShell.log");
}

QString logFilePathRuntime()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("MacDockShell.log");
}

void appendLine(const QString& line)
{
    if (!gLogFile) {
        return;
    }

    if (!gLogFile->isOpen()) {
        if (!gLogFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return;
        }
    }

    QTextStream out(gLogFile);
    out.setEncoding(QStringConverter::Utf8);
    out << line << '\n';
    out.flush();
    gLogFile->flush();
}

QString levelToString(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "CRITICAL";
    case QtFatalMsg: return "FATAL";
    }
    return "UNKNOWN";
}

void logMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString file = context.file ? QString::fromUtf8(context.file) : QString();
    const QString function = context.function ? QString::fromUtf8(context.function) : QString();
    const QString location = file.isEmpty()
        ? QString()
        : QString(" [%1:%2 %3]").arg(file).arg(context.line).arg(function);

    appendLine(QString("%1 [%2] %3%4")
               .arg(timestamp, levelToString(type), message, location));

    if (gPreviousHandler) {
        gPreviousHandler(type, context, message);
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

void logTopLevelWindows()
{
    const auto windows = QGuiApplication::topLevelWindows();
    appendLine(QString("Top-level Qt windows: %1").arg(windows.size()));
    for (QWindow* window : windows) {
        appendLine(QString("  title=%1 visible=%2 flags=%3 x=%4 y=%5 w=%6 h=%7")
            .arg(window->title())
            .arg(window->isVisible() ? "true" : "false")
            .arg(static_cast<qulonglong>(window->flags()))
            .arg(window->x())
            .arg(window->y())
            .arg(window->width())
            .arg(window->height()));
    }
}

void loadQml(QQmlApplicationEngine& engine, const QString& resourcePath)
{
    const QUrl url(QStringLiteral("qrc") + resourcePath);
    appendLine(QString("Loading QML entry: %1").arg(url.toString()));
    engine.load(url);
}

constexpr auto kSingleInstanceServer = "MacDockShell";
constexpr auto kLaunchShowSettings = "show-settings";
constexpr auto kLaunchAutostart = "autostart";

bool tryActivateExistingInstance(const QStringList& arguments)
{
    QLocalSocket socket;
    socket.connectToServer(QString::fromUtf8(kSingleInstanceServer));
    if (!socket.waitForConnected(300)) {
        return false;
    }

    const bool autostart = arguments.contains(QStringLiteral("--autostart"));
    socket.write(autostart ? kLaunchAutostart : kLaunchShowSettings);
    socket.waitForBytesWritten(300);
    return true;
}

bool listenForSecondaryLaunches(QLocalServer& server)
{
    const QString serverName = QString::fromUtf8(kSingleInstanceServer);
    if (server.listen(serverName)) {
        return true;
    }

    if (server.serverError() != QAbstractSocket::AddressInUseError) {
        return false;
    }

    QLocalServer::removeServer(serverName);
    return server.listen(serverName);
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication::setApplicationName("MacDockShell");
    QGuiApplication::setOrganizationName("ENI");

    QFile logFile(logFilePathFallback());
    gLogFile = &logFile;
    gPreviousHandler = qInstallMessageHandler(logMessageHandler);

    appendLine("============================================================");
    appendLine(QString("Session start: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    appendLine(QString("Log path: %1").arg(logFilePathFallback()));
    appendLine(QString("Current dir: %1").arg(QDir::currentPath()));
    appendLine("Creating QGuiApplication...");

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon());

    if (tryActivateExistingInstance(app.arguments())) {
        appendLine("Forwarded launch to an already running MacDockShell instance.");
        return 0;
    }

    QLocalServer singleInstanceServer;
    if (!listenForSecondaryLaunches(singleInstanceServer)) {
        appendLine(QString("Failed to start single-instance server: %1").arg(singleInstanceServer.errorString()));
    }

    TaskbarController taskbarController;
    DockModel dockModel;
    AppBarController appBarController;
    WindowEffects windowEffects;
    MacCursor macCursor;
    QTimer dockRefreshTimer;
    dockRefreshTimer.setInterval(1000);
    dockRefreshTimer.setSingleShot(false);

    QObject::connect(&dockRefreshTimer, &QTimer::timeout, &dockModel, &DockModel::refresh);
    dockRefreshTimer.start();

    QObject::connect(&dockModel, &DockModel::logMessage, [](const QString& message) {
        appendLine(QString("[DockModel] %1").arg(message));
    });
    QObject::connect(&taskbarController, &TaskbarController::shellActionLogged, [](const QString& message) {
        appendLine(QString("[TaskbarController] %1").arg(message));
    });
    QObject::connect(&appBarController, &AppBarController::logMessage, [](const QString& message) {
        appendLine(QString("[AppBarController] %1").arg(message));
    });
    QObject::connect(&singleInstanceServer, &QLocalServer::newConnection, &taskbarController, [&singleInstanceServer, &taskbarController]() {
        QLocalSocket* client = singleInstanceServer.nextPendingConnection();
        if (!client) {
            return;
        }

        QObject::connect(client, &QLocalSocket::readyRead, client, [client, &taskbarController]() {
            const QByteArray message = client->readAll();
            if (message == kLaunchAutostart) {
                taskbarController.tryAutostartShell();
            } else {
                taskbarController.setSettingsVisible(true);
            }
            client->disconnectFromServer();
        });
    });

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&taskbarController, &appBarController, &macCursor]() {
        appendLine("aboutToQuit fired. Restoring taskbar and unregistering appbar.");
        macCursor.restoreSystemCursors();
        appBarController.unregisterTopBar();
        taskbarController.restoreShell();
    });

    appendLine(QString("Runtime log path: %1").arg(logFilePathRuntime()));
    appendLine(QString("App dir: %1").arg(QCoreApplication::applicationDirPath()));
    appendLine(QString("Qt version: %1").arg(qVersion()));
    appendLine(QString("Application file path: %1").arg(QCoreApplication::applicationFilePath()));
    appendLine(QString("Library paths: %1").arg(QCoreApplication::libraryPaths().join("; ")));

    QQmlApplicationEngine topBarEngine;
    QQmlApplicationEngine dockEngine;
    QQmlApplicationEngine controlEngine;

    topBarEngine.rootContext()->setContextProperty("taskbarController", &taskbarController);
    topBarEngine.rootContext()->setContextProperty("dockModel", &dockModel);
    topBarEngine.rootContext()->setContextProperty("macCursor", &macCursor);
    dockEngine.rootContext()->setContextProperty("taskbarController", &taskbarController);
    dockEngine.rootContext()->setContextProperty("dockModel", &dockModel);
    dockEngine.rootContext()->setContextProperty("windowEffects", &windowEffects);
    dockEngine.rootContext()->setContextProperty("macCursor", &macCursor);
    controlEngine.rootContext()->setContextProperty("taskbarController", &taskbarController);
    controlEngine.rootContext()->setContextProperty("dockModel", &dockModel);
    controlEngine.rootContext()->setContextProperty("macCursor", &macCursor);

    auto connectWarnings = [&app](QQmlApplicationEngine& engine, const QString& name) {
        QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [name](const QList<QQmlError>& warnings) {
            appendLine(QString("%1 warnings count: %2").arg(name).arg(warnings.size()));
            for (const QQmlError& warning : warnings) {
                appendLine(QString("%1 warning: %2").arg(name, warning.toString()));
            }
        });
    };

    connectWarnings(topBarEngine, QStringLiteral("TopBarEngine"));
    connectWarnings(dockEngine, QStringLiteral("DockEngine"));
    connectWarnings(controlEngine, QStringLiteral("ControlEngine"));

    loadQml(topBarEngine, QStringLiteral(":/src/MacDockShell/qml/TopBarWindow.qml"));
    loadQml(dockEngine, QStringLiteral(":/src/MacDockShell/qml/DockWindow.qml"));
    loadQml(controlEngine, QStringLiteral(":/src/MacDockShell/qml/ControlWindow.qml"));

    if (topBarEngine.rootObjects().isEmpty() || dockEngine.rootObjects().isEmpty() || controlEngine.rootObjects().isEmpty()) {
        appendLine("One or more shell windows failed to load. Exiting with code -2.");
        return -2;
    }

    dockModel.refresh();

    auto installShellWindow = [&windowEffects](QQmlApplicationEngine& engine, bool applyGlass) {
        if (engine.rootObjects().isEmpty()) {
            return;
        }
        if (auto* shellWindow = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
            if (applyGlass) {
                windowEffects.applyDockGlass(shellWindow);
            }
        }
    };

    installShellWindow(topBarEngine, false);
    installShellWindow(dockEngine, true);
    installShellWindow(controlEngine, false);

    if (macCursor.installSystemCursors()) {
        appendLine("macOS Sierra system cursors installed.");
    } else {
        appendLine("Failed to install macOS Sierra system cursors.");
    }

    const bool launchedWithAutostart = QCoreApplication::arguments().contains(QStringLiteral("--autostart"));
    if (launchedWithAutostart) {
        appendLine("Launched with --autostart.");
        if (taskbarController.startWithWindows()) {
            taskbarController.setSettingsVisible(false);
            QTimer::singleShot(2000, &taskbarController, [&taskbarController]() {
                taskbarController.tryAutostartShell();
            });
        } else {
            appendLine("Autostart launch ignored because the setting is disabled.");
        }
    }

    QObject::connect(&taskbarController, &TaskbarController::shellActiveChanged, [&topBarEngine, &appBarController, &taskbarController]() {
        if (taskbarController.shellActive()) {
            if (!topBarEngine.rootObjects().isEmpty()) {
                QObject* root = topBarEngine.rootObjects().constFirst();
                if (auto* window = qobject_cast<QWindow*>(root)) {
                    appBarController.registerTopBar(reinterpret_cast<void*>(window->winId()), window->height());
                }
            }
        } else {
            appBarController.unregisterTopBar();
        }
    });

    logTopLevelWindows();
    appendLine("Entering app event loop...");

    const int code = app.exec();
    appendLine(QString("Application exited with code: %1").arg(code));
    appendLine("Session end.");

    qInstallMessageHandler(nullptr);
    gLogFile = nullptr;

    return code;
}
