// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "application.h"
#include "mpvnode.h"
#include "playersettings.h"
#include "playlist.h"
#include "resumestore.h"
#include "tracks.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class BackendTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void folderPlaylistNaturalOrder();
    void openWithoutFolder();
    void openSeveralKeepsOrder();
    void navigationAndRepeat();
    void appendRemoveMove();
    void durationsAreKept();
    void mediaAndSubtitleSuffixes();

    void resumeRules_data();
    void resumeRules();
    void resumeRoundTrip();
    void resumeCapacity();

    void settingsPersist();
    void recentFiles();
    void languagePreference();

    void formatTime_data();
    void formatTime();
    void urlFromInput_data();
    void urlFromInput();
    void openUrlsSplitsSubtitles();
    void screenshotPathIsUnique();

    void trackSplit();
    void mpvNodeRoundTrip();

private:
    QString touch(const QString &name);
    QString settingsFile() const { return m_dir->filePath(QStringLiteral("video-player.conf")); }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void BackendTest::initTestCase()
{
    // Nothing here may touch the user's own files.
    QStandardPaths::setTestModeEnabled(true);
}

void BackendTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

QString BackendTest::touch(const QString &name)
{
    const QString path = m_dir->filePath(name);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    return path;
}

void BackendTest::folderPlaylistNaturalOrder()
{
    touch(QStringLiteral("Episode 10.mkv"));
    const QString second = touch(QStringLiteral("Episode 2.mkv"));
    touch(QStringLiteral("episode 1.mp4"));
    touch(QStringLiteral("notes.txt"));
    touch(QStringLiteral("Episode 2.srt"));
    touch(QStringLiteral("soundtrack.FLAC"));

    Playlist playlist;
    QSignalSpy current(&playlist, &Playlist::currentIndexChanged);
    playlist.open({QUrl::fromLocalFile(second)});

    QStringList titles;
    for (const QUrl &url : playlist.urls())
        titles.append(Playlist::titleFor(url));
    QCOMPARE(titles, (QStringList{QStringLiteral("episode 1.mp4"), QStringLiteral("Episode 2.mkv"),
                                  QStringLiteral("Episode 10.mkv"), QStringLiteral("soundtrack.FLAC")}));
    QCOMPARE(playlist.currentIndex(), 1);
    QCOMPARE(playlist.currentUrl(), QUrl::fromLocalFile(second));
    QCOMPARE(current.count(), 1);
}

void BackendTest::openWithoutFolder()
{
    touch(QStringLiteral("a.mkv"));
    const QString b = touch(QStringLiteral("b.mkv"));
    Playlist playlist;
    playlist.open({QUrl::fromLocalFile(b)}, false);
    QCOMPARE(playlist.count(), 1);
    QCOMPARE(playlist.currentIndex(), 0);

    // A file the folder scan would not list, such as an unknown suffix, is still played.
    const QString odd = touch(QStringLiteral("clip.xyz"));
    playlist.open({QUrl::fromLocalFile(odd)});
    QCOMPARE(playlist.currentUrl(), QUrl::fromLocalFile(odd));
}

void BackendTest::openSeveralKeepsOrder()
{
    const QUrl stream(QStringLiteral("https://example.com/live.m3u8"));
    const QUrl file = QUrl::fromLocalFile(touch(QStringLiteral("z.mkv")));
    Playlist playlist;
    playlist.open({stream, file, stream});
    QCOMPARE(playlist.urls(), (QList<QUrl>{stream, file}));
    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(Playlist::titleFor(stream), QStringLiteral("live.m3u8"));
}

void BackendTest::durationsAreKept()
{
    const QUrl a = QUrl::fromLocalFile(touch(QStringLiteral("d1.mkv")));
    const QUrl b = QUrl::fromLocalFile(touch(QStringLiteral("d2.mkv")));
    Playlist playlist;
    playlist.open({a, b}, false);
    QCOMPARE(playlist.unprobed(), (QList<QUrl>{a, b}));

    QSignalSpy changed(&playlist, &QAbstractItemModel::dataChanged);
    playlist.setDuration(b, 61.5);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().at(0).toModelIndex().row(), 1);
    QCOMPARE(playlist.data(playlist.index(1), Playlist::DurationRole).toDouble(), 61.5);
    QCOMPARE(playlist.data(playlist.index(0), Playlist::DurationRole).toDouble(), 0.0);
    // A file without a length counts as read.
    playlist.setDuration(a, 0);
    QVERIFY(playlist.unprobed().isEmpty());
    playlist.setDuration(b, 61.5);
    QCOMPARE(changed.count(), 2);

    // A new queue keeps what was read before.
    playlist.open({b}, false);
    QCOMPARE(playlist.data(playlist.index(0), Playlist::DurationRole).toDouble(), 61.5);
    QVERIFY(playlist.unprobed().isEmpty());
}

