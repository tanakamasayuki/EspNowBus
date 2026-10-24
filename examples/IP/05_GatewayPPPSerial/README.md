# 05_GatewayPPPSerial

Gateway-side `EspNowIP` example using physical UART PPP as the uplink.

## What It Shows

- A `lwIP PPPoS + esp_netif PPP` oriented structure
- The gateway-side skeleton for a physical UART raw-PPP uplink
- Reusing the same gateway-side NAT model as other uplink types

## Intended Use

Use this as an alternative gateway example when Wi-Fi uplink is unavailable or intentionally avoided.

## Notes

- The target topology is `ESP32 UART <-> host PC PPP peer`
- `PPP.h` is AT-modem oriented, so it is not used for this scenario
- The intended path is to create `ESP_NETIF_DEFAULT_PPP()`, then bind a `ppp_pcb` with `pppapi_pppos_create()`
- UART RX should feed `pppos_input_tcpip()`, and PPP TX should be written from the PPPoS output callback
- The resulting `esp_netif_t*` can be passed into `EspNowIPGateway` just like the validated Wi-Fi STA and Ethernet uplink paths
- The sketch uses a local `PPPoSNetif` helper in this example folder that accepts any already-initialized `Stream`
- Serial initialization is the user's responsibility
- Logging stays off by default and can be enabled by passing a `Print*` logger
- If `pppIo` and `debugOut` point to the same stream, debug logging is disabled automatically to avoid corrupting PPP traffic

## Prepare and run

1. Check the [example guide](../../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [05_GatewayPPPSerial.ino](05_GatewayPPPSerial.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<Arduino.h>`, `<EspNowIP.h>`

Keep these required local files with the sketch: [PPPoSNetif.h](PPPoSNetif.h)

3. After upload, observe the display or other output used by this example. If using Serial, match its initialization in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `pppos.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `pppos.poll(...)` — Service pending input regularly; interpret its result before accessing new data.

Review these application constants before adapting the sketch: `kEnablePppLogs`. Match pins, channels and capacities to your application.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Verify and troubleshoot

Use a serial PPP gateway endpoint and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../../README.md) for the next sketch.
