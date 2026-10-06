// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#version 440

// The frosted plate: the video, small and prefiltered, blurred once more over
// a wide Gaussian here, desaturated and darkened toward the tint, and cut to
// FishUI's rounded rectangle (continuous or circular corners) with a hairline.
// The shape functions follow fishui/src/shaders/roundedrectangle.frag; keep
// them in step with it.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 tint;
    vec4 hairline;
    vec2 plateSize;
    vec2 spread;
    float radius;
    float desaturation;
    float ratio;
    float continuous;
};

layout(binding = 1) uniform sampler2D source;

// All shape math runs in device pixels.
vec2 size;
float deviceRadius;

vec2 cornerExtent(vec2 halfSize)
{
    if (continuous > 0.5)
        return clamp(vec2(deviceRadius * 1.5), vec2(0.0), halfSize);
    return vec2(clamp(deviceRadius, 0.0, min(halfSize.x, halfSize.y)));
}

vec2 cornerNormal(vec2 corner, vec2 r)
{
    if (continuous > 0.5)
        return normalize((corner / r) * (corner / r) / r);
    return normalize(corner);
}

float roundedDistance(vec2 p)
{
    vec2 halfSize = size * 0.5;
    vec2 r = cornerExtent(halfSize);
    vec2 edge = abs(p - halfSize) - halfSize;
    vec2 d = edge + r;
    if (min(r.x, r.y) <= 0.0 || min(d.x, d.y) <= 0.0)
        return max(edge.x, edge.y);
    if (continuous < 0.5)
        return length(d) - r.x;
    vec2 q = d / r;
    float l = pow(dot(q * q, q), 1.0 / 3.0);
    return (l - 1.0) * l * l / length(q * q / r);
}

float tangentCoverage(vec2 p, float inset, float pixelSize)
{
    vec2 halfSize = size * 0.5;
    vec2 r = cornerExtent(halfSize);
    vec2 edge = abs(p - halfSize) - halfSize;
    vec2 corner = edge + r;
    vec2 normal = edge.x > edge.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    if (min(r.x, r.y) > 0.0 && min(corner.x, corner.y) > 0.0)
        normal = cornerNormal(corner, r);
    float t = -roundedDistance(p) - inset;
    vec2 extent = abs(normal) * (0.5 * pixelSize);
    float a = max(extent.x, extent.y);
    float b = min(extent.x, extent.y);
    if (b < 0.001)
        return clamp(0.5 + t / (2.0 * a), 0.0, 1.0);
    float u = abs(t);
    if (u >= a + b)
        return t > 0.0 ? 1.0 : 0.0;
    if (u <= a - b)
        return 0.5 + t / (2.0 * a);
    float tail = (a + b - u) * (a + b - u) / (8.0 * a * b);
    return t > 0.0 ? 1.0 - tail : tail;
}

float roundedCoverage(vec2 p, float inset)
{
    float distance = roundedDistance(p) + inset;
    if (distance < -1.0) return 1.0;
    if (distance > 1.0) return 0.0;
    vec2 halfSize = size * 0.5;
    vec2 r = cornerExtent(halfSize);
    vec2 corner = abs(p - halfSize) - halfSize + r;
    if (min(r.x, r.y) <= 0.0 || min(corner.x, corner.y) < -0.5)
        return tangentCoverage(p, inset, 1.0);
    return 0.25 * (
        tangentCoverage(p + vec2(-0.25, -0.25), inset, 0.5) +
        tangentCoverage(p + vec2( 0.25, -0.25), inset, 0.5) +
        tangentCoverage(p + vec2(-0.25,  0.25), inset, 0.5) +
        tangentCoverage(p + vec2( 0.25,  0.25), inset, 0.5));
}

void main()
{
    size = plateSize * ratio;
    deviceRadius = radius * ratio;

    // 7x7 taps over +-spread, sigma half of it; bilinear sampling fills the gaps.
    vec3 color = vec3(0.0);
    float total = 0.0;
    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            vec2 offset = vec2(float(x), float(y)) / 3.0;
            float weight = exp(-2.0 * dot(offset, offset));
            color += texture(source, qt_TexCoord0 + offset * spread).rgb * weight;
            total += weight;
        }
    }
    color /= total;
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(color, vec3(luma), desaturation);
    // Colours arrive premultiplied.
    color = mix(color, tint.rgb / max(tint.a, 1.0 / 256.0), tint.a);

    // The hairline is the outermost device pixel, as RoundedRectangle draws a border.
    vec2 p = qt_TexCoord0 * size;
    float outer = roundedCoverage(p, 0.0);
    float inner = roundedCoverage(p, 1.0);
    fragColor = (vec4(color, 1.0) * inner + hairline * (outer - inner)) * qt_Opacity;
}