void BackendTest::navigationAndRepeat()
{
    Playlist playlist;
    const QList<QUrl> urls{QUrl(QStringLiteral("https://a/1.mp4")), QUrl(QStringLiteral("https://a/2.mp4")),
                           QUrl(QStringLiteral("https://a/3.mp4"))};
    playlist.open(urls);
    QVERIFY(!playlist.hasPrevious());
    QVERIFY(playlist.hasNext());
    QVERIFY(playlist.next());
    QVERIFY(playlist.next());
    QCOMPARE(playlist.currentIndex(), 2);
    QVERIFY(!playlist.hasNext());
    QVERIFY(!playlist.next(true));

    playlist.setRepeatMode(Playlist::RepeatAll);
    QVERIFY(playlist.hasNext());
    QVERIFY(playlist.next(true));
    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(playlist.previousIndex(), 2);

    // Repeat One replays the file when it ends, but Next still moves on.
    playlist.setRepeatMode(Playlist::RepeatOne);
    QSignalSpy current(&playlist, &Playlist::currentIndexChanged);
    QVERIFY(playlist.next(true));
    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(current.count(), 1);
    QVERIFY(playlist.next(false));
    QCOMPARE(playlist.currentIndex(), 1);
}

void BackendTest::appendRemoveMove()
{
    Playlist playlist;
    const QUrl a(QStringLiteral("https://a/a.mp4"));
    const QUrl b(QStringLiteral("https://a/b.mp4"));
    const QUrl c(QStringLiteral("https://a/c.mp4"));
    playlist.open({a, b});
    playlist.append({b, c});
    QCOMPARE(playlist.urls(), (QList<QUrl>{a, b, c}));

    playlist.setCurrentIndex(1);
    playlist.move(1, 2);
    QCOMPARE(playlist.urls(), (QList<QUrl>{a, c, b}));
    QCOMPARE(playlist.currentIndex(), 2);
    playlist.move(0, 2);
    QCOMPARE(playlist.urls(), (QList<QUrl>{c, b, a}));
    QCOMPARE(playlist.currentIndex(), 1);

    playlist.remove(0);
    QCOMPARE(playlist.currentIndex(), 0);
    QCOMPARE(playlist.currentUrl(), b);
    playlist.remove(0);
    QCOMPARE(playlist.currentIndex(), -1);
    QCOMPARE(playlist.count(), 1);
    QCOMPARE(playlist.nextIndex(), 0);
    playlist.clear();
    QCOMPARE(playlist.count(), 0);
}

void BackendTest::mediaAndSubtitleSuffixes()
{
    QVERIFY(Playlist::isMediaFile(QStringLiteral("/x/Movie.MKV")));
    QVERIFY(Playlist::isMediaFile(QStringLiteral("song.opus")));
    QVERIFY(!Playlist::isMediaFile(QStringLiteral("Movie.srt")));
    QVERIFY(Playlist::isSubtitleFile(QStringLiteral("Movie.zh-CN.ass")));
    QVERIFY(Application::isSubtitle(QUrl::fromLocalFile(QStringLiteral("/x/a.vtt"))));
    QVERIFY(!Playlist::isSubtitleFile(QStringLiteral("a.mp4")));

    // .ts: an MPEG transport stream, or a Qt translation that is not media.
    QFile translation(touch(QStringLiteral("zh_CN.ts")));
    translation.open(QIODevice::WriteOnly);
    translation.write("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE TS>\n<TS version=\"2.1\" language=\"zh_CN\">\n</TS>\n");
    translation.close();
    QVERIFY(!Playlist::isMediaFile(translation.fileName()));
    QFile stream(touch(QStringLiteral("broadcast.ts")));
    stream.open(QIODevice::WriteOnly);
    QByteArray packets(188 * 4, '\xff');
    for (int i = 0; i < packets.size(); i += 188)
        packets[i] = 0x47;
    stream.write(packets);
    stream.close();
    QVERIFY(Playlist::isMediaFile(stream.fileName()));
    QCOMPARE(Playlist::folderMedia(stream.fileName()).size(), 1);
}

void BackendTest::resumeRules_data()
{
    QTest::addColumn<double>("position");
    QTest::addColumn<double>("duration");
    QTest::addColumn<bool>("kept");
    QTest::newRow("short file") << 100.0 << 170.0 << false;
    QTest::newRow("first minute") << 59.0 << 3600.0 << false;
    QTest::newRow("middle") << 600.0 << 3600.0 << true;
    QTest::newRow("credits") << 3500.0 << 3600.0 << false;
    QTest::newRow("unknown length") << 600.0 << 0.0 << false;
}

