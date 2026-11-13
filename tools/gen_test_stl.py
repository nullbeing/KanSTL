#!/usr/bin/env python3
"""Generate simple binary STL test models without external dependencies."""

import argparse
import math
import struct


def write_binary_stl(path, triangles):
    with open(path, "wb") as f:
        f.write(b"kanstl test model".ljust(80, b"\0"))
        f.write(struct.pack("<I", len(triangles)))
        for a, b, c in triangles:
            f.write(struct.pack("<3f", 0.0, 0.0, 0.0))
            for v in (a, b, c):
                f.write(struct.pack("<3f", *v))
            f.write(struct.pack("<H", 0))


def cube(size=20.0):
    h = size / 2.0
    p = [
        (-h, -h, -h), (h, -h, -h), (h, h, -h), (-h, h, -h),
        (-h, -h, h), (h, -h, h), (h, h, h), (-h, h, h),
    ]
    faces = [
        (0, 2, 1), (0, 3, 2),  # -Z
        (4, 5, 6), (4, 6, 7),  # +Z
        (0, 1, 5), (0, 5, 4),  # -Y
        (3, 7, 6), (3, 6, 2),  # +Y
        (0, 4, 7), (0, 7, 3),  # -X
        (1, 2, 6), (1, 6, 5),  # +X
    ]
    return [(p[a], p[b], p[c]) for a, b, c in faces]


def icosphere(radius=10.0, subdivisions=2):
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
        return tuple(radius * c / length for c in v)

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

    return [(verts[a], verts[b], verts[c]) for a, b, c in faces]


def torus(major=10.0, minor=4.0, segments=100, rings=60):
    points = []
    for i in range(segments):
        u = 2.0 * math.pi * i / segments
        for j in range(rings):
            v = 2.0 * math.pi * j / rings
            x = (major + minor * math.cos(v)) * math.cos(u)
            y = (major + minor * math.cos(v)) * math.sin(u)
            z = minor * math.sin(v)
            points.append((x, y, z))

    def idx(i, j):
        return (i % segments) * rings + (j % rings)

    triangles = []
    for i in range(segments):
        for j in range(rings):
            a, b = idx(i, j), idx(i + 1, j)
            c, d = idx(i + 1, j + 1), idx(i, j + 1)
            triangles.append((points[a], points[b], points[c]))
            triangles.append((points[a], points[c], points[d]))
    return triangles


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--shape", choices=["cube", "icosphere", "torus"],
                        default="cube")
    parser.add_argument("--out", required=True)
    parser.add_argument("--subdiv", type=int, default=2)
    parser.add_argument("--segments", type=int, default=100)
    parser.add_argument("--rings", type=int, default=60)
    args = parser.parse_args()

    if args.shape == "cube":
        triangles = cube()
    elif args.shape == "icosphere":
        triangles = icosphere(subdivisions=args.subdiv)
    else:
        triangles = torus(segments=args.segments, rings=args.rings)

    write_binary_stl(args.out, triangles)
    print(f"{args.out}: {len(triangles)} triangles")


if __name__ == "__main__":
    main()
