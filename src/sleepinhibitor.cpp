// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sleepinhibitor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

static const QString screenSaverService = QStringLiteral("org.freedesktop.ScreenSaver");
static const QString screenSaverPath = QStringLiteral("/ScreenSaver");
static const QString login1Service = QStringLiteral("org.freedesktop.login1");
static const QString login1Path = QStringLiteral("/org/freedesktop/login1");

SleepInhibitor::SleepInhibitor(QObject *parent)
    : QObject(parent)
{
}

SleepInhibitor::~SleepInhibitor()
{
    release();
}

void SleepInhibitor::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    release();
    m_mode = mode;
    const quint64 generation = ++m_generation;
    Q_EMIT modeChanged();

    if (mode == Screen) {
        QDBusMessage call = QDBusMessage::createMethodCall(screenSaverService, screenSaverPath,
                                                           screenSaverService, QStringLiteral("Inhibit"));
        call << QStringLiteral("cutefish-video-player") << QStringLiteral("Playing a video");
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *call) {
            call->deleteLater();
            const QDBusPendingReply<uint> reply = *call;
            if (reply.isError())
                return;
            if (generation == m_generation) {
                m_cookie = reply.value();
            } else {
                QDBusMessage undo = QDBusMessage::createMethodCall(screenSaverService, screenSaverPath,
                                                                   screenSaverService, QStringLiteral("UnInhibit"));
                undo << reply.value();
                QDBusConnection::sessionBus().asyncCall(undo);
            }
        });
    } else if (mode == Suspend) {
        QDBusMessage call = QDBusMessage::createMethodCall(login1Service, login1Path,
                                                           login1Service + QStringLiteral(".Manager"),
                                                           QStringLiteral("Inhibit"));
        call << QStringLiteral("sleep") << QStringLiteral("Video Player") << QStringLiteral("Playing audio")
             << QStringLiteral("block");
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *call) {
            call->deleteLater();
            const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *call;
            // An outdated lock is closed as the reply goes out of scope.
            if (!reply.isError() && generation == m_generation)
                m_suspendLock = reply.value();
        });
    }
}

void SleepInhibitor::release()
{
    if (m_cookie) {
        QDBusMessage call = QDBusMessage::createMethodCall(screenSaverService, screenSaverPath,
                                                           screenSaverService, QStringLiteral("UnInhibit"));
        call << m_cookie;
        QDBusConnection::sessionBus().asyncCall(call);
        m_cookie = 0;
    }
    m_suspendLock = QDBusUnixFileDescriptor();
}
