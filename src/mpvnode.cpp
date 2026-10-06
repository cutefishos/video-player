// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvnode.h"

#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <cstring>

static char *copyString(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    char *copy = new char[utf8.size() + 1];
    std::memcpy(copy, utf8.constData(), utf8.size() + 1);
    return copy;
}

static mpv_node_list *createList(mpv_node *node, bool map, int count)
{
    node->format = map ? MPV_FORMAT_NODE_MAP : MPV_FORMAT_NODE_ARRAY;
    auto *list = new mpv_node_list;
    list->num = count;
    list->values = new mpv_node[count]();
    list->keys = map ? new char *[count]() : nullptr;
    node->u.list = list;
    return list;
}

namespace MpvNode
{

void fromVariant(mpv_node *node, const QVariant &value)
{
    switch (value.typeId()) {
    case QMetaType::Bool:
        node->format = MPV_FORMAT_FLAG;
        node->u.flag = value.toBool() ? 1 : 0;
        return;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        node->format = MPV_FORMAT_INT64;
        node->u.int64 = value.toLongLong();
        return;
    case QMetaType::Double:
    case QMetaType::Float:
        node->format = MPV_FORMAT_DOUBLE;
        node->u.double_ = value.toDouble();
        return;
    case QMetaType::QString:
    case QMetaType::QByteArray:
    case QMetaType::QUrl:
        node->format = MPV_FORMAT_STRING;
        node->u.string = copyString(value.typeId() == QMetaType::QUrl ? value.toUrl().toString(QUrl::FullyEncoded) : value.toString());
        return;
    case QMetaType::QStringList:
    case QMetaType::QVariantList: {
        const QVariantList items = value.toList();
        mpv_node_list *list = createList(node, false, items.size());
        for (int i = 0; i < items.size(); ++i)
            fromVariant(&list->values[i], items.at(i));
        return;
    }
    case QMetaType::QVariantMap: {
        const QVariantMap items = value.toMap();
        mpv_node_list *list = createList(node, true, items.size());
        int i = 0;
        for (auto it = items.cbegin(); it != items.cend(); ++it, ++i) {
            list->keys[i] = copyString(it.key());
            fromVariant(&list->values[i], it.value());
        }
        return;
    }
    default:
        node->format = MPV_FORMAT_NONE;
        return;
    }
}

void release(mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
        delete[] node->u.string;
        break;
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP: {
        mpv_node_list *list = node->u.list;
        for (int i = 0; i < list->num; ++i) {
            if (list->keys)
                delete[] list->keys[i];
            release(&list->values[i]);
        }
        delete[] list->keys;
        delete[] list->values;
        delete list;
        break;
    }
    default:
        break;
    }
    node->format = MPV_FORMAT_NONE;
}

QVariant toVariant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
    case MPV_FORMAT_OSD_STRING:
        return QString::fromUtf8(node->u.string);
    case MPV_FORMAT_FLAG:
        return bool(node->u.flag);
    case MPV_FORMAT_INT64:
        return qlonglong(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList items;
        for (int i = 0; i < node->u.list->num; ++i)
            items.append(toVariant(&node->u.list->values[i]));
        return items;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap items;
        for (int i = 0; i < node->u.list->num; ++i)
            items.insert(QString::fromUtf8(node->u.list->keys[i]), toVariant(&node->u.list->values[i]));
        return items;
    }
    default:
        return {};
    }
}

}