void BackendTest::resumeRules()
{
    QFETCH(double, position);
    QFETCH(double, duration);
    QFETCH(bool, kept);
    QCOMPARE(ResumeStore::worthKeeping(position, duration), kept);

    ResumeStore store(settingsFile());
    const QUrl url = QUrl::fromLocalFile(QStringLiteral("/videos/film.mkv"));
    store.save(url, 1200, 3600);
    store.save(url, position, duration);
    QCOMPARE(store.position(url) > 0, kept);
}

void BackendTest::resumeRoundTrip()
{
    const QUrl url = QUrl::fromLocalFile(QStringLiteral("/videos/中文 film [1].mkv"));
    {
        ResumeStore store(settingsFile());
        store.save(url, 754.25, 5400, {{QStringLiteral("audioTrack"), 2},
                                      {QStringLiteral("subtitleTrack"), 0},
                                      {QStringLiteral("subtitleDelay"), -0.3}});
    }
    ResumeStore store(settingsFile());
    const QVariantMap entry = store.entry(url);
    QCOMPARE(entry.value(QStringLiteral("position")).toDouble(), 754.25);
    QCOMPARE(entry.value(QStringLiteral("audioTrack")).toInt(), 2);
    QCOMPARE(entry.value(QStringLiteral("subtitleTrack")).toInt(), 0);
    QCOMPARE(entry.value(QStringLiteral("subtitleDelay")).toDouble(), -0.3);
    QVERIFY(store.entry(QUrl::fromLocalFile(QStringLiteral("/videos/other.mkv"))).isEmpty());

    // Without choices, tracks fall back to the file's defaults.
    store.save(url, 800, 5400);
    QCOMPARE(store.entry(url).value(QStringLiteral("audioTrack")).toInt(), -1);

    store.forget(url);
    QCOMPARE(store.position(url), 0.0);
}

void BackendTest::resumeCapacity()
{
    ResumeStore store(settingsFile());
    for (int i = 0; i <= ResumeStore::Capacity; ++i)
        store.save(QUrl(QStringLiteral("https://example.com/%1.mp4").arg(i)), 100, 1000);
    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Resume"));
    QCOMPARE(settings.childKeys().size(), ResumeStore::Capacity);
    QVERIFY(store.position(QUrl(QStringLiteral("https://example.com/%1.mp4").arg(ResumeStore::Capacity))) > 0);
}

void BackendTest::settingsPersist()
{
    {
        PlayerSettings settings(settingsFile());
        QCOMPARE(settings.volume(), 100);
        QVERIFY(settings.resumePlayback());
        QVERIFY(settings.folderPlaylist());
        QVERIFY(settings.screenshotFolder().endsWith(QStringLiteral("/Screenshots")));
        QSignalSpy volume(&settings, &PlayerSettings::volumeChanged);
        settings.setVolume(180);
        QCOMPARE(settings.volume(), 150);
        settings.setVolume(150);
        QCOMPARE(volume.count(), 1);
        settings.setMuted(true);
        settings.setResumePlayback(false);
        settings.setFolderPlaylist(false);
        settings.setRepeatMode(2);
        settings.setSubtitleScale(125);
        settings.setSubtitleLanguage(QStringLiteral("ja"));
        settings.setScreenshotFolder(QStringLiteral("/tmp/shots"));
    }
    PlayerSettings settings(settingsFile());
    QCOMPARE(settings.volume(), 150);
    QVERIFY(settings.muted());
    QVERIFY(!settings.resumePlayback());
    QVERIFY(!settings.folderPlaylist());
    QCOMPARE(settings.repeatMode(), 2);
    QCOMPARE(settings.subtitleScale(), 125);
    QCOMPARE(settings.subtitleLanguage(), QStringLiteral("ja"));
    QCOMPARE(settings.screenshotFolder(), QStringLiteral("/tmp/shots"));
}

