// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dialogs.h"
#include "mainwindow.h"
#include "widgets.h"

#include "base/locale.h"
#include "base/process.h"
#include "base/strings.h"
#include "ui/application.h"
#include "ui/button.h"
#include "ui/clipboard.h"
#include "ui/combo_box.h"
#include "ui/dialog_window.h"
#include "ui/file_chooser.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/cell_layout.h"
#include "ui/scroll_bar.h"
#include "ui/scroll_view.h"
#include "ui/switch.h"
#include "ui/text_field.h"
#include "ui/theme.h"
#include "ui/window_header.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>

using namespace cutefish;
using namespace cutefish::ui;

namespace
{

// The separator under every visible row of a card but its last.
Color separatorColor()
{
    const Theme& theme = Theme::current();
    return theme.disabledText.withAlpha(theme.darkMode ? 0.3 : 0.1);
}

// A rounded card with rows of a fixed height (44 for information, 48 for preferences).
class Card : public View
{
public:
    explicit Card(double rowHeight)
        : m_rowHeight(rowHeight)
    {
    }

    double heightForWidth(double) const override
    {
        double height = 0;
        for (const auto& child : children()) {
            if (child->isVisible())
                height += m_rowHeight;
        }
        return height;
    }
    SizeF sizeHint() const override { return {0, heightForWidth(0)}; }

protected:
    void layout() override
    {
        double y = 0;
        for (const auto& child : children()) {
            if (!child->isVisible())
                continue;
            child->setGeometry({0, y, geometry().width, m_rowHeight});
            y += m_rowHeight;
        }
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        const Corners corners = theme.continuousCorners ? Corners::Continuous : Corners::Circular;
        painter.drawRoundedRect(localRect(), {theme.bigRadius, 0, corners}, theme.secondBackground);
        const View* last = nullptr;
        for (const auto& child : children()) {
            if (child->isVisible())
                last = child.get();
        }
        for (const auto& child : children()) {
            if (!child->isVisible() || child.get() == last)
                continue;
            const RectF row = child->geometry();
            views::paintSeparator(painter, 16, row.y + row.height - 1 / painter.scale(), row.width - 32,
                                  separatorColor());
        }
    }

private:
    double m_rowHeight;
};

// The label, then the value or controls.
class Row : public View
{
public:
    Row(std::string text, std::string value = {})
        : m_label(views::addLabel(this, std::move(text)))
        , m_value(views::addLabel(this, std::move(value), 0, Theme::current().disabledText))
    {
        m_label->setElide(Label::Elide::Right);
        m_value->setElide(Label::Elide::Right);
        m_value->setTextAlignment(Label::TextAlignment::Right);
        m_value->setVisible(!m_value->text().empty());
    }

    // Controls laid out after the label, right to left, with a fixed width
    // (0 for their own).
    template<typename T, typename... Args>
    T* addControl(double width, Args&&... args)
    {
        T* view = addChild<T>(std::forward<Args>(args)...);
        m_controls.push_back({view, width});
        return view;
    }
    Label* value() const { return m_value; }
    void setValueMaximumWidth(double width) { m_valueMaximum = width; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        double right = width - 16;
        for (auto it = m_controls.rbegin(); it != m_controls.rend(); ++it) {
            const SizeF size = it->view->sizeHint();
            const double w = it->width > 0 ? it->width : std::ceil(size.width);
            it->view->setGeometry({right - w, views::layoutCenter(height, size.height), w,
                                   size.height});
            right -= w + 12;
        }
        if (m_value->isVisible()) {
            const SizeF value = m_value->sizeHint();
            // Long values would otherwise squeeze the label out.
            const double maximum = m_valueMaximum > 0 ? m_valueMaximum : std::round(width * 0.65);
            const double w = std::min(std::ceil(value.width), maximum);
            m_value->setGeometry({right - w, views::layoutCenter(height, value.height), w,
                                  value.height});
            right -= w + 12;
        }
        const SizeF label = m_label->sizeHint();
        m_label->setGeometry({16, views::layoutCenter(height, label.height),
                              std::max(0.0, right + 12 - 12 - 16), label.height});
    }

private:
    struct Control
    {
        View* view;
        double width;
    };

