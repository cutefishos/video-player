// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/dbus.h"
#include "base/signal.h"
#include "base/weak_ptr.h"

#include <cstdint>
#include <map>

// Keeps the screen on while a video plays, and the computer awake while
// audio plays.
class SleepInhibitor
{
public:
    enum class Mode {
        Off,
        // The screen stays on (org.freedesktop.ScreenSaver).
        Screen,
        // Only suspend is held off (logind), the screen may still blank.
        Suspend,
    };

    // The buses default to the loop's shared session and system ones.
    explicit SleepInhibitor(cutefish::base::dbus::Connection* session = nullptr,
                            cutefish::base::dbus::Connection* system = nullptr);
    ~SleepInhibitor();

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    // The ScreenSaver cookie or logind lock is held.
    bool isHeld() const { return m_cookie != 0 || m_suspendLock.isValid(); }

    cutefish::base::Signal<> modeChanged;
    // A held inhibition arrived or went.
    cutefish::base::Signal<> heldChanged;

private:
    void release();
    void finish(uint64_t call);

    cutefish::base::dbus::Connection* m_session;
    cutefish::base::dbus::Connection* m_system;
    Mode m_mode = Mode::Off;
    // Bumped on every change, so a late reply for an older mode is undone.
    uint64_t m_generation = 0;
    uint32_t m_cookie = 0;
    cutefish::base::dbus::UnixFd m_suspendLock;
    // Kept until answered: a dropped call would leave its inhibition behind.
    std::map<uint64_t, cutefish::base::dbus::PendingCall> m_calls;
    cutefish::base::WeakPtrFactory<SleepInhibitor> m_weakFactory{this};
};
