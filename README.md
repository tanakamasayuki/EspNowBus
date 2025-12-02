# EspNowBus

[日本語 README](README.ja.md)

Lightweight, group-oriented ESP-NOW message bus for ESP32 and Arduino sketches. EspNowBus focuses on keeping small networks (≈6 nodes) secure by default while exposing a simple Arduino-style API.

> Status: design phase. See `SPEC.ja.md` for the current specification; interfaces may change as the library is implemented.  
> Current code: queue/retry/timeout, key derivation, PMK/LMK setup for ESP-NOW encryption, HMAC for broadcast/control packets using keyBcast/keyAuth, and a minimal authenticated JOIN request/ack flow (nonceA echoed, responder nonceB returned/stored, resumption token carried in JOIN) are implemented.

## Highlights
- Simple API: `begin()`, `sendTo()`, `broadcast()`, `onReceive()`, `onSendResult()`.
- Secure-by-default: ESP-NOW encryption, join-time challenge/response, and authenticated broadcast are enabled unless you turn them off.
- Auto peer registration: nodes can broadcast join requests; eligible nodes accept and register peers automatically.
- Deterministic sending: outbound messages are queued and sent one at a time by a FreeRTOS task.

## Concepts
- **Group name → keys/IDs**: A `groupName` derives `groupSecret`, `groupId`, `keyAuth` (join auth), and `keyBcast` (broadcast auth).
- **Roles**: `Master` / `Flat` can accept registrations; `Slave` cannot. Internally managed via `canAcceptRegistrations`.
- **Packet types**: `DataUnicast`, `DataBroadcast`, `PeerAuthHello`, `PeerAuthResponse`, `ControlJoinReq`, `ControlJoinAck`.
- **Security**: Broadcast packets carry `groupId`, `seq`, and `authTag`; join uses challenge/response; encryption is recommended.

## Quick start (planned)
```cpp
#include <EspNowBus.h>

EspNowBus bus;

void setup() {
  Serial.begin(115200);

  EspNowBus::Config cfg;
  cfg.groupName = "my-group";
  cfg.canAcceptRegistrations = true;  // Master/Flat
  cfg.useEncryption = true;
  cfg.maxQueueLength = 16;

  bus.onReceive([](const uint8_t* mac, const uint8_t* data, size_t len) {
    Serial.printf("From %02X:%02X... len=%d\n", mac[0], mac[1], (int)len);
  });

  bus.onSendResult([](const uint8_t* mac, bool ok) {
    Serial.printf("Send to %02X:%02X... %s\n", mac[0], mac[1], ok ? "OK" : "FAIL");
  });

  bus.begin(cfg);
  bus.sendRegistrationRequest();  // ask peers to add us
}

void loop() {
  const char msg[] = "hello";
  bus.broadcast(msg, sizeof(msg));
  delay(1000);
}
```

## Configuration (spec)
`EspNowBus::Config`
- `groupName` (required): common group identifier used to derive keys.
- `useEncryption` (default `true`): ESP-NOW encryption; max 6 peers when enabled.
- `enablePeerAuth` (default `true`): join-time challenge/response.
- `enableBroadcastAuth` (default `true`): HMAC-tagged broadcasts with replay checks.
- `maxQueueLength` (default `16`): outbound queue length.
- `maxPayloadBytes` (default `1470`): max payload per send (ESP-NOW v2.0 MTU). Use `250` for maximum compatibility/low memory.
- `maxRetries` (default `1`): resend attempts after the initial send (0 = no retry).
- `retryDelayMs` (default `0`): delay between retries (defaults to immediate retry when a timeout is detected).
- `txTimeoutMs` (default `120`): in-flight send timeout; when elapsed, treat as failure and retry or give up.
- `canAcceptRegistrations` (default `true`): whether this node can accept new peers.
- `sendTimeoutMs` (default `50`): queueing timeout when adding to the send queue. `0`=non-blocking, `portMAX_DELAY`=block forever.
- `taskCore` (default `ARDUINO_RUNNING_CORE`): FreeRTOS send-task core pinning. `-1` for unpinned, `0` or `1` to pin; default matches the loop task.
- `taskPriority` (default `3`): send-task priority; keep above loop(1) but below WiFi internals (≈4–5).
- `taskStackSize` (default `4096`): send-task stack size (bytes).
- `enableAppAck` (default `true`): auto app-level ACKs for unicast. When enabled, delivery success is signaled by `AppAckReceived`; missing app-ACK triggers retries and `AppAckTimeout`.
- Not ISR-safe: `sendTo`/`broadcast` cannot be called from ISR (queue/blocking APIs are used).
- `replayWindowBcast` (default `64`): broadcast replay window (set 0 to disable).
- `replayWindowJoin` (default `64`): JOIN replay window（how many JOIN seq to remember; set 0 to disable replay drop). Helps avoid processing duplicate JOINs during bursts or retries. Values >64 are clamped internally (64-bit window).

