# EspNowBus

[日本語 README](README.ja.md)

Lightweight, group-oriented ESP-NOW message bus for ESP32 and Arduino sketches. EspNowBus focuses on keeping small networks (≈6 nodes) secure by default while exposing a simple Arduino-style API.

## Highlights
- Simple API: `begin()`, `sendTo()`, `broadcast()`, `onReceive()`, `onSendResult()`.
- Secure-by-default: ESP-NOW encryption, join-time challenge/response, and authenticated broadcast are enabled unless you turn them off.
- Auto peer registration: nodes can broadcast join requests; eligible nodes accept and register peers automatically.
- Deterministic sending: outbound messages are queued and sent one at a time by a FreeRTOS task.
- Heartbeat-driven liveness: periodic unicast heartbeat (Ping/Pong) and automatic re-JOIN when peers go quiet.

## Concepts
- **Group name → keys/IDs**: A `groupName` derives `groupSecret`, `groupId`, `keyAuth` (join auth), and `keyBcast` (broadcast auth).
- **Packet types**: `DataUnicast`, `DataBroadcast`, `ControlJoinReq`, `ControlJoinAck`, `ControlHeartbeat`, `ControlAppAck`.
- **Security**: Broadcast packets carry `groupId`, `seq`, and `authTag`; join uses challenge/response; encryption is recommended. Join/Ack and Heartbeat are sent without ESP-NOW encryption (peer not yet set up) but carry HMAC.

## Quick start
```cpp
#include <EspNowBus.h>

EspNowBus bus;

void setup() {
  Serial.begin(115200);

  EspNowBus::Config cfg;
  cfg.groupName = "my-group";
  cfg.useEncryption = true;
  cfg.maxQueueLength = 16;

  bus.onReceive([](const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry, bool isBroadcast) {
    Serial.printf("From %02X:%02X... len=%d retry=%d bcast=%d\n", mac[0], mac[1], (int)len, wasRetry, isBroadcast);
  });

  bus.onSendResult([](const uint8_t* mac, EspNowBus::SendStatus st) {
    Serial.printf("Send to %02X:%02X... status=%d\n", mac[0], mac[1], (int)st);
  });

  bus.begin(cfg);
  bus.sendJoinRequest();  // ask peers to add us (broadcast)
}

void loop() {
  const char msg[] = "hello";
  bus.broadcast(msg, sizeof(msg));
  delay(1000);
}
```

## Configuration
`EspNowBus::Config`
- `groupName` (required): common group identifier used to derive keys.
- `useEncryption` (default `true`): ESP-NOW encryption; max 6 peers when enabled.
- `enablePeerAuth` (default `true`): join-time challenge/response.
- `channel` (default `-1`): Wi-Fi channel. `-1` hashes `groupName`/`groupId` to pick 1–13 automatically; any explicit value is clamped to 1–13. Keep all group members on the same channel.
- `phyRate` (default `WIFI_PHY_RATE_11M_L`): ESP-NOW PHY rate. ESP-IDF 5.1+ sets the rate per-peer (including the broadcast peer entry). Invalid values fall back to the default. Practical hints:
  - `WIFI_PHY_RATE_1M_L` (802.11b): slow but most stable at long range.
  - `WIFI_PHY_RATE_11M_L` (802.11b): moderate speed, stable up to mid-range.
  - `WIFI_PHY_RATE_24M` (802.11g): fast at short range, broadly compatible.
  - `WIFI_PHY_RATE_MCS4_LGI` (802.11n, ~39 Mbps): realistic stable ceiling on plain ESP32.
  - `WIFI_PHY_RATE_MCS7_LGI` (802.11n, ~65 Mbps): fastest, but often unstable except on ESP32-S3/C3.
