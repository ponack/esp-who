# Object Tracking Example [[中文]](./README_CN.md)

A touch-controlled object tracking gimbal on the ESP32-P4 Function EV Board: a MIPI CSI camera feeds a human face detector, ByteTrack keeps stable identities across frames, and tapping a face on the LCD selects it as the target — a PID-driven 2-axis pan/tilt servo gimbal then keeps it centered.

The example uses the `noglib` BSP: no LVGL, no text on screen — just colored boxes (red = tracked, green + crosshair = selected) and touch input.

## Hardware

- ESP32-P4 Function EV Board (MIPI CSI camera + 800x1280 LCD + GT911 touch)
- Two servos for pan/tilt, e.g. SG90, MG90S, MG996R. Servo signal pins and all tuning parameters are configurable (see below).

## Interaction

### Select / deselect a target

Tap the LCD (GT911). Only a press rising edge counts as a tap; the hit-test uses the current track boxes:

- Tap inside a face box to select that person. The box turns green with a crosshair, and the gimbal starts keeping them centered.
- Tap the already-selected (green) box again to deselect. The gimbal holds its last angle and waits for the next tap.
- Tap a different face box to switch the target to that person.
- Tapping empty space does nothing.

The console command `home` also releases the selection and drives both servos back to their init angles.

### When the target is lost

If the selected face is occluded or walks out of view:

- The gimbal **holds its last position** (it does not return home by itself).
- For a short grace period (~30 frames, about 1 s at 30 fps) the same track ID is kept. If the face reappears in time, tracking resumes automatically with no extra tap.
- If the target is still missing after that, the selection is released. Boxes go back to red and the gimbal stays put until you tap a new target.

## Console Commands

Runtime tuning is done over the serial console (`track>` prompt). Parameters are applied by the detect task at the next frame boundary, never mid-frame.

| Command | Description |
| --- | --- |
| `help [command]` | List commands, or show help for one command |
| `pid <pan\|tilt> [kp\|ki\|kd <value>]` | Show or set PID gains |
| `servo <pan\|tilt> [min\|max\|init\|dir <value>]` | Show or set servo limits / init angle / direction (`dir` is `1` or `-1`) |
| `cam [fx\|fy <value>]` | Show or set the camera focal length |
| `profile [list\|save\|load\|del] [name]` | Manage parameter profiles (see below) |
| `home` | Move the gimbal to its init angles and release the selection |

Negative values work, e.g. `servo pan dir -1`.

### Parameter Profiles

Tuned parameters can be saved to NVS as named profiles (up to 8) and reloaded later, including across reboots:

```
track> profile                # active profile + dirty flag + all params
track> profile list           # "*" marks the active profile
track> profile save fast      # save current params as profile "fast"
track> profile save           # write back into the active profile
track> profile load smooth    # switch profile
track> profile load default   # back to the Kconfig defaults
track> profile del fast       # delete a profile (not the active one)
```

- `default` is a virtual profile: it always reflects the current Kconfig values and cannot be saved into or deleted.
- The active profile is persisted; after a reboot it is restored automatically.
- `dirty` means the current parameters differ from the active profile's stored copy.

## Kconfig

`idf.py menuconfig` → "Object Tracking Configuration" (see `main/Kconfig.projbuild`):

- **Pan/Tilt Servo**: GPIO, direction, PID gains, initial angle, angle range
- **Camera**: focal length (fx, fy)

Also set **Channel for console output** according to the board revision. Different ESP32-P4 Function EV Board versions may use different console hardware:

| Option | Hardware |
| --- | --- |
| `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` (default) | USB Serial/JTAG |
| `CONFIG_ESP_CONSOLE_UART_DEFAULT` | UART |
