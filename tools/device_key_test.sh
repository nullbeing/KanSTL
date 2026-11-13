#!/bin/sh
# On-device keyboard/render test using a virtual evdev keyboard.
set -u
export XDG_RUNTIME_DIR=/run/user/1000
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus

systemctl --user stop APPLaunch.service
sleep 1
rm -f /tmp/vkbd.path /tmp/stl.log /tmp/vkbd.log

printf '1090\n' | sudo -S python3 /tmp/vkbd_test.py >/tmp/vkbd.log 2>&1 &
VK=$!
for _ in $(seq 1 30); do
    [ -f /tmp/vkbd.path ] && break
    sleep 0.2
done
KBD=$(cat /tmp/vkbd.path 2>/dev/null || true)
if [ -z "$KBD" ]; then
    echo "virtual keyboard failed:"
    cat /tmp/vkbd.log
    systemctl --user start APPLaunch.service
    exit 1
fi
echo "virtual keyboard: $KBD"
[ -r "$KBD" ] || printf '1090\n' | sudo -S chmod 666 "$KBD"

sleep 1
/tmp/kanstl --kbd "$KBD" /tmp/icosphere.stl >/tmp/stl.log 2>&1 &
APP=$!
sleep 2
dd if=/dev/fb0 bs=640 count=170 of=/tmp/fb_before.raw 2>/dev/null
sleep 3
dd if=/dev/fb0 bs=640 count=170 of=/tmp/fb_after.raw 2>/dev/null
sleep 3

if kill -0 "$APP" 2>/dev/null; then
    echo "app: still running (long Esc did not exit)"
    kill "$APP" 2>/dev/null
else
    echo "app: exited after long Esc"
fi
wait "$APP" 2>/dev/null
wait "$VK" 2>/dev/null

echo "== app log =="
cat /tmp/stl.log
systemctl --user start APPLaunch.service
sleep 2
systemctl --user is-active APPLaunch.service | sed 's/^/APPLaunch: /'
