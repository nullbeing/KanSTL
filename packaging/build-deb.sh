#!/bin/sh
# Build the arm64 .deb for the Cardputer Zero AppStore.
#
#   packaging/build-deb.sh            # cross-compile, then pack into dist/
#   packaging/build-deb.sh --no-build # pack the existing build-aarch64/kanstl
#
# The package installs the binary both as /usr/bin/kanstl and inside APPLaunch
# (/usr/share/APPLaunch/bin/kanstl plus the run-kanstl wrapper), together with
# the desktop entry and the icon.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

NAME=kanstl
VERSION=$(python3 -c "import json; print(json.load(open('app-builder.json'))['version'])")
ARCH=arm64
BUILD=build-aarch64
BIN=$BUILD/$NAME
STAGE=dist/$NAME-$VERSION
DEB=dist/${NAME}_${VERSION}_${ARCH}.deb

if [ "$1" != "--no-build" ]; then
    cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
        -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
    cmake --build "$BUILD" -j"$(nproc)"
fi

[ -f "$BIN" ] || { echo "missing $BIN" >&2; exit 1; }
case "$(file -b "$BIN")" in
    *aarch64*) ;;
    *) echo "$BIN is not an aarch64 binary" >&2; exit 1 ;;
esac

rm -rf "$STAGE"
install -d "$STAGE/usr/bin" \
           "$STAGE/usr/share/APPLaunch/bin" \
           "$STAGE/usr/share/APPLaunch/applications" \
           "$STAGE/usr/share/APPLaunch/share/images" \
           "$STAGE/usr/share/doc/$NAME" \
           "$STAGE/DEBIAN"

aarch64-linux-gnu-strip --strip-unneeded -o "$STAGE/usr/bin/$NAME" "$BIN"
install -m755 "$STAGE/usr/bin/$NAME" "$STAGE/usr/share/APPLaunch/bin/$NAME"
install -m755 packaging/applaunch/bin/run-kanstl "$STAGE/usr/share/APPLaunch/bin/run-kanstl"
install -m644 packaging/applaunch/applications/kanstl.desktop \
    "$STAGE/usr/share/APPLaunch/applications/kanstl.desktop"
install -m644 packaging/applaunch/share/images/kanstl.png \
    "$STAGE/usr/share/APPLaunch/share/images/kanstl.png"
gzip -9nc debian/changelog > "$STAGE/usr/share/doc/$NAME/changelog.gz"
chmod 644 "$STAGE/usr/share/doc/$NAME/changelog.gz"

cat > "$STAGE/DEBIAN/control" <<EOF
Package: $NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: Weiming Feng <bestedwin@gmail.com>
Installed-Size: 230
Depends: libc6 (>= 2.34), libgcc-s1 (>= 4.0), libstdc++6 (>= 11)
Section: graphics
Priority: optional
Description: STL 3D model viewer for the Cardputer Zero
 KanSTL opens binary or ASCII STL files from a built-in file browser and
 renders them into a 320x170 RGB565 framebuffer with its own software
 rasterizer: flat shaded, EDGES, MATCAP and wireframe styles, rotate and
 zoom, auto-rotation and an XYZ orientation gizmo.
 .
 It registers with APPLaunch, needs no external 3D engine or LVGL, and runs
 as the normal desktop user.
EOF

(cd "$STAGE" && find . -type f ! -path './DEBIAN/*' | sed 's|^\./||' | sort | xargs -r md5sum > DEBIAN/md5sums)

mkdir -p dist
dpkg-deb --root-owner-group -Zxz -b "$STAGE" "$DEB"
echo "$DEB"
