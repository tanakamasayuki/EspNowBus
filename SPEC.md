# EspNowBus Specification

## 1. Overview
**EspNowBus** is a lightweight “group-oriented message bus” library that makes ESP-NOW on ESP32 easy to use from Arduino.

- Use ESP-NOW **without collisions** and **with ease**  
- For small networks (~6 nodes), **encryption + authentication enabled by default**  
- Concise API such as `sendTo()`, `broadcast()`, `onReceive()`  
- FreeRTOS task for **autonomous send control**

---

## 2. Design goals
- Provide logical isolation by specifying a single `groupName`; keys/IDs are derived internally so traffic from other groups is rejected
- **No interference** with other ESP-NOW networks
- Defaults:
  - ESP-NOW encryption  
  - Challenge/response authentication  
  - Broadcast authentication  
  enabled for “reasonably safe” operation
- Arduino-simple API
- Stable send-queue management via FreeRTOS
- Auto peer registration so “boot → auto join” is easy

---

## 3. Architecture overview
- **Single hop only** (no relay/routing)
- Derived from `groupName`:
- `groupSecret` (shared secret)
- `groupId` (public group ID)
- `keyAuth` (peer registration auth key)
- `keyBcast` (broadcast auth key)
- Sending is queued and processed **one at a time (serial)**
- Auto peer registration:
  - Every node can broadcast join requests
  - Respond to recruitment with valid groupId/auth and register peer (no roles/accept flags)
  - Recruitment is automatically broadcast every 30s by default; set `0` to disable auto recruitment

### Group-oriented communication
- Use **broadcast** and **unicast** of ESP-NOW appropriately  
  - Broadcast: Not encrypted, only signed with `groupName`-derived key. Reaches anyone; prevents tampering/impersonation. Use for one-to-many notifications or peer discovery.  
  - Unicast: Encrypted by default. Reaches only the target so confidential data is OK. Delivery confirmation is “logical” including payload.
- Packets from outside the group are dropped by signature verification, creating a “softly closed” network with just the same `groupName`.

---

## 4. Auto peer registration
- No roles or `canAcceptRegistrations`; respond/register if groupId/auth are valid
- Recruitment (JOIN request) is broadcast; auto every 30s by default  
  - Set `0` to disable auto recruitment and only send when calling `sendJoinRequest()` manually  
  - Use full broadcast (`targetMac = ff:ff:ff:ff:ff:ff`) or targeted recruitment by specifying `targetMac`
- Applicant validates groupId/auth and auto-calls `addPeer()`

---

## 5. Security model

### 5.1 Derivation from groupName
```
groupName → groupSecret
groupSecret → groupId / keyAuth / keyBcast
```

### 5.2 ESP-NOW encryption
- **Default: ON**  
  → Up to 6 peers  
- Turning OFF allows up to 20 peers but payload is plaintext

### 5.3 Challenge/response (JOIN)
- Executed on JOIN/re-JOIN (Config.enablePeerAuth default ON; disable if needed)
- Mutual authentication using `keyAuth`
- Provides minimal auth even when encryption is OFF (turning enablePeerAuth OFF removes this protection)

### 5.4 Broadcast authentication
- Broadcast packets always include
  - `groupId`
  - `seq`
  - `authTag = HMAC(keyBcast, ...)`
- Blocks other groups/fake packets
- Payload is not encrypted, so it reaches outsiders; prevents tampering/impersonation but do not use for secrets

### 5.5 Logical delivery confirmation for unicast
- Physical ACK alone reports success even if decrypt fails; with `enableAppAck=true`, verify HMAC including payload before completion
- With app-ACK disabled, only ESP-NOW physical ACK is used (lower guarantee). See 8.1 for details.

---

## 6. Packet structure (logical spec)

### 6.1 BaseHeader (common)
- `magic` (1): EspNowBus packet marker
- `version` (1)
- `type` (1): PacketType
- `flags` (1): bit flags  
  - bit0: `isRetry` (1 when re-sending same `msgId`/`seq`)  
  - bit1–7: reserved
- `id` (2): msgId for Unicast, seq for Broadcast/JOIN

### 6.2 PacketType list
- `DataUnicast`  
- `DataBroadcast`  
- `ControlJoinReq` / `ControlJoinAck`  
- `ControlHeartbeat`
- `ControlAppAck` (logical ACK)
- `ControlLeave` (explicit leave notice)