void BackendTest::recentFiles()
{
    PlayerSettings settings(settingsFile());
    const QString first = touch(QStringLiteral("first.mkv"));
    settings.addRecentFile(QUrl::fromLocalFile(first));
    for (int i = 0; i < PlayerSettings::RecentCapacity; ++i)
        settings.addRecentFile(QUrl::fromLocalFile(touch(QStringLiteral("f%1.mkv").arg(i))));
    QCOMPARE(settings.recentFiles().size(), PlayerSettings::RecentCapacity);
    QCOMPARE(settings.recentFiles().first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("f%1.mkv").arg(PlayerSettings::RecentCapacity - 1));

    // Reopening moves a file to the top, and deleted files are not listed.
    settings.addRecentFile(QUrl::fromLocalFile(m_dir->filePath(QStringLiteral("f3.mkv"))));
    QCOMPARE(settings.recentFiles().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("f3.mkv"));
    QFile::remove(m_dir->filePath(QStringLiteral("f3.mkv")));
    QCOMPARE(settings.recentFiles().size(), PlayerSettings::RecentCapacity - 1);

    const QUrl stream(QStringLiteral("https://example.com/a.mp4"));
    settings.addRecentFile(stream);
    QCOMPARE(settings.recentFiles().first().toMap().value(QStringLiteral("folder")).toString(), QStringLiteral("example.com"));
    settings.removeRecentFile(stream);
    settings.clearRecentFiles();
    QVERIFY(settings.recentFiles().isEmpty());
    QVERIFY(PlayerSettings(settingsFile()).recentFiles().isEmpty());
}

void BackendTest::languagePreference()
{
    PlayerSettings settings(settingsFile());
    const QStringList codes = settings.languagePreference(QStringLiteral("zh"));
    QCOMPARE(codes.mid(0, 3), (QStringList{QStringLiteral("zh"), QStringLiteral("chi"), QStringLiteral("zho")}));
    QVERIFY(codes.contains(QStringLiteral("en")));
    QVERIFY(codes.contains(QStringLiteral("eng")));
    QCOMPARE(codes.count(QStringLiteral("en")), 1);
}

void BackendTest::formatTime_data()
{
    QTest::addColumn<double>("seconds");
    QTest::addColumn<double>("reference");
    QTest::addColumn<QString>("text");
    QTest::newRow("zero") << 0.0 << 0.0 << QStringLiteral("0:00");
    QTest::newRow("seconds") << 65.9 << 0.0 << QStringLiteral("1:05");
    QTest::newRow("hours") << 3725.0 << 0.0 << QStringLiteral("1:02:05");
    QTest::newRow("long film") << 65.0 << 7200.0 << QStringLiteral("0:01:05");
    QTest::newRow("negative") << -3.0 << 0.0 << QStringLiteral("0:00");
}

void BackendTest::formatTime()
{
    QFETCH(double, seconds);
    QFETCH(double, reference);
    QFETCH(QString, text);
    QCOMPARE(Application::formatTime(seconds, reference), text);
}

void BackendTest::urlFromInput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("url");
    QTest::newRow("https") << QStringLiteral(" https://example.com/v.mp4 ") << QStringLiteral("https://example.com/v.mp4");
    QTest::newRow("bare host") << QStringLiteral("example.com/v.mp4") << QStringLiteral("http://example.com/v.mp4");
    QTest::newRow("rtsp") << QStringLiteral("rtsp://cam.local/stream") << QStringLiteral("rtsp://cam.local/stream");
    QTest::newRow("word") << QStringLiteral("hello") << QString();
    QTest::newRow("sentence") << QStringLiteral("two\nlines") << QString();
    QTest::newRow("missing file") << QStringLiteral("/no/such/file.mkv") << QString();
    QTest::newRow("javascript") << QStringLiteral("javascript:alert(1)") << QString();
}

void BackendTest::urlFromInput()
{
    QFETCH(QString, input);
    QFETCH(QString, url);
    QCOMPARE(Application::urlFromInput(input).toString(), url);
}

void BackendTest::openUrlsSplitsSubtitles()
{
    Application application(settingsFile());
    QSignalSpy requested(&application, &Application::openRequested);
    const QUrl video = QUrl::fromLocalFile(touch(QStringLiteral("show/a.mkv")));
    const QUrl subtitle = QUrl::fromLocalFile(touch(QStringLiteral("show/a.srt")));
    touch(QStringLiteral("show/b.mkv"));
    application.openUrls({subtitle, video});
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.at(0).at(0).value<QList<QUrl>>(), QList<QUrl>{video});
    QCOMPARE(requested.at(0).at(1).value<QList<QUrl>>(), QList<QUrl>{subtitle});

    // A dropped folder plays its media files.
    application.openUrls({QUrl::fromLocalFile(m_dir->filePath(QStringLiteral("show")))});
    QCOMPARE(requested.count(), 2);
    QCOMPARE(requested.at(1).at(0).value<QList<QUrl>>().size(), 2);
}

