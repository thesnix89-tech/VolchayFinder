#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFont>
#include <QFontDatabase>
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
#include <QThread>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QTranslator>

#include <objbase.h>
#include <winrt/base.h>

#include "TaskbarController.h"
#include "DockModel.h"
#include "AppBarController.h"
#include "WindowEffects.h"
#include "HoverTracker.h"
#include "MacCursor.h"
#include "DockDropTarget.h"
#include "ShelfController.h"
#include "TrayIconModel.h"
#include "SpotlightModel.h"
#include "NotificationCenterModel.h"

namespace {

QFile* gLogFile = nullptr;
QtMessageHandler gPreviousHandler = nullptr;
QTranslator gAppTranslator;

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

bool installAppLanguage(const QString& code)
{
    QGuiApplication::removeTranslator(&gAppTranslator);
    QLocale::setDefault(QLocale(code));

    const QString resourcePath = QStringLiteral(":/i18n/MacDockShell_%1.qm").arg(code);
    if (!gAppTranslator.load(resourcePath)) {
        appendLine(QString("Failed to load translation: %1").arg(resourcePath));
        return false;
    }

    QGuiApplication::installTranslator(&gAppTranslator);
    appendLine(QString("Installed UI language: %1").arg(code));
    return true;
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

bool loadBundledFonts()
{
    const QStringList fontPaths = {
        QStringLiteral(":/src/MacDockShell/fonts/SF-Pro-Text-Regular.otf"),
        QStringLiteral(":/src/MacDockShell/fonts/SF-Pro-Text-Medium.otf"),
        QStringLiteral(":/src/MacDockShell/fonts/SF-Pro-Text-Semibold.otf"),
        QStringLiteral(":/src/MacDockShell/fonts/SF-Pro-Text-Bold.otf"),
    };

    QString family;
    for (const QString& path : fontPaths) {
        const int fontId = QFontDatabase::addApplicationFont(path);
        if (fontId < 0) {
            appendLine(QString("Failed to load font: %1").arg(path));
            continue;
        }

        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty() && family.isEmpty()) {
            family = families.first();
        }
    }

    if (family.isEmpty()) {
        appendLine("SF Pro Text not loaded; using system default font.");
        return false;
    }

    QFont appFont(family);
    appFont.setPixelSize(13);
    QGuiApplication::setFont(appFont);
    appendLine(QString("Default font set to: %1").arg(family));
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
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        appendLine("WinRT apartment initialized.");
    } catch (const winrt::hresult_error& error) {
        appendLine(QString("WinRT apartment initialization failed: 0x%1 %2")
            .arg(static_cast<uint32_t>(error.code()), 8, 16, QLatin1Char('0'))
            .arg(QString::fromWCharArray(error.message().c_str())));
    }
    OleInitialize(nullptr);
    loadBundledFonts();

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
    appBarController.setResizeWindowToAppBarRect(false);
    WindowEffects windowEffects;
    HoverTracker hoverTracker;
    MacCursor macCursor;
    DockDropTarget dockDropTarget(&dockModel, &windowEffects);
    ShelfController shelfController;
    TrayIconModel trayIconModel;
    SpotlightModel spotlightModel;
    NotificationCenterModel notificationCenterModel;
    QTimer dockRefreshTimer;
    dockRefreshTimer.setInterval(1000);
    dockRefreshTimer.setSingleShot(false);

    QObject::connect(&dockRefreshTimer, &QTimer::timeout, &dockModel, &DockModel::refresh);
    dockRefreshTimer.start();

