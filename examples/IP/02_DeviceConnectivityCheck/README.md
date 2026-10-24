# 02_DeviceConnectivityCheck

Device-side connectivity diagnostics example for `EspNowIP`.

## What It Shows

- ICMP ping to the `gateway`
- DNS resolution
- UDP communication to NTP
- TCP communication via HTTP GET

## Intended Use

Use this after `01_DeviceBasic` has obtained a lease, in order to check how far real IP connectivity works.

## Notes

- The targets are fixed in the sketch.
- The intended workflow is to copy this example into `examples/IP/temp/` before editing it for local testing.
- `gateway ping` uses `10.201.0.1`.
- Outbound checks use `example.com`, `pool.ntp.org`, and `http://example.com/`.
- The device side uses `cfg.autoJoinIntervalMs = 0`, so it stays silent and expects the gateway side to advertise before taking a lease.

## Prepare and run

1. Check the [example guide](../../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [02_DeviceConnectivityCheck.ino](02_DeviceConnectivityCheck.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<Arduino.h>`, `<EspNowIP.h>`, `<HTTPClient.h>`, `<Network.h>`, `<NetworkClient.h>`, `<NetworkUdp.h>`, `<esp_netif.h>`, `<esp_wifi.h>`, `<lwip/ip_addr.h>`, `<ping/ping_sock.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `udp.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
2. `ip.poll(...)` — Service pending input regularly; interpret its result before accessing new data.

Review these application constants before adapting the sketch: `kMtu`, `kRunIntervalMs`, `kPingTaskStackSize`, `kNtpRetries`, `kNtpPort`. Match pins, channels and capacities to your application.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
selfMac=%02X:%02X:%02X:%02X:%02X:%02X
channel=%u second=%d
ip=
```

## Verify and troubleshoot

Inspect device-side IP connectivity and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../../README.md) for the next sketch.
