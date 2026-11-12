#!/usr/bin/env python3
"""Generate the STL Viewer app icon: a faceted low-poly sphere."""

import math

from PIL import Image, ImageDraw, ImageFilter

S = 1024
OUT = "packaging/applaunch/share/images/stlviewer.png"


def lerp_color(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def icosphere(subdivisions=1):
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [
        (-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
        (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
        (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1),
    ]
    faces = [
        (0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
        (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
        (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
        (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1),
    ]

    def normalize(v):
        length = math.sqrt(sum(c * c for c in v))
        return tuple(c / length for c in v)

    verts = [normalize(v) for v in verts]
    for _ in range(subdivisions):
        cache = {}
        new_faces = []

        def midpoint(i, j):
            key = (min(i, j), max(i, j))
            if key in cache:
                return cache[key]
            a, b = verts[i], verts[j]
            m = normalize(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2,
                           (a[2] + b[2]) / 2))
            verts.append(m)
            cache[key] = len(verts) - 1
            return cache[key]

        for a, b, c in faces:
            ab = midpoint(a, b)
            bc = midpoint(b, c)
            ca = midpoint(c, a)
            new_faces += [(a, ab, ca), (b, bc, ab), (c, ca, bc),
                          (ab, bc, ca)]
        faces = new_faces

    return verts, faces


def rotate(v, yaw, pitch):
    x, y, z = v
    cp, sp = math.cos(pitch), math.sin(pitch)
    cy, sy = math.cos(yaw), math.sin(yaw)
    y1 = y * cp - z * sp
    z1 = y * sp + z * cp
    return (x * cy + z1 * sy, y1, -x * sy + z1 * cy)


def main():
    # Background gradient.
    img = Image.new("RGB", (S, S))
    draw = ImageDraw.Draw(img)
    top = (7, 12, 24)
    bottom = (14, 24, 42)
    for y in range(S):
        draw.line([(0, y), (S, y)],
                  fill=lerp_color(top, bottom, y / (S - 1)))

    # Cyan glow behind the model.
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([S // 2 - 360, S // 2 - 360, S // 2 + 360, S // 2 + 360],
               fill=(20, 150, 190, 150))
    glow = glow.filter(ImageFilter.GaussianBlur(150))
    img = Image.alpha_composite(img.convert("RGBA"), glow).convert("RGB")
    draw = ImageDraw.Draw(img)

    # Faint technical grid.
    for i in range(0, S, 64):
        color = (18, 30, 50)
        draw.line([(i, 0), (i, S)], fill=color, width=2)
        draw.line([(0, i), (S, i)], fill=color, width=2)

    # Projected low-poly sphere.
    verts, faces = icosphere(1)
    yaw = math.radians(-24)
    pitch = math.radians(18)
    radius = 320.0
    cx, cy = S // 2, S // 2 + 10

    projected = []
    for v in verts:
        x, y, z = rotate(v, yaw, pitch)
        projected.append((cx + x * radius, cy - y * radius, z))

    light = (-0.35, 0.55, 0.75)
    light_len = math.sqrt(sum(c * c for c in light))
    light = tuple(c / light_len for c in light)

    triangles = []
    for face in faces:
        a, b, c = (projected[i] for i in face)
        ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        nx = ab[1] * ac[2] - ab[2] * ac[1]
        ny = ab[2] * ac[0] - ab[0] * ac[2]
        nz = ab[0] * ac[1] - ab[1] * ac[0]
        length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        n = (nx / length, ny / length, nz / length)
        diffuse = max(0.0, n[0] * light[0] + n[1] * light[1] +
                      n[2] * light[2])
        rim = (1.0 - abs(n[2])) ** 2
        shade = 0.16 + 0.80 * diffuse + 0.08 * rim
        triangles.append(((a[2] + b[2] + c[2]) / 3.0, a, b, c, shade))

    # Painter's algorithm: far faces first.
    triangles.sort(key=lambda item: item[0])

    dark = (12, 58, 78)
    bright = (95, 200, 225)
    edge = (125, 238, 252)

    for _, a, b, c, shade in triangles:
        color = lerp_color(dark, bright, shade)
        points = [(a[0], a[1]), (b[0], b[1]), (c[0], c[1])]
        draw.polygon(points, fill=color)
        draw.line(points + [points[0]], fill=edge, width=4, joint="curve")

    # Small XYZ gizmo in the lower-right corner.
    ox, oy = 820, 840
    axis_len = 82
    draw.line([(ox, oy), (ox + axis_len, oy)], fill=(255, 70, 70), width=11)
    draw.line([(ox, oy), (ox, oy - axis_len)], fill=(70, 235, 120), width=11)
    draw.line([(ox, oy), (ox - axis_len * 0.6, oy + axis_len * 0.55)],
              fill=(90, 150, 255), width=11)
    draw.ellipse([ox - 7, oy - 7, ox + 7, oy + 7], fill=(220, 235, 245))

    img = img.resize((256, 256), Image.LANCZOS)
    img.save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
