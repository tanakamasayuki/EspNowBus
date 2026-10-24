# 03_GatewayWiFiSTA

Gateway-side `EspNowIP` example using Wi-Fi STA as the uplink.

## What It Shows

- `EspNowIPGateway` initialization
- Wi-Fi STA uplink preparation
- The baseline `routing + IPv4 NAT` gateway structure
- Printing the STA channel used by the uplink

## Limitations

- This example is easy to test but is not the best production-oriented uplink option.
- `EspNowBus` normally derives its channel automatically from `groupName`. In normal use, keep that default, but in this example the effective channel is constrained by the STA uplink, so every node may need the same channel set explicitly.
- With `ESP-NOW` + `Wi-Fi STA`, the effective ESP-NOW channel is constrained by the connected STA channel.
- If the AP channel changes, roaming occurs, or multiple APs use different channels, the ESP-NOW side may stop linking correctly until the device side is aligned again.

## Intended Use

Use this as a development and validation example for the Wi-Fi STA uplink path.

## Test Procedure

1. Configure the gateway with your Wi-Fi STA SSID and password.
2. Boot the gateway and wait for the serial log to print the STA channel.
3. Configure the device-side `EspNowBus` channel to the same channel shown by the gateway.
4. Start the device-side example and confirm that `Bus session` and then `IP session` can be established.

Example (normally left commented out):

```cpp
// EspNowIPGateway::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```

## Prepare and run

1. Check the [example guide](../../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [03_GatewayWiFiSTA.ino](03_GatewayWiFiSTA.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<Arduino.h>`, `<WiFi.h>`, `<EspNowIP.h>`, `<esp_netif.h>`, `<time.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `gateway.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `gateway.poll(...)` — Service pending input regularly; interpret its result before accessing new data.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
Started NTP sync for uplink check.
Wi-Fi STA connected, channel=%ld, ip=%s
 gw=
```

## Verify and troubleshoot

Connect a bus gateway through Wi-Fi STA and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../../README.md) for the next sketch.