### 6.3 Behavior by type
#### DataUnicast
- `[BaseHeader][msgId][UserPayload]`
- No groupId
- Only accepted from existing peers
- `msgId` monotonically increases per sender (uint16, wraps). Retries use same `msgId` with `flags.isRetry=1`
- Receiver keeps last processed msgId per peer; duplicates are dropped (see later)

#### DataBroadcast
- `[BaseHeader][groupId][seq][authTag][UserPayload]`
- Delivered to onReceive only if groupId/authTag are valid
- `seq` monotonically increases per sender. Retries use same `seq` with `flags.isRetry=1`

#### ControlJoinReq / Ack / AppAck (fixed length)
- Common: attach `groupId(4, LE)` + `authTag(16)`, HMAC with `keyAuth`
- ControlJoinReq (broadcast):
  - `BaseHeader` (id=seq)
  - `groupId`
  - `nonceA[8]`
  - `prevToken[8]` (responder’s previous nonceB, or zero)
  - `targetMac[6]` (`ff:..:ff` for open recruitment; specify MAC for targeted)
  - `authTag = HMAC(keyAuth, header..targetMac)`
- ControlJoinAck (broadcast; only applicant with matching nonceA/targetMac accepts):
  - `BaseHeader` (id=seq)
  - `groupId`
  - `nonceA[8]` (echo)
  - `nonceB[8]` (new)
  - `targetMac[6]` (applicant MAC = JoinReq sender)
  - `authTag = HMAC(keyAuth, header..nonceB..targetMac)`
- ControlJoinReq/Ack are sent **without ESP-NOW encryption even when useEncryption=true** (peer not yet registered). HMAC prevents tampering/impersonation. Ack side adds peer then uses encrypted unicast. Broadcast Ack keeps reachability even if only one side kept the peer.
- ControlHeartbeat (unicast):
  - `BaseHeader` (id = msgId)
  - `groupId`
  - `kind` (1 byte: 0=Ping, 1=Pong)
  - `authTag = HMAC(keyAuth, header..kind)`
  - On Ping: update liveness and send Pong (no AppAck). Pong reception = heartbeat success
- ControlAppAck (unicast logical ACK):
  - `BaseHeader` (id = msgId)
  - `groupId`
  - `msgId` (2 bytes, LE)
  - `authTag = HMAC(keyAuth, header..msgId)`
  - Used only for unicast; auto-sent when `enableAppAck=true` (see separate section)
- ControlLeave (broadcast):
  - `BaseHeader` (id = seq)
  - `groupId`
  - `authTag = HMAC(keyBcast, header..groupId)`
  - Sent by end(sendLeave=true). Uses same monotonic seq as DataBroadcast and replay window filters duplicates. Broadcast once, no retries; wait briefly before shutdown.

---

## 7. API (high level)

### 7.1 Config struct

```cpp
struct Config {
    const char* groupName;                  // required

    bool useEncryption        = true;       // ESP-NOW encryption
    bool enablePeerAuth       = true;       // challenge/response ON
    bool enableAppAck = true;               // default ON; OFF = rely on physical ACK

    // Radio
    int8_t channel = -1;                    // -1 auto from groupName hash (1–13); otherwise clipped
    wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L; // default 11M; raise if you need throughput

    uint16_t maxQueueLength   = 16;         // TX queue length
    uint16_t maxPayloadBytes  = 1470;       // payload limit (ESP-NOW v2.0). Use 250 for compatibility
    uint32_t sendTimeoutMs    = 50;         // enqueue timeout: 0=non-block, portMAX_DELAY=forever
    uint8_t  maxRetries       = 1;          // retry count (excluding first send). 0 = no retry
    uint16_t retryDelayMs     = 0;          // delay between retries; default 0 for immediate retry
    uint32_t txTimeoutMs      = 120;        // in-flight send timeout
    uint32_t autoJoinIntervalMs = 30000;    // auto JOIN interval; 0 to disable

    // Heartbeat
    uint32_t heartbeatIntervalMs = 10000;   // heartbeat base interval. 1x ping, 2x targeted JOIN, 3x drop

    // TX task (queue worker) RTOS settings
    int8_t taskCore = ARDUINO_RUNNING_CORE; // -1 unpinned, 0/1 pinned; default same as loop core
    UBaseType_t taskPriority = 3;           // 1–5; above loop(1), below WiFi tasks(4–5) recommended
    uint16_t taskStackSize = 4096;          // worker stack size (bytes)

    // Replay window (configurable)
    uint16_t replayWindowBcast = 32;        // Broadcast: max 16 senders, 32-bit window; evict oldest sender when over

};
```

