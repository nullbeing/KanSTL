#!/usr/bin/env python3
"""Create a virtual keyboard and send a short test sequence."""
import time

from evdev import UInput, ecodes as e

ui = UInput(
    {e.EV_KEY: [e.KEY_RIGHT, e.KEY_LEFT, e.KEY_UP, e.KEY_DOWN,
                e.KEY_KPPLUS, e.KEY_KPMINUS, e.KEY_ESC]},
    name="stlviewer-test",
)
with open("/tmp/vkbd.path", "w") as f:
    f.write(ui.device.path)

time.sleep(4)

for _ in range(3):
    ui.write(e.EV_KEY, e.KEY_RIGHT, 1)
    ui.syn()
    time.sleep(0.12)
    ui.write(e.EV_KEY, e.KEY_RIGHT, 0)
    ui.syn()
    time.sleep(0.12)

for _ in range(3):
    ui.write(e.EV_KEY, e.KEY_KPPLUS, 1)
    ui.syn()
    time.sleep(0.12)
    ui.write(e.EV_KEY, e.KEY_KPPLUS, 0)
    ui.syn()
    time.sleep(0.12)

# Short Esc must not exit; long Esc should exit.
ui.write(e.EV_KEY, e.KEY_ESC, 1)
ui.syn()
time.sleep(0.1)
ui.write(e.EV_KEY, e.KEY_ESC, 0)
ui.syn()
time.sleep(0.5)
ui.write(e.EV_KEY, e.KEY_ESC, 1)
ui.syn()
time.sleep(1.0)
ui.write(e.EV_KEY, e.KEY_ESC, 0)
ui.syn()
time.sleep(0.5)
ui.close()
