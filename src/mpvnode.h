// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QVariant>

#include <mpv/client.h>

// Conversions between QVariant and mpv_node for the node-based mpv API.
namespace MpvNode
{
// Fills node from value; the node owns copies of every string and list and is
// freed with release(), never with mpv_free_node_contents().
void fromVariant(mpv_node *node, const QVariant &value);
void release(mpv_node *node);

QVariant toVariant(const mpv_node *node);
}