    Label* m_label;
    Label* m_value;
    std::vector<Control> m_controls;
    double m_valueMaximum = 0;
};

// The dialogs' column: margins 16 at the sides, 4 above, rows 12 apart.
class Column : public View
{
public:
    explicit Column(double top = 4, double spacing = 12)
        : m_top(top)
        , m_spacing(spacing)
    {
    }

    Label* addSectionLabel(const std::string& text)
    {
        Label* label = views::addLabel(this, text, 0, Theme::current().disabledText);
        label->setElide(Label::Elide::Right);
        label->setName("sectionLabel");
        return label;
    }

    double heightForWidth(double width) const override
    {
        double height = m_top;
        bool first = true;
        for (const auto& child : children()) {
            if (!child->isVisible())
                continue;
            if (!first)
                height += m_spacing;
            first = false;
            height += std::ceil(childHeight(child.get(), width));
        }
        return height;
    }
    SizeF sizeHint() const override { return {0, heightForWidth(geometry().width)}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        double y = m_top;
        bool first = true;
        for (const auto& child : children()) {
            if (!child->isVisible())
                continue;
            if (!first)
                y += m_spacing;
            first = false;
            const double height = childHeight(child.get(), width);
            if (child->name() == "sectionLabel")
                child->setGeometry({16, y, std::max(0.0, width - 32), height});
            else
                child->setGeometry({0, y, width, height});
            y += std::ceil(height);
        }
    }

private:
    static double childHeight(const View* child, double width)
    {
        if (child->name() == "sectionLabel")
            return child->sizeHint().height;
        return child->heightForWidth(width);
    }

    double m_top;
    double m_spacing;
};

std::string number(const MpvNode& value)
{
    if (value.isInt())
        return std::to_string(value.toInt());
    if (value.isDouble())
        return base::strings::number(value.toDouble());
    return value.toString();
}

void closeOnEscape(DialogWindow* dialog)
{
    dialog->window()->addShortcut(NoModifier, key::Escape, [dialog] { dialog->close(); });
}

} // namespace

// A web address, filled in from the clipboard when it holds one.
class Dialogs::OpenUrl
{
public:
    explicit OpenUrl(MainWindow& owner)
        : m_owner(owner)
    {
    }

    void open()
    {
        if (m_window) {
            m_window->activate();
            return;
        }
        m_window = std::make_unique<Window>();
        m_window->setTitle("Open URL");
        m_window->setTransientParent(&m_owner.window(), true);
        m_window->setBackgroundColor(Theme::current().secondBackground);
        auto root = std::make_unique<Root>(*this);
        m_root = root.get();
        m_window->setContentView(std::move(root));
        m_window->resize({420, m_root->heightForWidth(420)});
        m_window->setMinimumSize({420, m_root->heightForWidth(420)});
        m_window->setMaximumSize({420, m_root->heightForWidth(420)});
        m_window->onCloseRequested = [this] { close(); };
        m_window->addShortcut(NoModifier, key::Escape, [this] { close(); });
        m_window->show();
        Clipboard::requestText([weak = std::weak_ptr<bool>(m_alive), this](const std::string& text) {
            if (weak.expired() || !m_root)
                return;
            m_root->field->setText(::Application::clipboardUrl(text));
            m_root->field->selectAll();
            m_root->update();
        });
        m_root->field->setFocus();
    }

    void close()
    {
        if (!m_window)
            return;
        // Not from inside the window's own event.
        base::EventLoop::current()->post([this, weak = std::weak_ptr<bool>(m_alive)] {
            if (weak.expired())
                return;
            m_root = nullptr;
            m_window.reset();
        });
        m_window->hide();
    }

private:
    class Root : public View
    {
    public:
        explicit Root(OpenUrl& dialog)
            : header(addChild<WindowHeader>(44))
            , title(views::addLabel(this, "Open URL", 0, std::nullopt, 600))
            , field(addChild<TextField>())
            , cancel(addChild<DialogButton>("Cancel"))
            , open(addChild<DialogButton>("Open", true))
        {
            title->setFontSize(14);
            field->setName("urlField");
            field->setPlaceholder("https://");
            field->setInputPurpose(TextPurpose::Url);
            header->setMinimizeVisible(false);
            header->setMaximizeVisible(false);
            header->setButtonMargin(8);
            cancel->onClicked = [&dialog] { dialog.close(); };
            open->onClicked = [&dialog] { dialog.accept(); };
            field->onTextChanged = [this] { update(); };
            field->onEditingFinished = [] {};
            field->onAccepted = [&dialog] { dialog.accept(); };
            update();
        }

