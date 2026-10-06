// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvitem.h"
#include "thumbnailer.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// Real playback through libmpv, on clips ffmpeg makes for the test, drawn by
// Qt Quick's software renderer into an offscreen window.
class MediaTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsTracksAndChapters();
    void seeksAndEnds();
    void resumesWithFileOptions();
    void screenshot();
    void recoversLostRenderContext();
    void itemGoesWhilePlaying();
    void reportsFailure();
    void thumbnails();

private:
    std::unique_ptr<MpvItem> showPlayer();

    QQuickWindow m_window;
    QString m_clip;
    QTemporaryDir m_dir;
};

static bool ffmpeg(const QStringList &arguments)
{
    QProcess process;
    process.start(QStringLiteral("ffmpeg"), QStringList{QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-y")} + arguments);
    return process.waitForFinished(60000) && process.exitCode() == 0;
}

void MediaTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    if (QStandardPaths::findExecutable(QStringLiteral("ffmpeg")).isEmpty())
        QSKIP("ffmpeg is needed to make the test clips");
    QVERIFY(m_dir.isValid());
    m_window.resize(320, 240);
    m_window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&m_window));

    const QString metadata = m_dir.filePath(QStringLiteral("chapters.txt"));
    QFile chapters(metadata);
    QVERIFY(chapters.open(QIODevice::WriteOnly));
    chapters.write(";FFMETADATA1\n"
                   "[CHAPTER]\nTIMEBASE=1/1000\nSTART=0\nEND=4000\ntitle=Opening\n"
                   "[CHAPTER]\nTIMEBASE=1/1000\nSTART=4000\nEND=8000\ntitle=Finale\n");
    chapters.close();

    const QString subtitle = m_dir.filePath(QStringLiteral("embedded.srt"));
    QFile srt(subtitle);
    QVERIFY(srt.open(QIODevice::WriteOnly));
    srt.write("1\n00:00:01,000 --> 00:00:03,000\nHello\n");
    srt.close();

    m_clip = m_dir.filePath(QStringLiteral("clip.mkv"));
    QVERIFY(ffmpeg({QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                    QStringLiteral("testsrc=size=320x240:rate=25:duration=8"),
                    QStringLiteral("-i"), subtitle, QStringLiteral("-i"), metadata,
                    QStringLiteral("-map"), QStringLiteral("0"), QStringLiteral("-map"), QStringLiteral("1"),
                    QStringLiteral("-map_metadata"), QStringLiteral("2"), QStringLiteral("-map_chapters"), QStringLiteral("2"),
                    QStringLiteral("-metadata:s:s:0"), QStringLiteral("language=eng"),
                    QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-g"), QStringLiteral("25"),
                    QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), QStringLiteral("-c:s"), QStringLiteral("srt"), m_clip}));

    // Found next to the video by its name.
    QVERIFY(QFile::copy(subtitle, m_dir.filePath(QStringLiteral("clip.zh.srt"))));
}

std::unique_ptr<MpvItem> MediaTest::showPlayer()
{
    auto player = std::make_unique<MpvItem>();
    player->setParentItem(m_window.contentItem());
    player->setSize(QSizeF(320, 240));
    return player;
}

