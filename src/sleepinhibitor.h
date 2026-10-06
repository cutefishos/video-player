// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusUnixFileDescriptor>
#include <QObject>

// Keeps the screen on while a video plays, and the computer awake while
// audio plays.
class SleepInhibitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    enum Mode {
        Off,
        // The screen stays on (org.freedesktop.ScreenSaver).
        Screen,
        // Only suspend is held off (logind), the screen may still blank.
        Suspend,
    };
    Q_ENUM(Mode)

    explicit SleepInhibitor(QObject *parent = nullptr);
    ~SleepInhibitor() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

Q_SIGNALS:
    void modeChanged();

private:
    void release();

    Mode m_mode = Off;
    // Bumped on every change, so a late reply for an older mode is dropped.
    quint64 m_generation = 0;
    uint m_cookie = 0;
    QDBusUnixFileDescriptor m_suspendLock;
};