        void update() { open->setEnabled(!::Application::urlFromInput(field->text()).empty()); }

        double heightForWidth(double width) const override
        {
            // The column, 16 above it and 12 below.
            return 16 + std::ceil(title->sizeHint().height) + 16 + std::ceil(field->heightForWidth(width - 32)) + 16
                    + std::ceil(open->sizeHint().height) + 12;
        }

        WindowHeader* header;
        Label* title;
        TextField* field;
        DialogButton* cancel;
        DialogButton* open;

    protected:
        void layout() override
        {
            const double width = geometry().width;
            double y = 16;
            const SizeF titleSize = title->sizeHint();
            title->setGeometry({16, y, std::min(std::ceil(titleSize.width), width - 32 - 32), titleSize.height});
            y += std::ceil(titleSize.height) + 16;
            const double fieldHeight = std::ceil(field->heightForWidth(width - 32));
            field->setGeometry({16, y, width - 32, fieldHeight});
            y += fieldHeight + 16;
            const SizeF openSize = open->sizeHint();
            const SizeF cancelSize = cancel->sizeHint();
            const double openWidth = std::ceil(openSize.width);
            const double cancelWidth = std::ceil(cancelSize.width);
            open->setGeometry({width - 16 - openWidth, y, openWidth, openSize.height});
            cancel->setGeometry({width - 16 - openWidth - 8 - cancelWidth, y, cancelWidth, cancelSize.height});
            header->setGeometry({0, 0, width, 44});
            raiseChild(header);
        }
    };

    void accept()
    {
        const std::string url = ::Application::urlFromInput(m_root ? m_root->field->text() : std::string());
        if (url.empty())
            return;
        m_owner.app().openUrls({url});
        close();
    }

    MainWindow& m_owner;
    std::unique_ptr<Window> m_window;
    Root* m_root = nullptr;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

class Dialogs::About
{
public:
    explicit About(MainWindow& owner)
        : m_owner(owner)
    {
    }

    void show()
    {
        if (m_window) {
            m_window->activate();
            return;
        }
        m_window = std::make_unique<Window>();
        // No visible title; the window carries the application id.
        m_window->setTitle("cutefish-video-player");
        m_window->setTransientParent(&m_owner.window(), true);
        // A translucent, blurred background.
        m_window->setBackgroundColor(Theme::current().background.withAlpha(0.8));
        auto root = std::make_unique<Root>();
        Root* rootView = root.get();
        m_window->setContentView(std::move(root));
        const double height = rootView->heightForWidth(300);
        m_window->resize({300, height});
        m_window->setMinimumSize({300, height});
        m_window->setMaximumSize({300, height});
        m_window->setBackgroundBlur({0, 0, 300, height}, Theme::current().windowRadius);
        m_window->onCloseRequested = [this] { close(); };
        m_window->addShortcut(NoModifier, key::Escape, [this] { close(); });
        m_window->show();
    }

    void close()
    {
        if (!m_window)
            return;
        m_window->hide();
        base::EventLoop::current()->post([this, weak = std::weak_ptr<bool>(m_alive)] {
            if (!weak.expired())
                m_window.reset();
        });
    }

private:
    static constexpr const char* kLink = "https://github.com/cutefishos/video-player";

    class Root : public View
    {
    public:
        Root()
            : m_header(addChild<WindowHeader>(40))
            , m_icon(addChild<ThemeIcon>("multimedia-video-player", 64))
            , m_name(views::addLabel(this, "Video Player", 14))
            , m_description(views::addLabel(this, "Plays videos with subtitles and playlists."))
            , m_link(addChild<views::Clickable>())
            , m_linkLabel(views::addLabel(m_link, {}))
        {
            m_header->setMinimizeVisible(false);
            m_header->setMaximizeVisible(false);
            m_linkLabel->setSpans({{kLink, 0, Theme::current().highlight, false, true, kLink}});
            m_link->setCursorShape(CursorShape::Pointer);
            m_link->onClicked = [] { base::Process::startDetached("xdg-open", {kLink}, {}); };
        }