### 7.2 Logging
- Use ESP-IDF log macros (`ESP_LOGE/W/I/D/V`), default tag `"EspNowBus"`.
- Typical outputs:
  - `ESP_LOGE`: `begin` failure (esp_now_init/malloc/task), peer add failure, HMAC verify failure (with cause), send failure after retries
  - `ESP_LOGW`: send timeout, queue full drop, JOIN replay detected, unknown PacketType received
  - `ESP_LOGI`: `begin` success, JOIN success (Ack), peer add/remove
  - `ESP_LOGD/V`: Debug details (seq/msgId/retry counts, etc.)

### 7.3 EspNowBus class

```cpp
class EspNowBus {
public:
    bool begin(const Config& cfg);

    bool begin(const char* groupName,
               bool useEncryption = true,
               uint16_t maxQueueLength = 16);

    void end(bool stopWiFi = false, bool sendLeave = true);

    // timeoutMs: 0=non-block, portMAX_DELAY=forever, kUseDefault=Config.sendTimeoutMs
    bool sendTo(const uint8_t mac[6], const void* data, size_t len, uint32_t timeoutMs = kUseDefault);
    bool sendToAllPeers(const void* data, size_t len, uint32_t timeoutMs = kUseDefault);
    bool broadcast(const void* data, size_t len, uint32_t timeoutMs = kUseDefault);

    // JOIN recruitment (broadcast or targeted)
    bool sendJoinRequest(const uint8_t targetMac[6] = kBroadcastMac, uint32_t timeoutMs = kUseDefault);

    // Event callbacks
    void onReceive(ReceiveCallback cb);       // data received (mac, data, len, wasRetry, isBroadcast)
    void onSendResult(SendResultCallback cb); // send complete/fail
    void onAppAck(AppAckCallback cb);         // logical ACK received
    void onJoinEvent(JoinEventCb cb);         // JOIN accept/reject/success/leave (timeout or explicit)

// onJoinEvent flags (signature is fixed: mac, accepted, isAck)
// accepted=true,  isAck=false : JoinReq accepted (we sent Ack)
// accepted=true,  isAck=true  : JoinAck received → JOIN success
// accepted=false, isAck=true  : JoinAck failed (nonce mismatch, etc.)
// accepted=false, isAck=false : heartbeat 3× timeout or ControlLeave received → peer removed

    // Peer management
    bool addPeer(const uint8_t mac[6]);
    bool removePeer(const uint8_t mac[6]);
    bool hasPeer(const uint8_t mac[6]) const;
    size_t peerCount() const;
    bool getPeer(size_t index, uint8_t macOut[6]) const;

    // Queue status
    uint16_t sendQueueFree() const;
    uint16_t sendQueueSize() const;
};

// Special timeout values
static constexpr uint32_t kUseDefault = portMAX_DELAY - 1; // use Config.sendTimeoutMs; portMAX_DELAY = forever
static constexpr uint8_t  kBroadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}; // JOIN full broadcast
static constexpr uint16_t kMaxPayloadDefault = 1470; // ESP-NOW v2.0 MTU
static constexpr uint16_t kMaxPayloadLegacy  = 250;  // compatibility size
```

`end(stopWiFi=false, sendLeave=true)` behavior (argument order):
- stopWiFi: `true` stops Wi-Fi/ESP-NOW for power saving; `false` keeps Wi-Fi on
- sendLeave: `true` discards the TX queue, sends `ControlLeave` once (no retry, not queued), waits briefly (or until send complete/fail/txTimeout) then cleans up send task and ESP-NOW. `false` exits quietly without sending leave (recruit/respond and RX stop)

Value guidance:
- Full MTU: `maxPayloadBytes = 1470` (default)  
- Compatibility/memory: set `maxPayloadBytes = 250` (kMaxPayloadLegacy) and shrink `maxQueueLength` as needed.

Radio:
- `channel`: -1 maps `groupId` to 1–13 automatically. Explicit values are clipped to 1–13.
- `phyRate`: pass `wifi_phy_rate_t` (default `WIFI_PHY_RATE_11M_L`; raise for speed). Unsupported values fall back to default. ESP-IDF 5.1+ applies per peer (unicast & broadcast peer).

