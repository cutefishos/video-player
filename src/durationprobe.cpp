// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "durationprobe.h"
#include "mpvwait.h"

#include <QElapsedTimer>
#include <QThread>

#include <mpv/client.h>

#include <clocale>

DurationProbe::DurationProbe(QObject *parent)
    : QObject(parent)
{
    m_thread = QThread::create([this] { run(); });
    m_thread->setObjectName(QStringLiteral("DurationProbe"));
}

DurationProbe::~DurationProbe()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stopping = true;
        if (m_activeMpv)
            mpv_wakeup(m_activeMpv);
        m_wake.wakeAll();
    }
    m_thread->wait();
    delete m_thread;
}

void DurationProbe::probe(const QList<QUrl> &urls)
{
    QList<QUrl> queue;
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            queue.append(url);
    }
    {
        QMutexLocker locker(&m_mutex);
        m_queue = queue;
        m_wake.wakeAll();
    }
    if (!m_thread->isRunning() && !queue.isEmpty()) {
        // mpv needs the C locale for numbers, and the queue may come before the player.
        std::setlocale(LC_NUMERIC, "C");
        m_thread->start(QThread::LowPriority);
    }
}

void DurationProbe::run()
{
    mpv_handle *mpv = mpv_create();
    if (!mpv)
        return;
    const auto setOption = [mpv](const char *name, const char *value) {
        mpv_set_option_string(mpv, name, value);
    };
    // With every track off mpv ends the file before it counts as loaded, so
    // tracks stay selected and the null outputs skip what decoding they can.
    setOption("vo", "null");
    setOption("ao", "null");
    setOption("sid", "no");
    setOption("hwdec", "no");
    setOption("vd-lavc-skipframe", "all");
    setOption("vd-lavc-skipidct", "all");
    setOption("pause", "yes");
    setOption("idle", "yes");
    setOption("cache", "no");
    setOption("osd-level", "0");
    setOption("load-scripts", "no");
    setOption("ytdl", "no");
    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_activeMpv = mpv;
    }

    const auto stopping = [this] {
        QMutexLocker locker(&m_mutex);
        return m_stopping;
    };

    Q_FOREVER {
        QUrl url;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_stopping && m_queue.isEmpty())
                m_wake.wait(&m_mutex);
            if (m_stopping)
                break;
            url = m_queue.takeFirst();
        }

        const QByteArray path = url.toLocalFile().toUtf8();
        const char *load[] = {"loadfile", path.constData(), nullptr};
        double seconds = 0;
        if (mpv_command(mpv, load) >= 0) {
            bool ended = false;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 5000 && !stopping()) {
                const mpv_event_id event = mpv_wait_event(mpv, (5000 - timer.elapsed()) / 1000.0)->event_id;
                if (event == MPV_EVENT_FILE_LOADED) {
                    mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &seconds);
                    break;
                }
                if (event == MPV_EVENT_END_FILE) {
                    ended = true;
                    break;
                }
            }
            // The next loadfile would otherwise see this file's end first.
            const char *stop[] = {"stop", nullptr};
            if (!ended && mpv_command(mpv, stop) >= 0)
                waitForMpvEvent(mpv, MPV_EVENT_END_FILE, 2000, stopping);
        }
        if (stopping())
            break;
        QMetaObject::invokeMethod(this, [this, url, seconds] {
            Q_EMIT found(url, seconds);
        }, Qt::QueuedConnection);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_activeMpv = nullptr;
    }
    mpv_terminate_destroy(mpv);
}