void BackendTest::screenshotPathIsUnique()
{
    Application application(settingsFile());
    application.settings()->setScreenshotFolder(m_dir->filePath(QStringLiteral("shots")));
    const QString path = application.screenshotPath(QStringLiteral("AC/DC live"));
    QVERIFY(QFileInfo(path).absolutePath() == m_dir->filePath(QStringLiteral("shots")));
    QVERIFY(QFileInfo(path).fileName().startsWith(QStringLiteral("AC-DC live ")));
    QVERIFY(path.endsWith(QStringLiteral(".png")));
    QFile(path).open(QIODevice::WriteOnly);
    const QString next = application.screenshotPath(QStringLiteral("AC/DC live"));
    if (QFileInfo(next).baseName() == QFileInfo(path).baseName())
        QVERIFY(next.endsWith(QStringLiteral(" (2).png")));
    QVERIFY(next != path);
}

void BackendTest::trackSplit()
{
    const QVariantList list{
        QVariantMap{{QStringLiteral("id"), 1}, {QStringLiteral("type"), QStringLiteral("video")}},
        QVariantMap{{QStringLiteral("id"), 1}, {QStringLiteral("type"), QStringLiteral("audio")},
                    {QStringLiteral("lang"), QStringLiteral("jpn")}, {QStringLiteral("selected"), true}},
        QVariantMap{{QStringLiteral("id"), 2}, {QStringLiteral("type"), QStringLiteral("audio")},
                    {QStringLiteral("title"), QStringLiteral("Commentary")}, {QStringLiteral("lang"), QStringLiteral("en")}},
        QVariantMap{{QStringLiteral("id"), 3}, {QStringLiteral("type"), QStringLiteral("audio")}},
        QVariantMap{{QStringLiteral("id"), 1}, {QStringLiteral("type"), QStringLiteral("sub")},
                    {QStringLiteral("title"), QStringLiteral("English")}, {QStringLiteral("lang"), QStringLiteral("eng")},
                    {QStringLiteral("external"), true}},
    };
    const Tracks::Split split = Tracks::split(list);
    QVERIFY(split.hasVideo);
    QCOMPARE(split.audio.size(), 3);
    QCOMPARE(split.subtitle.size(), 1);
    QCOMPARE(split.audio.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("日本語"));
    QVERIFY(split.audio.at(0).toMap().value(QStringLiteral("selected")).toBool());
    QCOMPARE(split.audio.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Commentary (English)"));
    QCOMPARE(split.audio.at(2).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Track 3"));
    // A title that only names the language is not repeated.
    QCOMPARE(split.subtitle.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("English"));
    QVERIFY(split.subtitle.at(0).toMap().value(QStringLiteral("external")).toBool());

    // Cover art is not a picture to show.
    const Tracks::Split audioOnly = Tracks::split({QVariantMap{{QStringLiteral("type"), QStringLiteral("video")},
                                                               {QStringLiteral("albumart"), true}}});
    QVERIFY(!audioOnly.hasVideo);
}

void BackendTest::mpvNodeRoundTrip()
{
    const QVariantMap value{
        {QStringLiteral("flag"), true},
        {QStringLiteral("int"), 42},
        {QStringLiteral("double"), 1.5},
        {QStringLiteral("text"), QStringLiteral("é漢")},
        {QStringLiteral("list"), QVariantList{1, QStringLiteral("two"), QVariantList{}}},
    };
    mpv_node node;
    MpvNode::fromVariant(&node, value);
    QCOMPARE(node.format, MPV_FORMAT_NODE_MAP);
    const QVariantMap back = MpvNode::toVariant(&node).toMap();
    MpvNode::release(&node);
    QCOMPARE(node.format, MPV_FORMAT_NONE);
    QCOMPARE(back.value(QStringLiteral("flag")), QVariant(true));
    QCOMPARE(back.value(QStringLiteral("int")).toLongLong(), 42);
    QCOMPARE(back.value(QStringLiteral("double")).toDouble(), 1.5);
    QCOMPARE(back.value(QStringLiteral("text")).toString(), QStringLiteral("é漢"));
    QCOMPARE(back.value(QStringLiteral("list")).toList().size(), 3);
    QCOMPARE(back.value(QStringLiteral("list")).toList().at(1).toString(), QStringLiteral("two"));

    MpvNode::fromVariant(&node, QUrl(QStringLiteral("https://a/b c")));
    QCOMPARE(node.format, MPV_FORMAT_STRING);
    QCOMPARE(QString::fromUtf8(node.u.string), QStringLiteral("https://a/b%20c"));
    MpvNode::release(&node);
}

QTEST_GUILESS_MAIN(BackendTest)
#include "tst_backend.moc"