    QObject::connect(&dockModel, &DockModel::logMessage, [](const QString& message) {
        appendLine(QString("[DockModel] %1").arg(message));
    });
    QObject::connect(&shelfController, &ShelfController::logMessage, [](const QString& message) {
        appendLine(QString("[ShelfController] %1").arg(message));
    });
    QObject::connect(&shelfController, &ShelfController::recycleBinChanged, &dockModel, &DockModel::refresh);
    QObject::connect(&trayIconModel, &TrayIconModel::logMessage, [](const QString& message) {
        appendLine(QString("[TrayIconModel] %1").arg(message));
    });
    QObject::connect(&spotlightModel, &SpotlightModel::logMessage, [](const QString& message) {
        appendLine(QString("[SpotlightModel] %1").arg(message));
    });
    QObject::connect(&notificationCenterModel, &NotificationCenterModel::logMessage, [](const QString& message) {
        appendLine(QString("[NotificationCenterModel] %1").arg(message));
    });
    QObject::connect(&taskbarController, &TaskbarController::showMenuBarExtrasChanged, &trayIconModel, [&trayIconModel, &taskbarController]() {
        trayIconModel.setEnabled(taskbarController.showMenuBarExtras()
                                 && taskbarController.shellActive()
                                 && taskbarController.showTopBar());
    });
    QObject::connect(&taskbarController, &TaskbarController::trayExtrasRefreshMsChanged, &trayIconModel, [&trayIconModel, &taskbarController]() {
        trayIconModel.setRefreshIntervalMs(taskbarController.trayExtrasRefreshMs());
    });
    QObject::connect(&taskbarController, &TaskbarController::blackWhiteTrayIconsChanged, &trayIconModel, [&trayIconModel, &taskbarController]() {
        trayIconModel.setBlackWhiteIcons(taskbarController.blackWhiteTrayIcons());
    });
    QObject::connect(&taskbarController, &TaskbarController::shellActiveChanged, &trayIconModel, [&trayIconModel, &taskbarController]() {
        trayIconModel.setEnabled(taskbarController.showMenuBarExtras()
                                 && taskbarController.shellActive()
                                 && taskbarController.showTopBar());
    });
    QObject::connect(&taskbarController, &TaskbarController::showTopBarChanged, &trayIconModel, [&trayIconModel, &taskbarController]() {
        trayIconModel.setEnabled(taskbarController.showMenuBarExtras()
                                 && taskbarController.shellActive()
                                 && taskbarController.showTopBar());
    });
    trayIconModel.setRefreshIntervalMs(taskbarController.trayExtrasRefreshMs());
    trayIconModel.setBlackWhiteIcons(taskbarController.blackWhiteTrayIcons());
    trayIconModel.setTrayOnScreenScope([&taskbarController](const std::function<void()>& action) {
        taskbarController.withTrayOnScreenOnGuiThread(action);
    });
    trayIconModel.setGuiInvoker([&taskbarController](const std::function<void()>& action) {
        if (QThread::currentThread() == taskbarController.thread()) {
            action();
            return;
        }

        const std::function<void()> actionCopy = action;
        QMetaObject::invokeMethod(&taskbarController, [actionCopy]() {
            actionCopy();
        }, Qt::BlockingQueuedConnection);
    });
    trayIconModel.setTrayUiaBusyScope([&taskbarController](bool busy) {
        taskbarController.setTrayUiaBusy(busy);
    });
    QObject::connect(&taskbarController, &TaskbarController::shellActiveChanged, &trayIconModel, [&trayIconModel, &taskbarController]() {
        if (taskbarController.shellActive()) {
            trayIconModel.resetOverflowBootstrap();
        }
    });
    trayIconModel.setEnabled(taskbarController.showMenuBarExtras()
                             && taskbarController.shellActive()
                             && taskbarController.showTopBar());
    QObject::connect(&taskbarController, &TaskbarController::explorerIconStyleChanged, &dockModel, [&dockModel, &taskbarController]() {
        dockModel.setExplorerIconStyle(taskbarController.explorerIconStyle());
    });
    dockModel.setExplorerIconStyle(taskbarController.explorerIconStyle());
    QObject::connect(&taskbarController, &TaskbarController::trashIconStyleChanged, &dockModel, [&dockModel, &taskbarController]() {
        dockModel.setTrashIconStyle(taskbarController.trashIconStyle());
    });
    dockModel.setTrashIconStyle(taskbarController.trashIconStyle());
    QObject::connect(&taskbarController, &TaskbarController::dockSeparateTransientAppsChanged, &dockModel, [&dockModel, &taskbarController]() {
        dockModel.setSeparateTransientApps(taskbarController.dockSeparateTransientApps());
    });
    dockModel.setSeparateTransientApps(taskbarController.dockSeparateTransientApps());
    QObject::connect(&taskbarController, &TaskbarController::showDownloadsInDockChanged, &dockModel, [&dockModel, &taskbarController]() {
        dockModel.setShowDownloadsInDock(taskbarController.showDownloadsInDock());
    });
    dockModel.setShowDownloadsInDock(taskbarController.showDownloadsInDock());
    QObject::connect(&dockModel, &DockModel::unpinDownloadsFromDock, &taskbarController, [&taskbarController]() {
        taskbarController.setShowDownloadsInDock(false);
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
    topBarEngine.rootContext()->setContextProperty("windowEffects", &windowEffects);
    topBarEngine.rootContext()->setContextProperty("macCursor", &macCursor);
    topBarEngine.rootContext()->setContextProperty("hoverTracker", &hoverTracker);
    topBarEngine.rootContext()->setContextProperty("trayIconModel", &trayIconModel);
    topBarEngine.rootContext()->setContextProperty("spotlightModel", &spotlightModel);
    topBarEngine.rootContext()->setContextProperty("notificationCenterModel", &notificationCenterModel);
    dockEngine.rootContext()->setContextProperty("taskbarController", &taskbarController);
    dockEngine.rootContext()->setContextProperty("dockModel", &dockModel);
    dockEngine.rootContext()->setContextProperty("windowEffects", &windowEffects);
    dockEngine.rootContext()->setContextProperty("dockDropTarget", &dockDropTarget);
    dockEngine.rootContext()->setContextProperty("macCursor", &macCursor);
    dockEngine.rootContext()->setContextProperty("shelfController", &shelfController);
    QObject::connect(&dockDropTarget, &DockDropTarget::logMessage, [](const QString& message) {
        appendLine(QString("[DockDropTarget] %1").arg(message));
    });
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

    installAppLanguage(taskbarController.effectiveLanguage());

    QObject::connect(&taskbarController, &TaskbarController::languageChanged, &app, [&]() {
        installAppLanguage(taskbarController.effectiveLanguage());
        topBarEngine.retranslate();
        dockEngine.retranslate();
        controlEngine.retranslate();
        dockModel.refresh();
        taskbarController.refreshMenuBar();
    });

    loadQml(topBarEngine, QStringLiteral(":/src/MacDockShell/qml/TopBarWindow.qml"));
    loadQml(dockEngine, QStringLiteral(":/src/MacDockShell/qml/DockWindow.qml"));
    loadQml(controlEngine, QStringLiteral(":/src/MacDockShell/qml/ControlWindow.qml"));

    if (topBarEngine.rootObjects().isEmpty() || dockEngine.rootObjects().isEmpty() || controlEngine.rootObjects().isEmpty()) {
        appendLine("One or more shell windows failed to load. Exiting with code -2.");
        return -2;
    }

    if (!dockEngine.rootObjects().isEmpty()) {
        if (auto* dockWindow = qobject_cast<QWindow*>(dockEngine.rootObjects().constFirst())) {
            if (dockDropTarget.registerDockWindow(dockWindow)) {
                appendLine("Dock OLE drop target registered.");
            } else {
                appendLine("Failed to register dock OLE drop target.");
            }
        }
    }

    dockModel.refresh();

    auto installShellWindow = [&windowEffects](QQmlApplicationEngine& engine, bool applyGlass, bool enableHover = false, bool enableTopBarClickThrough = false) {
        if (engine.rootObjects().isEmpty()) {
            return;
        }
        if (auto* shellWindow = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
            if (applyGlass) {
                windowEffects.applyDockGlass(shellWindow);
            }
            if (enableHover) {
                windowEffects.enableHoverTracking(shellWindow);
            }
            if (enableTopBarClickThrough) {
                windowEffects.enableTopBarClickThrough(shellWindow);
            }
        }
    };

    installShellWindow(topBarEngine, false, true, true);
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
                    const int topBarHeight = root->property("barHeight").toInt();
                    appBarController.registerTopBar(reinterpret_cast<void*>(window->winId()),
                                                    topBarHeight > 0 ? topBarHeight : 26);
                }
            }
        } else {
            appBarController.unregisterTopBar();
        }
    });

    QObject::connect(&appBarController, &AppBarController::layoutChanged, &taskbarController, [&taskbarController]() {
        taskbarController.enforceTaskbarHidden();
    });
    QObject::connect(&taskbarController, &TaskbarController::shellLayoutRestoreNeeded, [&topBarEngine, &appBarController]() {
        if (topBarEngine.rootObjects().isEmpty()) {
            return;
        }
        if (auto* window = qobject_cast<QWindow*>(topBarEngine.rootObjects().constFirst())) {
            QObject* root = topBarEngine.rootObjects().constFirst();
            const int topBarHeight = root->property("barHeight").toInt();
            appBarController.updateTopBarRect(reinterpret_cast<void*>(window->winId()),
                                              topBarHeight > 0 ? topBarHeight : 26);
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