### Per-call timeout override
`sendTo` / `sendToAllPeers` / `broadcast` accept an optional `timeoutMs` parameter.  
Semantics: `0` = non-blocking, `portMAX_DELAY` = block forever, `kUseDefault` (`portMAX_DELAY - 1`) = use `Config.sendTimeoutMs`.

### Queue behavior and sizing
- Payloads are copied into the queue; `len > maxPayloadBytes` is rejected immediately.
- Queue is a FreeRTOS Queue holding metadata (pointer+length+dest type) to pre-allocated fixed-size buffers; begin fails if the pool cannot be allocated.
- Memory estimate: roughly `maxPayloadBytes * maxQueueLength` plus metadata (e.g., 1470B×16 ≈ 24KB).
- For constrained RAM or legacy compatibility, lower `maxPayloadBytes` (e.g., 250) and tune `maxQueueLength`.
- Introspection: `sendQueueFree()`/`sendQueueSize()` return remaining slots and enqueued count.

### Retries and duplicate handling
- Send task keeps a single in-flight slot with a "sending" flag. On ESP-NOW send-complete callback, it clears the flag and emits `onSendResult`.
- If the flag stays set longer than `txTimeoutMs`, treat as timeout and retry (or fail) using the same message ID/sequence; `retryDelayMs` defaults to 0 (immediate retry).
- Retries set a retry flag; receivers drop duplicate `msgId/seq` per peer and may optionally surface "wasRetry" metadata in callbacks.
- Send-complete CB should not touch shared state directly; notify the send task via FreeRTOS task notification (`xTaskNotifyFromISR`) and let the send task clear the flag and dispatch `onSendResult`.
- Minimal JOIN flow: `sendRegistrationRequest()` broadcasts a ControlJoinReq; nodes that can accept register the sender and unicast ControlJoinAck back. (No auth/encryption yet.)
- Broadcast/control packets carry `groupId` and a 16-byte HMAC tag (keyBcast or keyAuth); receivers verify and drop mismatches. Broadcast replay is limited via a 64-entry sliding window per peer.
- Even with ESP-NOW encryption disabled, Broadcast/Control/AppAck packets still carry HMAC (keyBcast/keyAuth) for authenticity; keep `enableAppAck` on for delivery assurance.
- JOIN packets carry an 8-byte nonce; Acceptors echo it back in Ack. Full challenge/response is still TODO.
- JOIN replay is limited by a separate window; Ack also returns a responder nonceB (currently stored for future validation).
- App-level ACKs (`enableAppAck=true` by default): receiver auto-replies with msgId-based ACKs; sender treats missing app-ACK as undelivered (even if ESP-NOW reported success). If an app-ACK arrives without a physical ACK, mark delivered but log a warning.
- On restart/drift: JOIN re-run with fresh tokens; prevToken mismatch is treated as fresh join (state is reset) to recover automatically.
- SendStatus semantics: for app-ACK-enabled unicast, completion is `AppAckReceived` (success) or `AppAckTimeout`; `SentOk` indicates only physical TX success when app-ACK is disabled.
- ControlAppAck: a unicast control packet carrying msgId (id field = msgId) with keyAuth HMAC; sent automatically when `enableAppAck` is true. Duplicates still emit AppAck to stop retries.

### Status list (current)
`Queued`, `SentOk`, `SendFailed`, `Timeout`, `DroppedFull`, `DroppedOldest`, `TooLarge`, `Retrying`, `AppAckReceived`, `AppAckTimeout`.

## Callbacks
- `onReceive(cb)`: called for accepted unicast and authenticated broadcast packets.
- `onSendResult(cb)`: delivery result per queued packet.

## Roadmap
- Finalize key derivation, packet layout, and replay windows (see `SPEC.ja.md` gaps).
- Implement core library and minimal examples for Arduino IDE and PlatformIO.
- Add tests for packet validation, join handshake, and queue/backoff behavior.

## Documentation
- Detailed spec (Japanese): `SPEC.ja.md`
- Japanese README: `README.ja.md`
- Use cases:
  - Small sensor → gateway networks
  - Controller → multiple robots or gadgets
  - Small multiplayer or event/local interactive setups
  - Ad-hoc device clusters that should stay isolated via group keys

## License
MIT (see `LICENSE`).