---

## 8. Behavior

### 8.1 Sending
- sendTo / broadcast / sendToAllPeers → enqueue
- Internal FreeRTOS task handles one item at a time
- Completion is reported via onSendResult
- Completion policy  
  - **Unicast + enableAppAck=true**: complete only after receiving AppAck with verified HMAC (payload included)  
  - **Unicast + enableAppAck=false**: rely on ESP-NOW physical ACK  
  - **Broadcast**: no ACK; wait based on payload length and PHY, then mark complete (next send waits in queue)
- Unicast logical ACK (enableAppAck=true)  
  - Receiver auto-replies AppAck with msgId; sender completes on receipt  
  - Physical ACK without logical ACK → “unknown” → retry or re-JOIN  
  - Logical ACK without physical ACK → treat as delivered but log warning  
  - With app-ACK disabled, `SentOk` is the completion signal; no logical ACK sent/received
- Heartbeat uses `ControlHeartbeat` unicast (default 10s Ping → Pong confirms; no AppAck)
- `len > Config.maxPayloadBytes` → enqueue fails immediately
- `maxPayloadBytes` is clipped to IDF `ESP_NOW_MAX_DATA_LEN(_V2)` upper, and header minimum lower. Usable payload ≈ `maxPayloadBytes - 6` for Unicast, ≈ `maxPayloadBytes - 6 - 4 - 16` for Broadcast/Control.
- TX queue memory is pre-allocated in `begin()`; no malloc later  
  - Payload kept in fixed-size buffers (`maxPayloadBytes`)  
  - Queue stores metadata only (pointer/len/dest) using FreeRTOS Queue  
  - Memory estimate: `maxPayloadBytes * maxQueueLength` + metadata (e.g., 1470B × 16 ≈ 24KB + α)  
  - If allocation fails, `begin()` returns false
- Enqueue timeout uses `timeoutMs` argument; `kUseDefault` = `Config.sendTimeoutMs`  
  - `timeoutMs = 0`: non-block  
  - `timeoutMs = portMAX_DELAY`: block forever (not usable from ISR)  
  - `kUseDefault` uses `portMAX_DELAY - 1` special value
- Send task: single in-flight slot  
  - Keep “sending” flag and `currentTx`. Send immediately, record start time, set flag  
  - ESP-NOW send callback uses task notification (`xTaskNotifyFromISR`) to pass result; doesn’t touch state directly  
  - Task clears flag on notification and emits onSendResult  
  - If flag stays set beyond `txTimeoutMs`, treat as timeout → retry/abort
- Retries: on timeout or ESP-NOW failure, resend same `msgId/seq` up to `Config.maxRetries` (immediate if `retryDelayMs=0`)  
  - `retryDelayMs` inserts delay (use for backoff)  
  - Set `flags.isRetry=1` on retry  
  - If all attempts fail, onSendResult reports `SendFailed`
- Send task pinned to ARDUINO_RUNNING_CORE by default, priority 3, stack 4096B  
  - `taskCore = -1` unpinned; 0/1 to pin  
  - Don’t set priority too high or WiFi/ESP-NOW tasks may starve
- Physical ACK returns even if decryption fails; `onSendResult(SentOk)` means physical success only, not logical delivery  
- Auto purge is removed; manage reconnection/drop via heartbeat and targeted recruitment

### 8.2 Receiving
- Branch by PacketType from BaseHeader
- DataUnicast → only authenticated peers
- DataBroadcast → verify groupId & authTag
- ControlJoinReq → pass to auto peer registration
- ControlHeartbeat → verify HMAC, update liveness on Ping and send Pong (no AppAck)
- ControlLeave → verify groupId/authTag, remove sender peer immediately; stop heartbeat/targeted recruitment to that peer and revert to waiting for their future recruitment

### 8.3 Auto peer registration
- Recruitment is every 30s by default. `0` disables auto recruitment; app calls `sendJoinRequest()` when needed  
  - Interval adjustable to any `>0` ms  
  - Use open recruitment (`targetMac = ff:..:ff`) or targeted by specifying `targetMac` (handy for re-joining known peers)
- Applicant decision  
  - Drop if groupId mismatch  
  - If `targetMac` is not `ff:..:ff`, only respond when it matches self MAC; otherwise ignore  
  - From non-peers → apply  
  - From existing peer → check last heartbeat and send-fail count; re-apply if link may be dead, ignore if healthy