        double heightForWidth(double) const override
        {
            // The column, then a header's height below it, under the header.
            return 40 + columnHeight() + 40;
        }

    protected:
        void layout() override
        {
            const double width = geometry().width;
            m_header->setGeometry({0, 0, width, 40});
            double y = 40;
            m_icon->setGeometry({rounding::centeredStart(width, 64), y, 64, 64});
            y += 64 + 5 + 12 + 5;
            for (Label* label : {m_name, m_description}) {
                const SizeF size = label->sizeHint();
                label->setGeometry({rounding::centeredStart(width, std::ceil(size.width)), y, std::ceil(size.width),
                                    size.height});
                y += std::ceil(size.height) + 5;
            }
            const SizeF link = m_linkLabel->sizeHint();
            m_link->setGeometry({rounding::centeredStart(width, std::ceil(link.width)), y, std::ceil(link.width),
                                 link.height});
            m_linkLabel->setGeometry({0, 0, std::ceil(link.width), link.height});
        }

    private:
        double columnHeight() const
        {
            return 64 + 5 + 12 + 5 + std::ceil(m_name->sizeHint().height) + 5
                    + std::ceil(m_description->sizeHint().height) + 5 + std::ceil(m_linkLabel->sizeHint().height) + 5;
        }

        WindowHeader* m_header;
        ThemeIcon* m_icon;
        Label* m_name;
        Label* m_description;
        views::Clickable* m_link;
        Label* m_linkLabel;
    };

