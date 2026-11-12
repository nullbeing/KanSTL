#!/usr/bin/env python3
"""Generate the STL Viewer app icon: a triangulated isometric cube."""

from PIL import Image, ImageDraw, ImageFilter

S = 1024
OUT = "packaging/applaunch/share/images/stlviewer.png"


def lerp_color(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def main():
    # Background gradient.
    img = Image.new("RGB", (S, S))
    draw = ImageDraw.Draw(img)
    top = (7, 12, 24)
    bottom = (14, 24, 42)
    for y in range(S):
        draw.line([(0, y), (S, y)],
                  fill=lerp_color(top, bottom, y / (S - 1)))

    # Soft cyan glow behind the cube.
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([120, 120, 900, 900], fill=(20, 150, 190, 95))
    glow = glow.filter(ImageFilter.GaussianBlur(150))
    img = Image.alpha_composite(img.convert("RGBA"), glow).convert("RGB")
    draw = ImageDraw.Draw(img)

    # Faint technical grid.
    for i in range(0, S, 128):
        color = (17, 28, 47)
        draw.line([(i, 0), (i, S)], fill=color, width=2)
        draw.line([(0, i), (S, i)], fill=color, width=2)

    # Isometric cube geometry.
    cx, cy = 440, 560
    half = 250
    dx, dy = 150, -110

    f0 = (cx - half, cy - half)   # front top-left
    f1 = (cx + half, cy - half)   # front top-right
    f2 = (cx + half, cy + half)   # front bottom-right
    f3 = (cx - half, cy + half)   # front bottom-left
    b0 = (f0[0] + dx, f0[1] + dy)
    b1 = (f1[0] + dx, f1[1] + dy)
    b2 = (f2[0] + dx, f2[1] + dy)

    outer = (125, 240, 255)
    inner = (72, 180, 212)

    # Face fills: top, right, front (back faces are hidden).
    top_tris = [
        ((f0, b0, b1), (39, 132, 162)),
        ((f0, b1, f1), (33, 116, 146)),
    ]
    right_tris = [
        ((f1, b1, b2), (24, 92, 120)),
        ((f1, b2, f2), (20, 80, 106)),
    ]
    front_tris = [
        ((f0, f1, f2), (15, 64, 86)),
        ((f0, f2, f3), (12, 56, 76)),
    ]

    for triangles in (top_tris, right_tris, front_tris):
        for points, color in triangles:
            draw.polygon(points, fill=color)

    # Triangulation edges.
    for triangles in (top_tris, right_tris, front_tris):
        for points, _ in triangles:
            draw.line(list(points) + [points[0]], fill=inner, width=6,
                      joint="curve")

    # Strong outer silhouette.
    for quad in ((f0, b0, b1, f1), (f1, b1, b2, f2),
                 (f0, f1, f2, f3)):
        draw.line(list(quad) + [quad[0]], fill=outer, width=11,
                  joint="curve")

    # Small RGB axis triad in the lower-right corner.
    ox, oy = 872, 872
    axis_len = 92
    draw.line([(ox, oy), (ox + axis_len, oy)], fill=(255, 78, 78), width=12)
    draw.line([(ox, oy), (ox, oy - axis_len)], fill=(70, 235, 120), width=12)
    draw.line([(ox, oy), (ox - axis_len * 0.58, oy + axis_len * 0.55)],
              fill=(90, 155, 255), width=12)
    draw.ellipse([ox - 9, oy - 9, ox + 9, oy + 9], fill=(225, 238, 246))

    img = img.resize((256, 256), Image.LANCZOS)
    img.save(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
