// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string_view>
#include <utility>
#include <vector>

// The application's own glyphs, by name, as SVG.
const std::vector<std::pair<std::string_view, std::string_view>>& embeddedIcons();
