// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application.h"
#include "cutefishapplication.h"
#include "mpvitem.h"
#include "playersettings.h"
#include "playlist.h"
#include "preferences.h"
#include "resumestore.h"
#include "sleepinhibitor.h"
#include "thumbnailer.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTranslator>

int cutefish_application_main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    // mpv renders through OpenGL only.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication &app = *qGuiApp;
    app.setApplicationName(QStringLiteral("cutefish-video-player"));
    app.setApplicationVersion(QStringLiteral(VIDEO_PLAYER_VERSION));
    app.setDesktopFileName(QStringLiteral("cutefish-video-player"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("multimedia-video-player")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Cutefish Video Player"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Files or URLs to play."), QStringLiteral("[files...]"));
    parser.process(app);

    QStringList arguments;
    for (const QString &argument : parser.positionalArguments())
        arguments.append(QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile).toString());

    const QString translation = QStringLiteral("/usr/share/cutefish-video-player/translations/%1.qm").arg(QLocale().name());
    if (QFile::exists(translation)) {
        auto *translator = new QTranslator(&app);
        if (translator->load(translation))
            QGuiApplication::installTranslator(translator);
    }

    const char *uri = "Cutefish.VideoPlayer";
    qmlRegisterType<MpvItem>(uri, 1, 0, "MpvItem");
    qmlRegisterUncreatableType<Playlist>(uri, 1, 0, "Playlist", QStringLiteral("Provided by App"));
    qmlRegisterUncreatableType<PlayerSettings>(uri, 1, 0, "PlayerSettings", QStringLiteral("Provided by App"));
    qmlRegisterUncreatableType<ResumeStore>(uri, 1, 0, "ResumeStore", QStringLiteral("Provided by App"));
    qmlRegisterUncreatableType<SleepInhibitor>(uri, 1, 0, "SleepInhibitor", QStringLiteral("Provided by App"));
    qmlRegisterUncreatableType<Thumbnailer>(uri, 1, 0, "Thumbnailer", QStringLiteral("Provided by App"));

    // The object goes up before the name, so a second launch that finds the
    // name always finds someone to hand its files to.
    Application application(Preferences::ownFile());
    auto bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("com.cutefish.VideoPlayer");
    const QString path = QStringLiteral("/VideoPlayer");
    if (bus.isConnected()) {
        bus.registerObject(path, &application, QDBusConnection::ExportAllSlots);
        if (!bus.registerService(service)) {
            QDBusMessage call = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("open"));
            call << arguments;
            // A player that does not answer gets a window of our own instead.
            if (bus.call(call, QDBus::Block, 3000).type() == QDBusMessage::ReplyMessage)
                return 0;
            bus.unregisterObject(path);
        }
    }

    for (const QString &argument : std::as_const(arguments)) {
        const QUrl url(argument);
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile() && !Application::isSubtitle(url)) {
            application.setLaunchVideoSize(MpvItem::probeVideoSize(url, 1000));
            break;
        }
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider(application.thumbnailer()));
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &application);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    application.watchRatio(window);
    QObject::connect(&application, &Application::activationRequested, window, [window] {
        if (window->visibility() == QWindow::Minimized)
            window->showNormal();
        window->raise();
        window->requestActivate();
    });
    if (!arguments.isEmpty())
        application.open(arguments);
    return app.exec();
}
