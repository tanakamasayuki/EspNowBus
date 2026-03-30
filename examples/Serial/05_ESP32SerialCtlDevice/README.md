# 05_ESP32SerialCtlDevice

This example exposes the external `ESP32SerialCtl` CLI over Serial over EspNow.

Use it together with:

- Controller side: `examples/Serial/02_ControllerBridge`
- Device side: `examples/Serial/05_ESP32SerialCtlDevice`

## Dependency

This example requires the external `ESP32SerialCtl` library.

- Repository: `https://github.com/tanakamasayuki/ESP32SerialCtl`
- WebSerial console: `https://tanakamasayuki.github.io/ESP32SerialCtl/`

The bundled `sketch.yaml` expects:

```yaml
libraries:
  - ESP32SerialCtl (1.0.2)
```

Install `ESP32SerialCtl` before compiling this sketch.

## Wiring and Roles

- Flash `02_ControllerBridge` to the ESP32 connected to the PC by USB.
- Flash `05_ESP32SerialCtlDevice` to the remote ESP32/ESP32-S3 device.
- Open the USB serial port of the controller device from your PC.

The controller sketch bridges USB Serial to the currently selected EspNowSerial session.

## Basic Flow

1. Power both devices and wait until they pair over ESP-NOW.
2. Connect your PC to the controller device, not to the remote device.
3. If needed, type `///list` on the controller serial port to inspect sessions.
4. If needed, type `///use 0` or another index to select a session.

`02_ControllerBridge` automatically selects session `0` when it becomes available, so in the common two-device case you usually do not need to type `///use 0`.

After a session is selected, normal serial traffic is forwarded transparently:

- PC to controller USB Serial to remote `ESP32SerialCtl`
- Remote `ESP32SerialCtl` response to EspNowSerial to controller to PC

## WebSerial Usage

You can use the WebSerial console from the `ESP32SerialCtl` project:

- `https://tanakamasayuki.github.io/ESP32SerialCtl/`

Connect the browser to the USB serial port of the controller device running `02_ControllerBridge`.

Once connected, the WebSerial UI talks to the remote device through the bridge. The remote device runs `ESP32SerialCtl` on top of `EspNowSerialPort`, so commands such as `help`, `sys info`, and other `ESP32SerialCtl` commands are available from the browser.

## Notes

- Local controller commands use the `///` prefix and are handled only by `02_ControllerBridge`.
- Example local commands:
  - `///list`
  - `///use 0`
  - `///help`
- Any line that does not start with `///` is forwarded to the selected remote session.
