# 12_ExplicitLeave

Interactive example for leave, rejoin, and restart handling.

## What It Shows

- `end(...)` with and without stopping Wi-Fi
- Re-`begin()` after leave
- Broadcast send after restart of the bus
- Join and send result callbacks while changing state

## How to Use

Flash the same sketch to two boards, open a serial monitor, and use the listed single-letter commands such as `e`, `p`, `b`, `s`, and `r`.

## Prepare and run

1. Check the [example guide](../../docs/GUIDE_EXAMPLES.md) for setup and hardware. Use the root README for supported boards and dependencies.
2. Open [12_ExplicitLeave.ino](12_ExplicitLeave.ino) and select the actual board/port. Prepare the dependencies used by these includes:

   `<EspNowBus.h>`

3. Upload and open the Serial monitor at 115200 baud. Check startup messages and input conditions in the source.

## Follow the sketch

These APIs are listed in their first source order. Registering a callback does not execute its body at that point.

1. `bus.onReceive(...)` — Register a receive observer; use explicit payload length and copy bytes needed beyond the callback.
2. `bus.onSendResult(...)` — Observe asynchronous progress/completion and distinguish physical from application acknowledgement.
3. `bus.begin(...)` — Initialize this component after its prerequisites; inspect the returned status where provided.
4. `bus.broadcast(...)` — Submit to the group broadcast path; it does not provide one application ACK per receiver.

## API usage and precautions

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## Inputs and observable output

These are selected output formats from the source. printf specifiers are replaced by runtime values; inspect the surrounding conditions rather than assuming these lines always occur in this order.

```text
RX from %02X:%02X:%02X:%02X:%02X:%02X data='%s' len=%u retry=%d bcast=%d
TX to %02X:%02X:%02X:%02X:%02X:%02X status=%s
JoinEvent mac=%02X:%02X:%02X:%02X:%02X:%02X accepted=%d isAck=%d
```

## Verify and troubleshoot

Leave and observe peer-state changes and verify the result on its output. First use the supplied input, then change one configuration or data source at a time.

| Symptom | First check |
|---|---|
| No output | Board/dependencies, initialization results, output destination and Serial baud |
| Starts but stops making progress | Wait conditions, buffer space, peer/input readiness and service calls |
| Unexpected values | Lengths/units, format, data lifetime, shared state and partial results |

See the [guide](../../docs/GUIDE_EXAMPLES.md) for shared principles and the [example index](../README.md) for the next sketch.
