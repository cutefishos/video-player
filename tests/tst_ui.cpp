// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application.h"
#include "mpvitem.h"
#include "playersettings.h"
#include "playlist.h"
#include "resumestore.h"
#include "sleepinhibitor.h"
#include "thumbnailer.h"

#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

static QStringList qmlWarnings;
static QtMessageHandler previousHandler = nullptr;

static void captureWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.contains(QLatin1String("qrc:/qml/")) || message.contains(QLatin1String("Binding loop"))
            || message.contains(QLatin1String("recursive rearrange")))
        qmlWarnings.append(message);
    if (previousHandler)
        previousHandler(type, context, message);
}

static QRectF sceneRect(QQuickItem *item)
{
    return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
}

class UiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void emptyWindow_data();
    void emptyWindow();
    void playbackLayout();
    void replayAtEnd();

private:
    QQuickWindow *load(QQmlApplicationEngine &engine, Application &application, bool dark);
    QString makeClip();

    QTemporaryDir m_dir;
};

void UiTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    previousHandler = qInstallMessageHandler(captureWarnings);
    const char *uri = "Cutefish.VideoPlayer";
    qmlRegisterType<MpvItem>(uri, 1, 0, "MpvItem");
    qmlRegisterUncreatableType<Playlist>(uri, 1, 0, "Playlist", QString());
    qmlRegisterUncreatableType<PlayerSettings>(uri, 1, 0, "PlayerSettings", QString());
    qmlRegisterUncreatableType<ResumeStore>(uri, 1, 0, "ResumeStore", QString());
    qmlRegisterUncreatableType<SleepInhibitor>(uri, 1, 0, "SleepInhibitor", QString());
    qmlRegisterUncreatableType<Thumbnailer>(uri, 1, 0, "Thumbnailer", QString());
}

void UiTest::cleanupTestCase()
{
    qInstallMessageHandler(previousHandler);
}

QQuickWindow *UiTest::load(QQmlApplicationEngine &engine, Application &application, bool dark)
{
    engine.addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider(application.thumbnailer()));
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &application);
    QQmlComponent theme(&engine);
    theme.setData(QByteArray("import QtQuick\nimport FishUI as FishUI\nQtObject { Component.onCompleted: FishUI.Theme.darkMode = ")
                  + (dark ? "true" : "false") + " }", QUrl());
    std::unique_ptr<QObject> themeOverride(theme.create());
    if (!themeOverride)
        qWarning() << theme.errorString();
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return nullptr;
    return qobject_cast<QQuickWindow *>(engine.rootObjects().first());
}

QString UiTest::makeClip()
{
    const QString clip = m_dir.filePath(QStringLiteral("Sample Clip.mkv"));
    if (QFile::exists(clip))
        return clip;
    QProcess process;
    process.start(QStringLiteral("ffmpeg"), {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-y"),
                                             QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                                             QStringLiteral("testsrc=size=640x360:rate=25:duration=20"),
                                             QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), clip});
    if (!process.waitForFinished(60000) || process.exitCode() != 0)
        return {};
    return clip;
}

void UiTest::emptyWindow_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("light") << false;
    QTest::newRow("dark") << true;
}

void UiTest::emptyWindow()
{
    QFETCH(bool, dark);
    Application application(m_dir.filePath(QStringLiteral("empty.conf")));
    QQmlApplicationEngine engine;
    qmlWarnings.clear();
    QQuickWindow *window = load(engine, application, dark);
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.join(QLatin1Char('\n'))));

    auto empty = window->findChild<QQuickItem *>(QStringLiteral("emptyPage"));
    QVERIFY(empty && empty->isVisible());
    auto bar = window->findChild<QQuickItem *>(QStringLiteral("controlBar"));
    QVERIFY(bar && !bar->isVisible());

    // The welcome column sits centred under the header at the smallest size.
    for (const QSize &size : {QSize(960, 600), QSize(480, 320)}) {
        window->resize(size);
        QTest::qWait(50);
        auto button = window->findChild<QQuickItem *>(QStringLiteral("openFileButton"));
        QVERIFY(button);
        const QRectF rect = sceneRect(button);
        QVERIFY2(rect.top() >= 44 && rect.bottom() <= size.height(), qPrintable(QStringLiteral("%1 %2").arg(rect.top()).arg(rect.bottom())));
    }

    // Menus are made on first use, without warnings.
    for (const char *name : {"mainMenu", "contextMenu", "playlistMenu"}) {
        QVariant menu;
        QVERIFY(QMetaObject::invokeMethod(window, "menu", Q_RETURN_ARG(QVariant, menu), Q_ARG(QVariant, QString::fromLatin1(name))));
        QVERIFY2(menu.value<QObject *>(), name);
    }

    // Dialogs are made on first use, and open without warnings.
    QVERIFY(!window->findChild<QWindow *>(QStringLiteral("preferencesDialog")));
    for (const char *name : {"preferences", "shortcuts", "openUrl", "about"}) {
        QVERIFY(QMetaObject::invokeMethod(window, "openDialog", Q_ARG(QVariant, QString::fromLatin1(name))));
        QVariant dialog;
        QVERIFY(QMetaObject::invokeMethod(window, "dialog", Q_RETURN_ARG(QVariant, dialog), Q_ARG(QVariant, QString::fromLatin1(name))));
        auto *dialogWindow = qobject_cast<QQuickWindow *>(dialog.value<QObject *>());
        QVERIFY2(dialogWindow && dialogWindow->isVisible(), name);
        QVERIFY(QTest::qWaitForWindowExposed(dialogWindow));
        if (QByteArray(name) == "openUrl") {
            // Typing goes to the field, Tab walks to the buttons, Return opens.
            auto field = dialogWindow->findChild<QQuickItem *>(QStringLiteral("urlField"));
            QVERIFY(field);
            QTRY_VERIFY(field->hasActiveFocus());
            field->setProperty("text", QStringLiteral("https://example.com/a.mp4"));
            QTest::keyClick(dialogWindow, Qt::Key_Tab);
            QVERIFY(!field->hasActiveFocus());
            QSignalSpy opened(&application, &Application::openRequested);
            field->forceActiveFocus();
            QTest::keyClick(dialogWindow, Qt::Key_Return);
            QCOMPARE(opened.count(), 1);
            QTRY_VERIFY(!dialogWindow->isVisible());
            continue;
        }
        // Escape closes every dialog.
        QTest::keyClick(dialogWindow, Qt::Key_Escape);
        QTRY_VERIFY2(!dialogWindow->isVisible(), name);
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.join(QLatin1Char('\n'))));
}

