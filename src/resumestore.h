// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>

// Where each recently watched file was left, with its audio and subtitle
// choices, so it reopens there. Only the most recent entries are kept.
class ResumeStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int Capacity = 200;

    explicit ResumeStore(const QString &file, QObject *parent = nullptr);

    // position (seconds), audioTrack, subtitleTrack (mpv ids, 0 = off,
    // -1 = the file's default) and subtitleDelay; empty when there is none.
    Q_INVOKABLE QVariantMap entry(const QUrl &url) const;
    Q_INVOKABLE double position(const QUrl &url) const;
    // Remembers where url was left, or forgets it when that is not worth
    // returning to: a short file, the first minute, or the end.
    Q_INVOKABLE void save(const QUrl &url, double position, double duration,
                          const QVariantMap &choices = {});
    Q_INVOKABLE void forget(const QUrl &url);
    Q_INVOKABLE void clear();

    static bool worthKeeping(double position, double duration);

private:
    static QString keyFor(const QUrl &url);

    QString m_file;
};
