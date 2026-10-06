// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tracks.h"

#include <QCoreApplication>
#include <QLocale>

static QString languageName(const QString &code)
{
    if (code.isEmpty())
        return {};
    const QLocale::Language language = QLocale::codeToLanguage(code, QLocale::AnyLanguageCode);
    if (language == QLocale::AnyLanguage)
        return code;
    // English's native name is "American English"; the language alone is meant.
    if (language == QLocale::English)
        return QLocale::languageToString(language);
    const QString name = QLocale(language).nativeLanguageName();
    return name.isEmpty() ? QLocale::languageToString(language) : name;
}

namespace Tracks
{

QString label(const QVariantMap &track)
{
    const QString title = track.value(QStringLiteral("title")).toString().trimmed();
    const QString language = languageName(track.value(QStringLiteral("lang")).toString());
    if (!title.isEmpty() && !language.isEmpty() && title.compare(language, Qt::CaseInsensitive) != 0)
        return QStringLiteral("%1 (%2)").arg(title, language);
    if (!title.isEmpty())
        return title;
    if (!language.isEmpty())
        return language;
    return QCoreApplication::translate("Tracks", "Track %1").arg(track.value(QStringLiteral("id")).toInt());
}

Split split(const QVariantList &trackList)
{
    Split result;
    for (const QVariant &entry : trackList) {
        const QVariantMap track = entry.toMap();
        const QString type = track.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("video")) {
            if (!track.value(QStringLiteral("albumart")).toBool())
                result.hasVideo = true;
            continue;
        }

        QVariantMap choice;
        choice.insert(QStringLiteral("id"), track.value(QStringLiteral("id")).toInt());
        choice.insert(QStringLiteral("label"), label(track));
        choice.insert(QStringLiteral("selected"), track.value(QStringLiteral("selected")).toBool());
        choice.insert(QStringLiteral("external"), track.value(QStringLiteral("external")).toBool());
        if (type == QLatin1String("audio"))
            result.audio.append(choice);
        else if (type == QLatin1String("sub"))
            result.subtitle.append(choice);
    }
    return result;
}

}