void UiTest::playbackLayout()
{
    if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
        QSKIP("ffmpeg is needed to make the test clip");
    const QString clip = makeClip();
    QVERIFY(!clip.isEmpty());

    Application application(m_dir.filePath(QStringLiteral("playback.conf")));
    QQmlApplicationEngine engine;
    qmlWarnings.clear();
    QQuickWindow *window = load(engine, application, false);
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto player = window->findChild<MpvItem *>(QStringLiteral("player"));
    QVERIFY(player);
    application.openUrls({QUrl::fromLocalFile(clip)});
    QTRY_VERIFY_WITH_TIMEOUT(player->loaded(), 10000);
    QCOMPARE(window->title(), QStringLiteral("Sample Clip"));
    QCOMPARE(application.settings()->recentFiles().size(), 1);

    // The window first fits the 640x360 picture, within 3/4 of the screen.
    QTRY_VERIFY_WITH_TIMEOUT(window->width() != 960, 5000);
    const QSize screen = window->screen()->availableSize();
    // One device pixel per video pixel, never under the minimum size.
    const qreal fitRatio = window->effectiveDevicePixelRatio();
    const int fitWidth = qMax(480, qMin(qRound(640 / fitRatio), screen.width() * 3 / 4));
    QVERIFY2(qAbs(window->width() - fitWidth) <= 1, qPrintable(QString::number(window->width())));
    if (window->width() > 480)
        QVERIFY(qAbs(window->height() - window->width() * 9.0 / 16) <= 1);

    auto bar = window->findChild<QQuickItem *>(QStringLiteral("controlBar"));
    QVERIFY(bar);
    QTRY_VERIFY(bar->isVisible());
    // The software renderer has no blur; the plate is then drawn solid.
    auto glass = bar->findChild<QQuickItem *>(QStringLiteral("controlGlass"));
    QVERIFY(glass);
    QCOMPARE(glass->property("blurred").toBool(),
             window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software);

    for (const QSize &size : {QSize(1280, 800), QSize(801, 501), QSize(480, 320)}) {
        window->resize(size);
        QTest::qWait(80);
        const QRectF plate = sceneRect(bar);
        QVERIFY(plate.left() >= 12 - 0.01 && plate.right() <= size.width() - 12 + 0.01);
        const qreal ratio = window->effectiveDevicePixelRatio();
        // A fixed-width panel, centred to the device pixel.
        QCOMPARE(plate.width(), qMin(460.0, size.width() - 24.0));
        QVERIFY2(qAbs(plate.center().x() - size.width() / 2.0) * ratio <= 1,
                 qPrintable(QString::number(plate.center().x() - size.width() / 2.0)));

        // Glyphs sit in the middle of their buttons.
        for (QQuickItem *button : bar->findChildren<QQuickItem *>()) {
            if (!button->objectName().endsWith(QLatin1String("Button")) || !button->isVisible())
                continue;
            QQuickItem *glyph = nullptr;
            for (QQuickItem *child : button->findChildren<QQuickItem *>()) {
                if (QByteArray(child->metaObject()->className()).contains("IconItem"))
                    glyph = child;
            }
            QVERIFY(glyph);
            QVERIFY2((sceneRect(glyph).center() - sceneRect(button).center()).manhattanLength() * ratio <= 1,
                     qPrintable(button->objectName()));
        }

        // The times line up with the outer glyphs of the row above.
        auto glyphOf = [&](const char *name) {
            auto button = bar->findChild<QQuickItem *>(QLatin1String(name));
            for (QQuickItem *child : button->findChildren<QQuickItem *>()) {
                if (QByteArray(child->metaObject()->className()).contains("IconItem"))
                    return sceneRect(child);
            }
            return QRectF();
        };
        auto elapsed = bar->findChild<QQuickItem *>(QStringLiteral("elapsedLabel"));
        auto total = bar->findChild<QQuickItem *>(QStringLiteral("totalLabel"));
        QVERIFY(qAbs(sceneRect(elapsed).left() - glyphOf("muteButton").left()) * ratio <= 1);
        QVERIFY(qAbs(sceneRect(total).right() - glyphOf("fullscreenButton").right()) * ratio <= 1);

        // No two visible controls overlap, and all stay on the plate.
        QList<QRectF> rects;
        const auto items = bar->findChildren<QQuickItem *>();
        for (QQuickItem *item : items) {
            const QString name = item->objectName();
            if (!name.endsWith(QLatin1String("Button")) && name != QLatin1String("volumeSlider"))
                continue;
            if (!item->isVisible())
                continue;
            const QRectF rect = sceneRect(item);
            QVERIFY2(plate.contains(rect), qPrintable(name));
            for (const QRectF &other : std::as_const(rects))
                QVERIFY2(!rect.intersects(other), qPrintable(QStringLiteral("%1 overlaps at %2").arg(name).arg(size.width())));
            rects.append(rect);
        }

        // Wide enough, the play button sits in the middle of the plate.
        auto play = window->findChild<QQuickItem *>(QStringLiteral("playButton"));
        QVERIFY2(qAbs(sceneRect(play).center().x() - plate.center().x()) < 1,
                     qPrintable(QString::number(sceneRect(play).center().x() - plate.center().x())));
        qInfo() << size << "plate" << plate << "play" << sceneRect(play);
    }

    // Playing with the pointer still hides the controls and the pointer.
    window->resize(960, 600);
    player->setPaused(false);
    QTRY_VERIFY_WITH_TIMEOUT(!player->paused(), 3000);
    QTest::mouseMove(window, QPoint(480, 250));
    QVERIFY(window->property("controlsVisible").toBool());
    QTRY_COMPARE_WITH_TIMEOUT(bar->opacity(), 0.0, 4000);
    QTRY_COMPARE(window->cursor().shape(), Qt::BlankCursor);

    // The playlist opens over the right side, clear of the control bar.
    window->setProperty("pointerActive", true);
    window->setProperty("panelOpen", true);
    auto panel = window->findChild<QQuickItem *>(QStringLiteral("sidePanel"));
    QTRY_VERIFY(panel && panel->isVisible());
    QCOMPARE(qRound(sceneRect(panel).right()), 960 - 12);
    QVERIFY(!sceneRect(panel).intersects(sceneRect(bar)));
    for (int tab = 0; tab < 3; ++tab) {
        window->setProperty("panelTab", tab);
        QTest::qWait(30);
    }

    // Full screen shows no window buttons, the main menu included.
    window->showFullScreen();
    auto menuButton = window->findChild<QQuickItem *>(QStringLiteral("mainMenuButton"));
    QTRY_VERIFY(window->property("isFullScreen").toBool());
    QVERIFY(!menuButton->isVisible());
    QVERIFY(!window->property("minimizeButtonVisible").toBool());
    QVERIFY(!window->property("closeButtonVisible").toBool());
    window->showNormal();

    window->close();
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.join(QLatin1Char('\n'))));
}

QTEST_MAIN(UiTest)
void UiTest::replayAtEnd()
{
    if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
        QSKIP("ffmpeg is needed to make the test clip");
    const QString clip = makeClip();
    QVERIFY(!clip.isEmpty());

    Application application(m_dir.filePath(QStringLiteral("replay.conf")));
    QQmlApplicationEngine engine;
    QQuickWindow *window = load(engine, application, false);
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto player = window->findChild<MpvItem *>(QStringLiteral("player"));
    QVERIFY(player);
    application.openUrls({QUrl::fromLocalFile(clip)});
    QTRY_VERIFY_WITH_TIMEOUT(player->loaded(), 10000);

    player->seek(19, true);
    player->setPaused(false);
    QTRY_VERIFY_WITH_TIMEOUT(player->endReached(), 10000);
    QTRY_VERIFY(player->paused());

    // The play button at the end starts the file over.
    auto play = window->findChild<QQuickItem *>(QStringLiteral("playButton"));
    QVERIFY(play);
    QTRY_VERIFY(play->isVisible());
    QTest::qWait(500);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, sceneRect(play).center().toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(!player->endReached() && !player->paused(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(player->position() > 0.5 && player->position() < 5, 5000);
}

#include "tst_ui.moc"