    MainWindow& m_owner;
    std::unique_ptr<Window> m_window;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

Dialogs::Dialogs(MainWindow& owner)
    : m_owner(owner)
    , m_openUrl(std::make_unique<OpenUrl>(owner))
    , m_about(std::make_unique<About>(owner))
{
}

Dialogs::~Dialogs() = default;

void Dialogs::openUrl()
{
    m_openUrl->open();
}

void Dialogs::about()
{
    m_about->show();
}

void Dialogs::mediaInfo(const MpvNode::Map& details, const std::string& name)
{
    const MpvNode info(details);
    m_mediaInfo = std::make_unique<DialogWindow>(&m_owner.window(), "Media Info", 440);
    m_mediaInfo->window()->setBackgroundColor(Theme::current().background);
    closeOnEscape(m_mediaInfo.get());
    auto* column = m_mediaInfo->setContent<Column>();

    Label* title = views::addLabel(column, name, 12, std::nullopt, 500);
    title->setWrap(true);
    title->setWrapAnywhere(true);
    title->setMaximumLineCount(2);
    title->setName("mediaTitle");
    const auto value = [&info](const char* key) {
        const MpvNode& node = info[key];
        return node.isNone() ? std::string() : number(node);
    };
    const auto bitrate = [&info](const char* key) {
        const MpvNode& node = info[key];
        return node.isNone() || node.toDouble() == 0
                ? std::string()
                : std::format("{} kb/s", std::lround(node.toDouble() / 1000));
    };
    const auto addRow = [](Card* card, const char* text, const std::string& rowValue) {
        Row* row = card->addChild<Row>(text, rowValue);
        row->setVisible(!rowValue.empty());
    };

    auto* file = column->addChild<Card>(44);
    file->addChild<Row>("Location", value("path"));
    addRow(file, "Format", value("file-format"));
    addRow(file, "Size", info["file-size"].isNone() ? std::string()
                                                     : ::Application::formatBytes(info["file-size"].toInt()));
    addRow(file, "Duration", info["duration"].isNone() || info["duration"].toDouble() == 0
                                     ? std::string()
                                     : ::Application::formatTime(info["duration"].toDouble()));

    const bool video = !value("video-codec").empty();
    column->addSectionLabel("Video")->setVisible(video);
    auto* videoCard = column->addChild<Card>(44);
    videoCard->setVisible(video);
    addRow(videoCard, "Codec", value("video-codec"));
    addRow(videoCard, "Resolution",
           info["width"].isNone() || info["width"].toInt() == 0
                   ? std::string()
                   : value("width") + " × " + value("height"));
    addRow(videoCard, "Frame rate",
           info["container-fps"].isNone() || info["container-fps"].toDouble() == 0
                   ? std::string()
                   : std::format("{} fps", base::Locale::system().toString(info["container-fps"].toDouble(), 2)));
    addRow(videoCard, "Bit rate", bitrate("video-bitrate"));

    const bool audio = !value("audio-codec-name").empty();
    column->addSectionLabel("Audio")->setVisible(audio);
    auto* audioCard = column->addChild<Card>(44);
    audioCard->setVisible(audio);
    addRow(audioCard, "Codec", value("audio-codec-name"));
    addRow(audioCard, "Channels", value("audio-params/channel-count"));
    addRow(audioCard, "Sample rate", info["audio-params/samplerate"].isNone()
                                             ? std::string()
                                             : value("audio-params/samplerate") + " Hz");
    addRow(audioCard, "Bit rate", bitrate("audio-bitrate"));

    // The title row is padded as the cards' text.
    title->setName("sectionLabel");
    m_mediaInfo->show();
}

void Dialogs::preferences()
{
    PlayerSettings& settings = m_owner.app().settings();
    m_preferences = std::make_unique<DialogWindow>(&m_owner.window(), "Settings", 460);
    m_preferences->window()->setBackgroundColor(Theme::current().background);
    closeOnEscape(m_preferences.get());
    auto* column = m_preferences->setContent<Column>();

    static const std::vector<std::pair<std::string, std::string>> languages = {
        {"", "Display Language"}, {"en", "English"},   {"zh", "中文"},      {"ja", "日本語"},
        {"ko", "한국어"},          {"es", "Español"},   {"fr", "Français"}, {"de", "Deutsch"},
        {"it", "Italiano"},       {"pt", "Português"}, {"ru", "Русский"},
    };
    static const std::vector<std::pair<int, std::string>> sizes = {
        {80, "Small"}, {100, "Medium"}, {125, "Large"}, {150, "Extra Large"},
    };
    std::vector<std::string> languageNames;
    for (const auto& [code, name] : languages)
        languageNames.push_back(name);
    const auto languageIndex = [](const std::string& code) {
        for (size_t i = 0; i < languages.size(); ++i) {
            if (languages[i].first == code)
                return int(i);
        }
        return 0;
    };

    column->addSectionLabel("Playback");
    auto* playback = column->addChild<Card>(48);
    auto* resume = playback->addChild<Row>("Resume where you left off")->addControl<Switch>(0);
    resume->setName("resumeSwitch");
    resume->setPadding({0, 0, 0, 0});
    resume->setChecked(settings.resumePlayback());
    resume->onToggled = [&settings](bool checked) { settings.setResumePlayback(checked); };
    auto* folder = playback->addChild<Row>("Play the other videos in the folder")->addControl<Switch>(0);
    folder->setName("folderPlaylistSwitch");
    folder->setPadding({0, 0, 0, 0});
    folder->setChecked(settings.folderPlaylist());
    folder->onToggled = [&settings](bool checked) { settings.setFolderPlaylist(checked); };
    auto* audio = playback->addChild<Row>("Audio language")->addControl<ComboBox>(180, languageNames);
    audio->setCurrentIndex(languageIndex(settings.audioLanguage()));
    audio->onActivated = [&settings](int index) { settings.setAudioLanguage(languages[size_t(index)].first); };

    column->addSectionLabel("Subtitles");
    auto* subtitles = column->addChild<Card>(48);
    auto* subtitle = subtitles->addChild<Row>("Subtitle language")->addControl<ComboBox>(180, languageNames);
    subtitle->setCurrentIndex(languageIndex(settings.subtitleLanguage()));
    subtitle->onActivated = [&settings](int index) {
        settings.setSubtitleLanguage(languages[size_t(index)].first);
    };
    std::vector<std::string> sizeNames;
    int sizeIndex = 0;
    for (size_t i = 0; i < sizes.size(); ++i) {
        sizeNames.push_back(sizes[i].second);
        if (sizes[i].first == settings.subtitleScale())
            sizeIndex = int(i);
    }
    auto* size = subtitles->addChild<Row>("Subtitle size")->addControl<ComboBox>(180, sizeNames);
    size->setCurrentIndex(sizeIndex);
    size->onActivated = [&settings](int index) { settings.setSubtitleScale(sizes[size_t(index)].first); };

    column->addSectionLabel("Screenshots");
    auto* screenshots = column->addChild<Card>(48);
    Row* saveTo = screenshots->addChild<Row>("Save to",
                                             std::filesystem::path(settings.screenshotFolder()).filename().string());
    saveTo->value()->setElide(Label::Elide::Middle);
    saveTo->setValueMaximumWidth(200);
    auto* change = saveTo->addControl<Button>(0, "Change…");
    change->onClicked = [this, &settings] {
        m_folderChooser = std::make_unique<FileChooser>(m_preferences ? m_preferences->window() : &m_owner.window());
        m_folderChooser->setMode(FileChooser::Mode::OpenFolder);
        m_folderChooser->setTitle("Save Screenshots To");
        m_folderChooser->setCurrentFolder(settings.screenshotFolder());
        m_folderChooser->onAccepted = [this, &settings](const std::vector<std::string>& paths, const std::string&) {
            if (paths.empty())
                return;
            settings.setScreenshotFolder(paths.front());
            if (m_preferences) {
                // Reopened in place, showing the new folder.
                preferences();
            }
        };
        m_folderChooser->open();
    };
    m_preferences->show();
}

void Dialogs::shortcuts()
{
    m_shortcuts = std::make_unique<DialogWindow>(&m_owner.window(), "Keyboard Shortcuts", 480);
    m_shortcuts->window()->setBackgroundColor(Theme::current().background);
    closeOnEscape(m_shortcuts.get());

    struct Section
    {
        const char* title;
        std::vector<std::pair<const char*, const char*>> keys;
    };
    static const std::vector<Section> sections = {
        {"Playback", {
            {"Play or pause", "Space, K"},
            {"Back or forward 5 seconds", "← →"},
            {"Back or forward 30 seconds", "Shift+← →"},
            {"Go to the start or the end", "Home, End"},
            {"Slower, faster, normal speed", "[ ] Backspace"},
            {"Previous or next frame", ", ."},
            {"Previous or next chapter", "Page Up, Page Down"},
            {"Previous or next file", "B, N"},
        }},
        {"Sound and Subtitles", {
            {"Volume up or down", "↑ ↓"},
            {"Mute", "M"},
            {"Next audio track", "A"},
            {"Next or previous subtitle", "J, Shift+J"},
            {"Show or hide subtitles", "V"},
            {"Subtitles earlier or later", "Z, Shift+Z"},
        }},
        {"Window", {
            {"Full screen", "F, F11, double-click"},
            {"Leave full screen", "Esc"},
            {"Playlist", "P"},
            {"Open a file", "Ctrl+O"},
            {"Open a URL", "Ctrl+L"},
            {"Take a screenshot", "Ctrl+Alt+S"},
            {"Media info", "Ctrl+I"},
            {"Settings", "Ctrl+,"},
            {"Keyboard shortcuts", "Ctrl+?"},
            {"Close", "Ctrl+W"},
        }},
    };

    // 600 tall in all; the list scrolls inside.
    class Scroller : public ScrollView
    {
    public:
        double heightForWidth(double) const override { return 600 - 44 - 16; }
    };
    auto* scroller = m_shortcuts->setContent<Scroller>();
    scroller->setScrollDirections(false, true);
    scroller->setClipsChildren(true);
    auto* column = scroller->setContent<Column>(4, 12);
    for (const Section& section : sections) {
        column->addSectionLabel(section.title);
        auto* card = column->addChild<Card>(44);
        for (const auto& [text, keys] : section.keys)
            card->addChild<Row>(text, keys);
    }
    m_shortcuts->show();
}
