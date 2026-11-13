#!/bin/sh
# Install kanstl into APPLaunch on the Cardputer Zero.
# Run this on the device from the repository root.
set -eu

ROOT="${APP_ROOT:-/usr/share/APPLaunch}"
BUILD="${BUILD:-build-aarch64}"
BIN="$BUILD/kanstl"

if [ ! -f "$BIN" ]; then
    echo "missing $BIN; build it first with:" >&2
    echo "  cmake -S . -B $BUILD -DCMAKE_BUILD_TYPE=Release \\" >&2
    echo "    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \\" >&2
    echo "    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++" >&2
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    SUDO=sudo
else
    SUDO=
fi

$SUDO install -Dm755 "$BIN" "$ROOT/bin/kanstl"
$SUDO install -Dm755 packaging/applaunch/bin/run-kanstl "$ROOT/bin/run-kanstl"
$SUDO install -Dm644 packaging/applaunch/applications/kanstl.desktop \
    "$ROOT/applications/kanstl.desktop"
$SUDO install -Dm644 packaging/applaunch/share/images/kanstl.png \
    "$ROOT/share/images/kanstl.png"

echo "installed to $ROOT"
echo "restart APPLaunch if the entry is not visible:"
echo "  systemctl --user restart APPLaunch.service"
