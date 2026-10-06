// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

// One device pixel at any scale; RoundedRectangle snaps it to the pixel grid.
FishUI.RoundedRectangle {
    height: 1 / FishUI.Dpi.ratio
    color: FishUI.Theme.disabledTextColor
    opacity: FishUI.Theme.darkMode ? 0.3 : 0.1
}
