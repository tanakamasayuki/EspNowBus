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

## Prepare and run

1. Check the [example guide](../../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [05_ESP32SerialCtlDevice.ino](05_ESP32SerialCtlDevice.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<ESP32SerialCtl.h>`, `<EspNowSerial.h>`, `<WiFi.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `serialHub.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `controlSerial.attach(...)` — Connect to an already prepared resource and retain its lifetime.
3. `serialHub.poll(...)` — Service pending input regularly; interpret its result before accessing new data.
4. `remoteCli.service(...)` — Process pending commands regularly; blocking the caller postpones new input.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
ESP32SerialCtl device ready: %02X:%02X:%02X:%02X:%02X:%02X
Use examples/Serial/02_ControllerBridge on the controller side.
```

## Verify and troubleshoot

Expose a serial command device through the bus and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../../README.md) for the next sketch.
