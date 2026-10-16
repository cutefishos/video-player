// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "widgets.h"

#include "playersettings.h"

#include <functional>
#include <vector>

class MainWindow;

// A short message over the video, with an optional action.
class Osd : public cutefish::ui::View
{
public:
    Osd();

    // From the plate's edge to its text when there is no action.
    static constexpr double kTextInset = 12;

    void show(const std::string& message, int timeoutMs, std::string actionText, std::function<void()> action);
    void hide();
    cutefish::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return isVisible() && m_opacity.target() > 0; }

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    void hoverChanged() override;

private:
    void fadeTo(double opacity);

    cutefish::ui::Label* m_label;
    views::PillButton* m_action;
    std::function<void()> m_callback;
    cutefish::base::Timer m_hideTimer;
    views::Tween m_opacity{0, std::chrono::milliseconds(180)};
};

// The play or pause glyph a click on the video flashes.
class Flash : public cutefish::ui::View
{
public:
    Flash();

    void show(const std::string& icon);

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    void apply();

    cutefish::ui::Icon* m_icon;
    std::unique_ptr<cutefish::ui::Animation> m_animation;
    double m_value = 0;
    double m_scale = 1;
};

class RecentRow : public views::Clickable
{
public:
    explicit RecentRow(const PlayerSettings::RecentFile& file, double resumeAt, bool last);

    cutefish::ui::SizeF sizeHint() const override { return {0, 52}; }

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    cutefish::ui::Label* m_name;
    cutefish::ui::Label* m_folder;
    cutefish::ui::Label* m_resume;
    bool m_last;
};

// How to open a video, and the recently watched ones.
class EmptyPage : public cutefish::ui::View
{
public:
    explicit EmptyPage(MainWindow& owner);

    // Re-read on each showing, which leaves out files deleted since.
    void reload();

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    MainWindow& m_owner;
    cutefish::ui::ThemeIcon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_subtitle;
    views::PillButton* m_openFile;
    views::PillButton* m_openUrl;
    cutefish::ui::Label* m_recentLabel;
    views::Clickable* m_clear;
    cutefish::ui::Label* m_clearLabel;
    cutefish::ui::View* m_card;
    std::vector<RecentRow*> m_rows;
};

// The page in place of the picture when a file cannot be played.
class ErrorPage : public cutefish::ui::View
{
public:
    explicit ErrorPage(MainWindow& owner);

    void update(const std::string& text, bool hasNext);

protected:
    void layout() override;

private:
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_detail;
    views::PillButton* m_another;
    views::PillButton* m_next;
};
