// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QMutex>
#include <QObject>
#include <QUrl>
#include <QWaitCondition>

class QThread;

// Reads the length of queued local files with a silent mpv on a thread of its
// own, stopping each file as soon as it has loaded.
class DurationProbe : public QObject
{
    Q_OBJECT

public:
    explicit DurationProbe(QObject *parent = nullptr);
    ~DurationProbe() override;

    // Replaces what is still to be read; streams are left out.
    void probe(const QList<QUrl> &urls);

Q_SIGNALS:
    // seconds is 0 when the file has no length or could not be opened.
    void found(const QUrl &url, double seconds);

private:
    void run();

    QThread *m_thread = nullptr;
    QMutex m_mutex;
    QWaitCondition m_wake;
    // Guarded by m_mutex.
    bool m_stopping = false;
    QList<QUrl> m_queue;
    struct mpv_handle *m_activeMpv = nullptr;
};
