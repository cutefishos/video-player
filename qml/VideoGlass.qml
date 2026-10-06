// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

// The plate of the overlay controls: frosted glass made of the video behind
// it. The video under the plate is taken at a quarter of its resolution,
// blurred until no edge survives, then desaturated and darkened so white text
// keeps at least 4.5:1 contrast even over a white picture. Without a GPU scene
// graph it is a plain dark plate.
Item {
    id: glass

    // The item drawn behind the plate, usually the video.
    property Item source
    property real radius: 20
    readonly property color hairline: Qt.rgba(1, 1, 1, 0.12)
    readonly property bool blurred: source !== null && GraphicsInfo.api !== GraphicsInfo.Software
                                    && GraphicsInfo.api !== GraphicsInfo.Unknown
    // Over white, 62% of this tint leaves a grey of about 0.16 luminance:
    // 5:1 against white text.
    readonly property color tint: Qt.rgba(0.07, 0.07, 0.08, 0.62)

    ShaderEffectSource {
        id: capture
        visible: false
        sourceItem: glass.blurred ? glass.source : null
        live: true
        hideSource: false
        smooth: true
        // A quarter of the device resolution: the blur needs no more.
        textureSize: Qt.size(Math.max(1, Math.ceil(glass.width * FishUI.Dpi.ratio / 4)),
                             Math.max(1, Math.ceil(glass.height * FishUI.Dpi.ratio / 4)))
        sourceRect: {
            if (!glass.source)
                return Qt.rect(0, 0, 0, 0)
            // Dependencies: the plate's own geometry and its parent's.
            glass.x; glass.y; glass.width; glass.height
            glass.parent.x; glass.parent.y
            return glass.mapToItem(glass.source, 0, 0, glass.width, glass.height)
        }
    }

    // The capture shrunk to an eighth and smoothed there; the glass shader
    // stretches it back and blurs it across about 64 px.
    Item {
        id: small
        visible: false
        width: Math.max(4, Math.ceil(glass.width / 8))
        height: Math.max(4, Math.ceil(glass.height / 8))

        ShaderEffect {
            anchors.fill: parent
            property var source: capture
        }
    }

    FishUI.FastBlur {
        id: blur
        width: small.width
        height: small.height
        visible: false
        source: glass.blurred ? small : null
        radius: 8
    }

    ShaderEffectSource {
        id: blurTexture
        visible: false
        sourceItem: glass.blurred ? blur : null
        hideSource: true
        smooth: true
    }

    ShaderEffect {
        anchors.fill: parent
        visible: glass.blurred
        property var source: blurTexture
        property color tint: glass.tint
        property color hairline: glass.hairline
        property size plateSize: Qt.size(width, height)
        // The reach of the blur: 64 px each way.
        property size spread: Qt.size(64 / Math.max(1, width), 64 / Math.max(1, height))
        property real radius: glass.radius
        property real desaturation: 0.35
        property real ratio: FishUI.Dpi.ratio
        property real continuous: FishUI.Theme.continuousCorners ? 1 : 0
        fragmentShader: "qrc:/shaders/glass.frag.qsb"
    }

    // Without the shader: the same shape, solid.
    FishUI.RoundedRectangle {
        anchors.fill: parent
        visible: !glass.blurred
        radius: glass.radius
        color: Qt.rgba(0.1, 0.1, 0.11, 0.85)
        border.width: 1 / FishUI.Dpi.ratio
        border.color: glass.hairline
    }
}