- `maxQueueLength` (default `16`): outbound queue length.
- `maxPayloadBytes` (default `1470`): max payload per send. ESP-IDF 5.4+ supports ~1470 bytes; older IDF is effectively limited to ~250 bytes. Actual usable bytes are smaller due to internal headers (Unicast ≈ `maxPayloadBytes - 6`, Broadcast ≈ `maxPayloadBytes - 6 - 4 - 16`).
- `maxRetries` (default `1`): resend attempts after the initial send (0 = no retry).
- `retryDelayMs` (default `0`): delay between retries (defaults to immediate retry when a timeout is detected).
- `txTimeoutMs` (default `120`): in-flight send timeout; when elapsed, treat as failure and retry or give up.
- `sendTimeoutMs` (default `50`): queueing timeout when adding to the send queue. `0`=non-blocking, `portMAX_DELAY`=block forever.
- `autoJoinIntervalMs` (default `30000`): periodic JOIN broadcast interval; `0` disables auto join.
- `heartbeatIntervalMs` (default `10000`): heartbeat cadence. 1× → send heartbeat ping, 2× → broadcast targeted JOIN, 3× → drop peer.
- `taskCore` (default `ARDUINO_RUNNING_CORE`): FreeRTOS send-task core pinning. `-1` for unpinned, `0` or `1` to pin; default matches the loop task.
- `taskPriority` (default `3`): send-task priority; keep above loop(1) but below WiFi internals (≈4–5).
- `taskStackSize` (default `4096`): send-task stack size (bytes).
- `enableAppAck` (default `true`): auto app-level ACKs for unicast. When enabled, delivery success is signaled by `AppAckReceived`; missing app-ACK triggers retries and `AppAckTimeout`.
- Not ISR-safe: `sendTo`/`broadcast` cannot be called from ISR (queue/blocking APIs are used).
- `replayWindowBcast` (default `32`): broadcast replay window (set 0 to disable; max 16 senders, 32-bit window, evict oldest sender on overflow).

### Per-call timeout override
`sendTo` / `sendToAllPeers` / `broadcast` accept an optional `timeoutMs` parameter.  
Semantics: `0` = non-blocking, `portMAX_DELAY` = block forever, `kUseDefault` (`portMAX_DELAY - 1`) = use `Config.sendTimeoutMs`.

### Queue behavior and sizing
- Payloads are copied into the queue; `len > maxPayloadBytes` is rejected immediately.
- Queue is a FreeRTOS Queue holding metadata (pointer+length+dest type) to pre-allocated fixed-size buffers; begin fails if the pool cannot be allocated.
- Memory estimate: roughly `maxPayloadBytes * maxQueueLength` plus metadata (e.g., 1470B×16 ≈ 24KB).
- For constrained RAM or legacy compatibility, lower `maxPayloadBytes` (e.g., 250) and tune `maxQueueLength`.
- Introspection: `sendQueueFree()`/`sendQueueSize()` return remaining slots and enqueued count.
- Peer introspection: `peerCount()` and `getPeer(index, macOut)` allow enumerating known peers.

## Examples (use-cases)
- `examples/01_Broadcast`: Simple periodic broadcast (auto-JOIN disabled).
- `examples/02_JoinAndUnicast`: JOIN peers then unicast to a random peer with AppAck; periodic JOIN helps rediscover peers.
- `examples/03_SendToAllPeers`: Per-peer unicast fan-out (`sendToAllPeers`) for delivery assurance with encryption/auth/AppAck.
- `examples/04_MasterSlave`: Master (accepts JOIN) and Slave (sends sensor-ish data to all peers) pair sketch.
- `examples/05_SendStatusDemo`: Inspect `SendStatus` via switch; useful to see retries/timeouts vs. app-level ACK outcomes.
- `examples/06_NoAppAck`: App-level ACK disabled; shows physical `SentOk` only (lightweight, no logical delivery check).
- `examples/07_AutoPurge`: JOIN event callbacks and heartbeat-based removal/leave cases.
- `examples/08_ChannelOverride`: Explicit Wi-Fi channel selection; demonstrates clamping when 0 is specified.
- `examples/09_PhyRateOverride`: Override ESP-NOW PHY rate to `WIFI_PHY_RATE_1M_L` for longer range (default is 24M).
- `examples/10_LowFootprintBroadcast`: Minimal footprint broadcast (encryption/AppAck/peerAuth OFF, payload capped at 250B, small queue).
- `examples/11_FullConfigTemplate`: Template with every `Config` field spelled out at its default value.

