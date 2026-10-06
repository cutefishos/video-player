// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "playlist.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

static const QSet<QString> &mediaSuffixes()
{
    static const QSet<QString> suffixes = {
        // Video
        QStringLiteral("3g2"), QStringLiteral("3gp"), QStringLiteral("asf"), QStringLiteral("avi"),
        QStringLiteral("divx"), QStringLiteral("f4v"), QStringLiteral("flv"), QStringLiteral("m2ts"),
        QStringLiteral("m4v"), QStringLiteral("mkv"), QStringLiteral("mov"), QStringLiteral("mp4"),
        QStringLiteral("mpeg"), QStringLiteral("mpg"), QStringLiteral("mts"), QStringLiteral("ogv"),
        QStringLiteral("rm"), QStringLiteral("rmvb"), QStringLiteral("ts"), QStringLiteral("vob"),
        QStringLiteral("webm"), QStringLiteral("wmv"),
        // Audio
        QStringLiteral("aac"), QStringLiteral("ape"), QStringLiteral("flac"), QStringLiteral("m4a"),
        QStringLiteral("mka"), QStringLiteral("mp3"), QStringLiteral("oga"), QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("wav"), QStringLiteral("wma"), QStringLiteral("wv"),
    };
    return suffixes;
}

Playlist::Playlist(QObject *parent)
    : QAbstractListModel(parent)
{
}

int Playlist::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_urls.size();
}

QVariant Playlist::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_urls.size())
        return {};
    const QUrl &url = m_urls.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return titleFor(url);
    case UrlRole:
        return url;
    case CurrentRole:
        return index.row() == m_current;
    default:
        return {};
    }
}

QHash<int, QByteArray> Playlist::roleNames() const
{
    return {
        {UrlRole, "url"},
        {TitleRole, "title"},
        {CurrentRole, "current"},
    };
}

void Playlist::setCurrentIndex(int index)
{
    if (index < -1 || index >= m_urls.size() || index == m_current)
        return;
    const int previous = m_current;
    m_current = index;
    if (previous >= 0)
        Q_EMIT dataChanged(this->index(previous), this->index(previous), {CurrentRole});
    if (index >= 0)
        Q_EMIT dataChanged(this->index(index), this->index(index), {CurrentRole});
    Q_EMIT currentIndexChanged();
    Q_EMIT navigationChanged();
}

QUrl Playlist::currentUrl() const
{
    return m_current >= 0 ? m_urls.at(m_current) : QUrl();
}

void Playlist::setRepeatMode(RepeatMode mode)
{
    if (m_repeatMode == mode)
        return;
    m_repeatMode = mode;
    Q_EMIT repeatModeChanged();
    Q_EMIT navigationChanged();
}

void Playlist::open(const QList<QUrl> &urls, bool includeFolder)
{
    QList<QUrl> queue;
    QUrl first;
    if (includeFolder && urls.size() == 1 && urls.first().isLocalFile()) {
        first = QUrl::fromLocalFile(QFileInfo(urls.first().toLocalFile()).absoluteFilePath());
        queue = folderMedia(first.toLocalFile());
        if (!queue.contains(first))
            queue = {first};
    } else {
        for (const QUrl &url : urls) {
            if (url.isValid() && !queue.contains(url))
                queue.append(url);
        }
        if (!queue.isEmpty())
            first = queue.first();
    }

    beginResetModel();
    m_urls = queue;
    m_current = first.isEmpty() ? -1 : m_urls.indexOf(first);
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT currentIndexChanged();
    Q_EMIT navigationChanged();
}

void Playlist::append(const QList<QUrl> &urls)
{
    QList<QUrl> added;
    for (const QUrl &url : urls) {
        if (url.isValid() && !m_urls.contains(url) && !added.contains(url))
            added.append(url);
    }
    if (added.isEmpty())
        return;

    beginInsertRows({}, m_urls.size(), m_urls.size() + added.size() - 1);
    for (const QUrl &url : added) {
        m_urls.append(url);
    }
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT navigationChanged();
}