void MediaTest::loadsTracksAndChapters()
{
    auto item = showPlayer();
    MpvItem &player = *item;
    QSignalSpy loaded(&player, &MpvItem::fileLoaded);
    player.load(QUrl::fromLocalFile(m_clip), {}, true);
    QVERIFY(loaded.wait(10000));
    QTRY_COMPARE_WITH_TIMEOUT(qRound(player.duration()), 8, 5000);
    QVERIFY(player.loaded());
    QVERIFY(player.seekable());
    QTRY_VERIFY_WITH_TIMEOUT(player.hasVideo(), 5000);
    QCOMPARE(player.subtitleTracks().size(), 2);
    QVERIFY(player.audioTracks().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(player.chapters().size(), 2, 5000);
    QCOMPARE(player.chapters().at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Finale"));
    QCOMPARE(player.chapters().at(1).toMap().value(QStringLiteral("time")).toDouble(), 4.0);

    const QVariantMap info = player.mediaInfo();
    QCOMPARE(info.value(QStringLiteral("video-codec")).toString().contains(QStringLiteral("264")), true);
    QCOMPARE(info.value(QStringLiteral("width")).toInt(), 320);

    QSignalSpy subtitle(&player, &MpvItem::subtitleTrackChanged);
    const int id = player.subtitleTracks().last().toMap().value(QStringLiteral("id")).toInt();
    player.setSubtitleTrack(id);
    QTRY_COMPARE_WITH_TIMEOUT(player.subtitleTrack(), id, 3000);
    player.setSubtitleTrack(0);
    QTRY_COMPARE_WITH_TIMEOUT(player.subtitleTrack(), 0, 3000);

    player.setChapter(1);
    QTRY_VERIFY_WITH_TIMEOUT(player.position() >= 3.9, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(player.chapter(), 1, 3000);
}

void MediaTest::seeksAndEnds()
{
    auto item = showPlayer();
    MpvItem &player = *item;
    QSignalSpy loaded(&player, &MpvItem::fileLoaded);
    player.load(QUrl::fromLocalFile(m_clip), {}, true);
    QVERIFY(loaded.wait(10000));
    QTRY_VERIFY(player.paused());

    player.seek(5.0, true);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(player.position() - 5.0) < 0.1, 5000);
    player.seekRelative(-2);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(player.position() - 3.0) < 0.1, 5000);

    player.setSpeed(2.0);
    QTRY_COMPARE(player.speed(), 2.0);
    player.setVolume(130);
    QTRY_COMPARE(player.volume(), 130);
    player.setMuted(true);
    QTRY_VERIFY(player.muted());

    // Kept on the last frame and paused at the end.
    player.setPaused(false);
    QTRY_VERIFY_WITH_TIMEOUT(player.endReached(), 10000);
    QTRY_VERIFY(player.paused());
    QVERIFY(player.loaded());

    // Playing again starts over.
    player.setPaused(false);
    QTRY_VERIFY_WITH_TIMEOUT(!player.endReached() && player.position() < 3, 5000);

    // A new file starts at normal speed.
    QSignalSpy reloaded(&player, &MpvItem::fileLoaded);
    player.load(QUrl::fromLocalFile(m_clip), {}, true);
    QVERIFY(reloaded.wait(10000));
    QTRY_COMPARE(player.speed(), 1.0);
}

void MediaTest::resumesWithFileOptions()
{
    auto item = showPlayer();
    MpvItem &player = *item;
    QSignalSpy loaded(&player, &MpvItem::fileLoaded);
    player.load(QUrl::fromLocalFile(m_clip), {{QStringLiteral("start"), 6.5}, {QStringLiteral("sub-delay"), 0.4}}, true);
    QVERIFY(loaded.wait(10000));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(player.position() - 6.5) < 0.2, 5000);
    QTRY_VERIFY(qAbs(player.subtitleDelay() - 0.4) < 0.001);
}

void MediaTest::screenshot()
{
    auto item = showPlayer();
    MpvItem &player = *item;
    QSignalSpy loaded(&player, &MpvItem::fileLoaded);
    player.load(QUrl::fromLocalFile(m_clip), {{QStringLiteral("start"), 2}}, true);
    QVERIFY(loaded.wait(10000));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(player.position() - 2) < 0.1 && !player.buffering(), 5000);
    QSignalSpy saved(&player, &MpvItem::screenshotSaved);
    const QString path = m_dir.filePath(QStringLiteral("shot.png"));
    player.screenshot(path);
    QVERIFY(saved.wait(10000));
    QCOMPARE(saved.first().at(0).toString(), path);
    QVERIFY(saved.first().at(1).toBool());
    // At the clip's own resolution, not the item's.
    QCOMPARE(QImage(path).size(), QSize(320, 240));
}

