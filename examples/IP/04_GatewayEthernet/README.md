# 04_GatewayEthernet

Gateway-side `EspNowIP` example for a wired Ethernet uplink.

The current reference candidate is `LilyGo T-Internet-COM`. This example assumes `ETH.h` with `LAN8720 RMII`.

## What It Shows

- Passing an Ethernet `esp_netif` obtained from `ETH.h` into the gateway uplink
- Reusing the same `routing + IPv4 NAT` gateway model with wired uplink
- A gateway option without the Wi-Fi STA channel constraint

## Intended Use

Use this as a more deployment-friendly gateway example when a wired Ethernet uplink is available.

## Notes

- The current reference candidate is `T-Internet-COM`
- The sketch uses Arduino ESP32 `ETH.h` instead of `M5_Ethernet.h`
- The expected path is: bring up `ETH`, obtain `ETH.netif()`, then pass it to `EspNowIPGateway`

## Prepare and run

1. Check the [example guide](../../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [04_GatewayEthernet.ino](04_GatewayEthernet.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<Arduino.h>`, `<EspNowIP.h>`, `<ETH.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `ETH.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `gateway.poll(...)` — Service pending input regularly; interpret its result before accessing new data.

Review these application constants before adapting the sketch: `kEthAddr`, `kEthPowerPin`, `kEthMdcPin`, `kEthMdioPin`, `kEthType`, `kEthClockMode`. Match pins, channels and capacities to your application.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
ETH Started
ETH Connected
ETH MAC: %s, IPv4: %s, %s, %uMbps
```

## Verify and troubleshoot

Connect a bus gateway through Ethernet and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../../README.md) for the next sketch.