void Playlist::remove(int index)
{
    if (index < 0 || index >= m_urls.size())
        return;

    beginRemoveRows({}, index, index);
    m_urls.removeAt(index);
    const bool currentChanged = index <= m_current;
    if (index < m_current)
        --m_current;
    else if (index == m_current)
        m_current = -1;
    endRemoveRows();

    Q_EMIT countChanged();
    if (currentChanged)
        Q_EMIT currentIndexChanged();
    Q_EMIT navigationChanged();
}

void Playlist::move(int from, int to)
{
    if (from < 0 || from >= m_urls.size() || to < 0 || to >= m_urls.size() || from == to)
        return;

    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_urls.move(from, to);
    int current = m_current;
    if (current == from)
        current = to;
    else if (from < current && current <= to)
        --current;
    else if (to <= current && current < from)
        ++current;
    const bool currentChanged = current != m_current;
    m_current = current;
    endMoveRows();

    if (currentChanged)
        Q_EMIT currentIndexChanged();
    Q_EMIT navigationChanged();
}

void Playlist::clear()
{
    if (m_urls.isEmpty())
        return;
    beginResetModel();
    m_urls.clear();
    m_current = -1;
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT currentIndexChanged();
    Q_EMIT navigationChanged();
}

int Playlist::nextIndex(bool automatic) const
{
    if (m_urls.isEmpty())
        return -1;
    if (automatic && m_repeatMode == RepeatOne && m_current >= 0)
        return m_current;
    if (m_current + 1 < m_urls.size())
        return m_current + 1;
    return m_repeatMode == RepeatAll ? 0 : -1;
}

int Playlist::previousIndex() const
{
    if (m_current < 0)
        return -1;
    if (m_current > 0)
        return m_current - 1;
    return m_repeatMode == RepeatAll && m_urls.size() > 1 ? m_urls.size() - 1 : -1;
}

bool Playlist::next(bool automatic)
{
    const int index = nextIndex(automatic);
    if (index < 0)
        return false;
    if (index == m_current)
        Q_EMIT currentIndexChanged();
    else
        setCurrentIndex(index);
    return true;
}

bool Playlist::previous()
{
    const int index = previousIndex();
    if (index < 0)
        return false;
    if (index == m_current)
        Q_EMIT currentIndexChanged();
    else
        setCurrentIndex(index);
    return true;
}

bool Playlist::isMediaFile(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (!mediaSuffixes().contains(suffix))
        return false;
    // .ts is also a Qt translation, which is XML; a transport stream is binary.
    if (suffix == QLatin1String("ts")) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly))
            return !file.read(64).trimmed().startsWith('<');
    }
    return true;
}

bool Playlist::isSubtitleFile(const QString &fileName)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("ass"), QStringLiteral("idx"), QStringLiteral("smi"), QStringLiteral("srt"),
        QStringLiteral("ssa"), QStringLiteral("sub"), QStringLiteral("sup"), QStringLiteral("vtt"),
    };
    return suffixes.contains(QFileInfo(fileName).suffix().toLower());
}

QList<QUrl> Playlist::folderMedia(const QString &file)
{
    const QFileInfo info(file);
    QStringList names;
    const QDir folder = info.absoluteDir();
    const QStringList entries = folder.entryList(QDir::Files | QDir::Readable);
    for (const QString &name : entries) {
        if (isMediaFile(folder.filePath(name)) || name == info.fileName())
            names.append(name);
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), collator);

    QList<QUrl> urls;
    for (const QString &name : std::as_const(names))
        urls.append(QUrl::fromLocalFile(folder.absoluteFilePath(name)));
    return urls;
}

QString Playlist::titleFor(const QUrl &url)
{
    if (url.isLocalFile())
        return QFileInfo(url.toLocalFile()).fileName();
    const QString name = url.fileName();
    return name.isEmpty() ? url.toDisplayString() : name;
}
