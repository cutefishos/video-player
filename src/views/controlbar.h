// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "player.h"
#include "widgets.h"

#include <vector>

class MainWindow;
class Thumbnailer;

// A press jumps there, a drag scrubs on keyframes and lands
// exactly on release; hovering shows the frame and time under the pointer.
class SeekBar : public cutefish::ui::View
{
public:
    explicit SeekBar(MainWindow& owner);

    void update(double duration, double position, const std::vector<Player::Chapter>& chapters,
                Thumbnailer* thumbnailer, bool enabled);
    bool isDragging() const { return m_dragging; }
    bool isHoveredBar() const { return m_hovered; }
    // What the bar shows: the drag while scrubbing, else the player.
    double shownPosition() const { return m_dragging ? m_dragTime : m_position; }

    // The bar is 20 tall; its view is 6 taller above and below for the hit area.
    static constexpr double kHitMargin = 6;
    cutefish::ui::SizeF sizeHint() const override { return {0, 20 + 2 * kHitMargin}; }
    bool acceptsPointer() const override { return m_enabled; }

    std::function<void()> onDraggingChanged;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerMove(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;
    void hoverMove(const cutefish::ui::PointerEvent& event) override;

private:
    class Preview;

    double timeAt(double x) const;
    std::string chapterAt(double seconds) const;
    void setHovered(bool hovered);
    void updatePreview();

    MainWindow& m_owner;
    double m_duration = 0;
    double m_position = 0;
    std::vector<Player::Chapter> m_chapters;
    Thumbnailer* m_thumbnailer = nullptr;
    bool m_enabled = false;
    bool m_hovered = false;
    bool m_dragging = false;
    double m_dragTime = 0;
    double m_mouseX = 0;
    views::Tween m_trackHeight{4, std::chrono::milliseconds(120), cutefish::ui::Animation::Easing::OutCubic};
    views::Tween m_handleScale{0, std::chrono::milliseconds(120), cutefish::ui::Animation::Easing::OutCubic};
    Preview* m_preview;
};

// The speaker mutes; its wheel and the slider set the
// volume, which boosts past the notch at 100% up to 150%.
class VolumeControl : public cutefish::ui::View
{
public:
    explicit VolumeControl(MainWindow& owner);

    void update(int volume, bool muted);
    void retranslate();
    bool isDragging() const { return m_dragging; }
    cutefish::ui::SizeF sizeHint() const override { return {36 + 2 + 72, 36}; }
    bool acceptsPointer() const override { return true; }

    std::function<void()> onDraggingChanged;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerMove(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;
    bool scroll(const cutefish::ui::ScrollEvent& event) override;

private:
    void apply(double x);
    cutefish::ui::RectF trackRect() const;

    MainWindow& m_owner;
    views::ControlButton* m_mute;
    int m_volume = 100;
    bool m_muted = false;
    bool m_hovered = false;
    bool m_dragging = false;
};

// The floating panel at the bottom: volume, the transport,
// the side panel and full screen; the timeline below.
class ControlBar : public cutefish::ui::View
{
public:
    explicit ControlBar(MainWindow& owner);

    static constexpr double kWidth = 460;
    static constexpr double kHeight = 10 + 40 + 2 + 20 + 12;

    // Pulls the player's and the window's state.
    void refresh();
    // The tooltips that refresh() leaves as they are.
    void retranslate();
    bool isBusy() const { return m_seekBar->isDragging() || m_volume->isDragging(); }
    bool isHoveredBar() const { return isHovered(); }
    cutefish::ui::SizeF sizeHint() const override { return {kWidth, kHeight}; }
    // Presses and wheels on the plate stay off the video below it.
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;
    bool scroll(const cutefish::ui::ScrollEvent&) override { return true; }

private:
    MainWindow& m_owner;
    VolumeControl* m_volume;
    views::ControlButton* m_skipBack;
    views::ControlButton* m_play;
    views::ControlButton* m_skipForward;
    views::ControlButton* m_panel;
    views::ControlButton* m_fullScreen;
    cutefish::ui::Label* m_elapsed;
    SeekBar* m_seekBar;
    views::Clickable* m_totalArea;
    cutefish::ui::Label* m_total;
    cutefish::ui::Label* m_metrics;
    bool m_showRemaining = false;
};