- Even if one side rebooted and unicast fails, broadcast recruitment (targeted if needed) restores pairing

#### JOIN sequence (requester → accepter)
1. Requester calls `sendJoinRequest(targetMac=ff:ff:ff:ff:ff:ff or specific)`  
2. Requester broadcasts `ControlJoinReq` (groupId + authTag + targetMac, plaintext)  
3. Accepter receives `ControlJoinReq`, verifies `groupId/authTag` (process only if `targetMac` matches self or broadcast)  
4. Accepter sends `ControlJoinAck` via broadcast (plaintext, keyAuth HMAC)  
5. Accepter adds applicant MAC as peer after sending Ack, then uses encrypted unicast  
6. Requester receives Ack (nonceA/targetMac match), adds peer, then uses encrypted unicast

JOIN replay considerations:
- No window; accept only matching nonceA/nonceB/targetMac with HMAC (only that recruitment)
- Old JOIN/Ack may arrive but heartbeat/send-fail counters suppress immediate re-JOIN
- Forging ControlJoinAck requires MAC spoof + matching nonce/HMAC

### 8.4 Duplicate detection / retries
- Unicast: keep last `msgId` per peer; same `msgId` (retry) is dropped (optionally signal “wasRetry” to onReceive)  
- Broadcast: re-send `seq` is dropped after authTag verify using replay window. `flags.isRetry` is debug only  
- Replay window width 32; accept only closest future direction on overflow. Broadcast supports max 16 senders, 32-bit window; evict oldest sender when over
- Logical ACK: even if receiver flags duplicate and omits UserPayload, it still replies Ack when `enableAppAck=true` (prevents sender retries)
- onSendResult statuses: `Queued`, `SentOk`, `SendFailed`, `Timeout`, `DroppedFull`, `DroppedOldest`, `TooLarge`, `Retrying`, `AppAckReceived`, `AppAckTimeout`
- ControlAppAck replay: accept only when msgId matches in-flight; otherwise ignore (warn). 16-bit msgId wrap may rarely cause false completion, accepted risk
- JOIN replay: no window; rely on nonceA/B/targetMac + HMAC and heartbeat/send-fail for re-JOIN control (don’t re-register immediately on old JOIN)

### 8.5 Heartbeat and peer retention
- Any of: unicast RX, encrypted soft ACK, `ControlHeartbeat` RX resets “last seen” and failure count
- If elapsed > `Config.heartbeatIntervalMs` (default 10s): send unicast `ControlHeartbeat(Ping)` (if peer removed, decrypt fails and won’t be received). On Ping RX, update liveness and send Pong (no AppAck)
- Elapsed > 2× heartbeat: send targeted broadcast recruitment including peer MAC to try re-pairing
- Elapsed > 3× heartbeat: consider dead and remove peer
- Targeted recruitment prioritizes recovery when one side rebooted and unicast broke

### 8.6 Explicit leave
- Calling `end(stopWiFi=false, sendLeave=true)` discards the TX queue, sends `ControlLeave` broadcast once (no retries, not queued), waits for completion/failure/txTimeout or a fixed short delay, then stops send task and ESP-NOW
- ControlLeave is a keyBcast-signed leave notice. Receiver verifies and immediately deletes sender peer, halting heartbeat reconnection/targeted recruitment to that peer (future re-JOIN waits for their recruitment)
- `end(false, false)` exits quietly without sending leave; stops auto recruitment/heartbeat/RX
- `end(true, sendLeave)` also stops Wi-Fi/ESP-NOW for power savings (keep false if Wi-Fi is needed for other uses)
- After end, internal state (queue/peers) is discarded; call `begin()` again to rejoin
- ControlLeave RX and heartbeat 3× timeout both notify `onJoinEvent(mac, false, false)`
- No cooldown after leave; rejoin by calling `begin()` then autoJoin/sendJoinRequest. Intended for temporary unavailability (firmware update, planned reboot)

---

## 9. Use cases
- Sensor node → gateway  
- Controller → multiple robots  
- Small multiplayer game network  
- Local interaction gadgets at events

---

## 10. Out of scope for v1
- Multi-hop/mesh/routing
- Lobby management (recruitment period control)
- Player ID management
- Advanced node role control