### Retries, JOIN, heartbeat, duplicates
- Send task keeps a single in-flight slot with a "sending" flag. On ESP-NOW send-complete callback, it clears the flag and emits `onSendResult`.
- If the flag stays set longer than `txTimeoutMs`, treat as timeout and retry (or fail) using the same message ID/sequence; `retryDelayMs` defaults to 0 (immediate retry).
- Retries set a retry flag; receivers drop duplicate `msgId/seq` per peer and may optionally surface "wasRetry" metadata in callbacks.
- Send-complete CB should not touch shared state directly; notify the send task via FreeRTOS task notification (`xTaskNotifyFromISR`) and let the send task clear the flag and dispatch `onSendResult`.
- JOIN flow: `sendJoinRequest(targetMac)` broadcasts ControlJoinReq (HMAC+targetMac). Acceptors validate `groupId/targetMac/HMAC` and broadcast ControlJoinAck (echo nonceA, add nonceB+targetMac, HMAC). Both sides add peer after Ack and switch to encrypted unicast.
- Broadcast/control packets carry `groupId` and a 16-byte HMAC tag (keyBcast or keyAuth); receivers verify and drop mismatches. Broadcast replay uses a small table (max 16 senders, 32-bit window; evict oldest sender on overflow).
- Even with ESP-NOW encryption disabled, Broadcast/Control/AppAck/Heartbeat packets carry HMAC (keyBcast/keyAuth) for authenticity; keep `enableAppAck` on for delivery assurance.
- Heartbeat: unicast Ping/Pong without AppAck. Pong reception marks liveness; missing heartbeat drives targeted JOIN at 2× interval and disconnect at 3× interval.
- App-level ACKs (`enableAppAck=true` by default): receiver auto-replies with msgId-based ACKs; sender treats missing app-ACK as undelivered (even if ESP-NOW reported success). If an app-ACK arrives without a physical ACK, mark delivered but log a warning.
- SendStatus semantics: for app-ACK-enabled unicast, completion is `AppAckReceived` (success) or `AppAckTimeout`; `SentOk` indicates only physical TX success when app-ACK is disabled.
- ControlAppAck: a unicast control packet carrying msgId (id field = msgId) with keyAuth HMAC; sent automatically when `enableAppAck` is true. Duplicates still emit AppAck to stop retries.

### Status list
- `Queued`: enqueued successfully.
- `SentOk`: physical send success (app-ACK disabled).
- `SendFailed`: physical send failed (ESP-NOW failure).
- `Timeout`: physical send timeout.
- `DroppedFull`: queue full at enqueue time.
- `DroppedOldest`: reserved (not used in current implementation).
- `TooLarge`: payload exceeds `maxPayloadBytes`.
- `Retrying`: resend in progress.
- `AppAckReceived`: logical ACK arrived (app-ACK enabled).
- `AppAckTimeout`: logical ACK did not arrive after retries (app-ACK enabled).

SendStatus notes:
- Both progress and final results are reported (`Queued`, `Retrying` as progress; `SentOk`/`SendFailed`/`Timeout` or `AppAckReceived`/`AppAckTimeout` as completion). You may see multiple events per packet.
- Completion states: app-ACK disabled → `SentOk` (success) / `SendFailed` or `Timeout` (failure). app-ACK enabled → `AppAckReceived` (success) / `AppAckTimeout` (failure).
- In normal operation with healthy peers, auto-retry will often succeed; you may not need to watch every status. For critical requirements, monitor failures to trigger recovery（re-JOINなど）.

## Callbacks
- `onReceive(cb)`: accepted unicast and authenticated broadcast packets.
- `onReceive(const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry, bool isBroadcast)`: accepted unicast or authenticated broadcast; `wasRetry` is true if sender flagged retry, `isBroadcast` tells the path.
- `onSendResult(const uint8_t* mac, SendStatus status)`: per-queued packet result. With app-ACK enabled, completion is `AppAckReceived`/`AppAckTimeout`.
- `onAppAck(const uint8_t* mac, uint16_t msgId)`: fired for every AppAck received (even if not in-flight); typically for debugging/telemetry.
- `onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck)`: JOIN events. `accepted=true,isAck=false`=JoinReq accepted; `accepted=true,isAck=true`=JoinAck success; `accepted=false,isAck=true`=JoinAck mismatch/fail; `accepted=false,isAck=false`=heartbeat timeout (peer removed).

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
