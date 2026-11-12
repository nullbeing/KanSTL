#!/usr/bin/env python3
"""Generate the STL Viewer app icon (wireframe cube on a dark background)."""

from PIL import Image, ImageDraw

S = 1024
img = Image.new("RGBA", (S, S), (10, 16, 30, 255))
d = ImageDraw.Draw(img)

# Subtle background grid.
for i in range(0, S, 128):
    d.line([(i, 0), (i, S)], fill=(18, 28, 48, 255), width=2)
    d.line([(0, i), (S, i)], fill=(18, 28, 48, 255), width=2)

cx, cy = S // 2, S // 2 + 10
half = 250
dx, dy = 130, -95

front = [
    (cx - half, cy - half),
    (cx + half, cy - half),
    (cx + half, cy + half),
    (cx - half, cy + half),
]
back = [(x + dx, y + dy) for x, y in front]

cyan = (79, 207, 255, 255)
dim = (30, 120, 160, 255)
width = 14

# Back face (dimmer), then connecting edges, then front face.
d.line(back + [back[0]], fill=dim, width=width, joint="curve")
for a, b in zip(front, back):
    d.line([a, b], fill=dim, width=width)
d.line(front + [front[0]], fill=cyan, width=width, joint="curve")

# Small accent triangle on the front face.
d.line([(cx - 60, cy + 70), (cx + 70, cy - 40)], fill=cyan, width=10)
d.line([(cx + 70, cy - 40), (cx + 70, cy + 90)], fill=cyan, width=10)
d.line([(cx + 70, cy + 90), (cx - 60, cy + 70)], fill=cyan, width=10)

img = img.resize((256, 256), Image.LANCZOS)
img.save("packaging/applaunch/share/images/stlviewer.png")
print("packaging/applaunch/share/images/stlviewer.png")
