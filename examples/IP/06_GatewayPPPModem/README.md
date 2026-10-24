# 06_GatewayPPPModem

Gateway-side `EspNowIP` example using an AT-command cellular modem PPP uplink.

## What It Shows

- A modem uplink flow based on Arduino ESP32 `PPP.h`
- Passing `PPP.netif()` into `EspNowIPGateway`
- A PPP example intended for AT-modem style APN / PIN / UART pin / flow-control setup

## Intended Use

Use this when your uplink is a modem supported by `PPP.h`, such as SIM7600, SIM7070, SIM7000, BG96, or SIM800.

## Notes

- This is different from `05_GatewayPPPSerial`, which is for host-PC raw PPP
- `PPP.h` is `esp_modem` oriented, so this sample is for cellular modems that support AT commands and PPP
- Some cellular modems expose HTTP, MQTT, TCP, and similar features through dedicated AT commands, but this example uses PPP instead of those modem-specific command APIs
- Using PPP makes the uplink available as a `NetIF`, so routing / NAT / DNS / socket use can follow the same path as the Wi-Fi STA and Ethernet examples
- The tradeoff is extra framing and control overhead, so PPP can be less efficient than using a modem's direct AT-command data features
- Adjust `PPP_MODEM_APN`, `PPP_MODEM_PIN`, UART pins, flow control, and modem model for your hardware
- Once `PPP.netif()` is available, the NAT / routing path is the same as the Wi-Fi STA and Ethernet uplink examples

## Prepare and run

1. Check the [example guide](../../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [06_GatewayPPPModem.ino](06_GatewayPPPModem.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<Arduino.h>`, `<EspNowIP.h>`, `<PPP.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `PPP.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `gateway.poll(...)` — Service pending input regularly; interpret its result before accessing new data.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
PPP Started
PPP Connected
PPP Got IP: %s gw=%s dns=%s
```

## Verify and troubleshoot

Connect a gateway with a PPP modem and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../../README.md) for the next sketch.
