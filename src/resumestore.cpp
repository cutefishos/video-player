// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "resumestore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QSettings>

#include <algorithm>

static const QString resumeGroup = QStringLiteral("Resume");
static constexpr double minimumPosition = 60;
static constexpr double minimumDuration = 180;

ResumeStore::ResumeStore(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
{
}

QString ResumeStore::keyFor(const QUrl &url)
{
    // Paths hold characters QSettings keys cannot.
    return QString::fromLatin1(QCryptographicHash::hash(url.toString(QUrl::FullyEncoded).toUtf8(),
                                                        QCryptographicHash::Sha256).toHex());
}

bool ResumeStore::worthKeeping(double position, double duration)
{
    return duration >= minimumDuration && position >= minimumPosition && position < duration * 0.95;
}

// Stored as: position, time saved, audio track, subtitle track, subtitle delay.
QVariantMap ResumeStore::entry(const QUrl &url) const
{
    QSettings settings(m_file, QSettings::IniFormat);
    settings.beginGroup(resumeGroup);
    const QStringList fields = settings.value(keyFor(url)).toStringList();
    if (fields.size() < 5)
        return {};
    return {
        {QStringLiteral("position"), qMax(0.0, fields.at(0).toDouble())},
        {QStringLiteral("audioTrack"), fields.at(2).toInt()},
        {QStringLiteral("subtitleTrack"), fields.at(3).toInt()},
        {QStringLiteral("subtitleDelay"), fields.at(4).toDouble()},
    };
}

double ResumeStore::position(const QUrl &url) const
{
    return entry(url).value(QStringLiteral("position"), 0.0).toDouble();
}

void ResumeStore::save(const QUrl &url, double position, double duration, const QVariantMap &choices)
{
    if (url.isEmpty())
        return;
    if (!worthKeeping(position, duration)) {
        forget(url);
        return;
    }

    QSettings settings(m_file, QSettings::IniFormat);
    settings.beginGroup(resumeGroup);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    settings.setValue(keyFor(url), QStringList{
        QString::number(position, 'f', 3),
        QString::number(now),
        QString::number(choices.value(QStringLiteral("audioTrack"), -1).toInt()),
        QString::number(choices.value(QStringLiteral("subtitleTrack"), -1).toInt()),
        QString::number(choices.value(QStringLiteral("subtitleDelay"), 0.0).toDouble(), 'f', 3),
    });

    QStringList keys = settings.childKeys();
    if (keys.size() <= Capacity)
        return;
    auto stamp = [&settings](const QString &key) {
        return settings.value(key).toStringList().value(1).toLongLong();
    };
    std::sort(keys.begin(), keys.end(), [&](const QString &a, const QString &b) {
        return stamp(a) < stamp(b);
    });
    for (int i = 0; i < keys.size() - Capacity; ++i)
        settings.remove(keys.at(i));
}

void ResumeStore::forget(const QUrl &url)
{
    QSettings settings(m_file, QSettings::IniFormat);
    settings.beginGroup(resumeGroup);
    settings.remove(keyFor(url));
}

void ResumeStore::clear()
{
    QSettings settings(m_file, QSettings::IniFormat);
    settings.remove(resumeGroup);
}