void MediaTest::recoversLostRenderContext()
{
    auto item = showPlayer();
    MpvItem &player = *item;
    QSignalSpy loaded(&player, &MpvItem::fileLoaded);
    player.load(QUrl::fromLocalFile(m_clip), {{QStringLiteral("start"), 2.0}}, false);
    QVERIFY(loaded.wait(10000));
    QTRY_VERIFY_WITH_TIMEOUT(player.videoActive(), 5000);
    QTRY_VERIFY(player.subtitleTrack() > 0);

    // Out of the window the context goes, and mpv drops its video output.
    player.setParentItem(nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(!player.videoActive(), 5000);
    const double position = player.position();
    QSignalSpy failed(&player, &MpvItem::playbackFailed);
    QSignalSpy reloaded(&player, &MpvItem::fileLoaded);
    player.setParentItem(m_window.contentItem());
    QTRY_VERIFY_WITH_TIMEOUT(player.videoActive(), 5000);
    QVERIFY(player.loaded());
    // Back where it was, quietly: no failure, no new file for the window.
    QVERIFY(player.position() >= position - 0.5);
    QVERIFY(failed.isEmpty());
    QVERIFY(reloaded.isEmpty());
    QVERIFY(player.subtitleTrack() > 0);
}

// Destroying the item while mpv still has events and frames to deliver.
void MediaTest::itemGoesWhilePlaying()
{
    for (int i = 0; i < 5; ++i) {
        auto item = showPlayer();
        QSignalSpy loaded(item.get(), &MpvItem::fileLoaded);
        item->load(QUrl::fromLocalFile(m_clip), {}, false);
        QVERIFY(loaded.wait(10000));
        item->setSpeed(4.0);
        QTest::qWait(20 * i);
        item.reset();
        QTest::qWait(10);
    }
}

void MediaTest::reportsFailure()
{
    auto item = showPlayer();
    MpvItem &player = *item;
    QSignalSpy failed(&player, &MpvItem::playbackFailed);
    player.load(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("missing.mkv"))));
    QVERIFY(failed.wait(10000));
    QVERIFY(!failed.first().first().toString().isEmpty());
    QVERIFY(!player.loaded());
}

void MediaTest::thumbnails()
{
    Thumbnailer thumbnailer;
    thumbnailer.setSource(QUrl::fromLocalFile(m_clip));
    thumbnailer.setDuration(8);
    const int bucket = thumbnailer.bucket(4.2);
    QCOMPARE(bucket, 4);
    QSignalSpy revision(&thumbnailer, &Thumbnailer::revisionChanged);
    thumbnailer.request(bucket);
    QTRY_VERIFY_WITH_TIMEOUT(thumbnailer.has(bucket), 10000);
    const QImage image = thumbnailer.image(bucket);
    QCOMPARE(image.size(), QSize(240, 180));
    // testsrc is colourful everywhere; a black frame means nothing was drawn.
    int lit = 0;
    for (int y = 0; y < image.height(); y += 10) {
        for (int x = 0; x < image.width(); x += 10)
            lit += qGray(image.pixel(x, y)) > 40;
    }
    QVERIFY2(lit > 100, qPrintable(QString::number(lit)));

    // A new file drops the old frames.
    thumbnailer.setSource(QUrl());
    QVERIFY(!thumbnailer.has(bucket));

    // Quitting mid-work does not wait out the worker's timeouts.
    for (int i = 0; i < 3; ++i) {
        auto busy = std::make_unique<Thumbnailer>();
        busy->setSource(QUrl::fromLocalFile(m_clip));
        busy->setDuration(8);
        busy->request(busy->bucket(1.0 + i));
        QTest::qWait(i * 15);
        QElapsedTimer timer;
        timer.start();
        busy.reset();
        QVERIFY2(timer.elapsed() < 1000, qPrintable(QString::number(timer.elapsed())));
    }
}

QTEST_MAIN(MediaTest)
#include "tst_media.moc"
