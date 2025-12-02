#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// ESP32 ESP-NOW message bus (design in SPEC.ja.md). Implementation is WIP.
// APIs are stubbed so the library can be included and built while the core logic is developed.

class EspNowBus {
public:
    struct Config {
        const char* groupName;               // Required

        bool useEncryption        = true;
        bool enablePeerAuth       = true;
        bool enableBroadcastAuth  = true;

        uint16_t maxQueueLength   = 16;
        uint16_t maxPayloadBytes  = 1470;
        uint32_t sendTimeoutMs    = 50;
        uint8_t  maxRetries       = 1;
        uint16_t retryDelayMs     = 0;
        uint32_t txTimeoutMs      = 120;

        bool canAcceptRegistrations = true;

        int8_t taskCore      = ARDUINO_RUNNING_CORE; // -1 = unpinned, 0/1 = pinned core
        UBaseType_t taskPriority = 3;
        uint16_t taskStackSize   = 4096;
    };

    // sendTimeout special values
    static constexpr uint32_t kUseDefault = portMAX_DELAY - 1;
    static constexpr uint16_t kMaxPayloadDefault = 1470;
    static constexpr uint16_t kMaxPayloadLegacy  = 250;
    static constexpr uint8_t  kAuthTagLen = 16;
    static constexpr uint16_t kReplayWindow = 64;
    static constexpr uint8_t  kNonceLen = 8;
    static constexpr uint16_t kNonceWindow = 128;
    static constexpr uint32_t kReseedIntervalMs = 60 * 60 * 1000; // periodic key reseed (if desired)

    enum PacketType : uint8_t {
        DataUnicast = 1,
        DataBroadcast = 2,
        ControlJoinReq = 3,
        ControlJoinAck = 4,
        ControlAppAck = 5,
    };

    struct JoinReqPayload {
        uint8_t nonceA[kNonceLen];
        uint8_t prevToken[kNonceLen]; // responder's nonceB from previous session, or zero
    };

    struct JoinAckPayload {
        uint8_t nonceA[kNonceLen];
        uint8_t nonceB[kNonceLen];
    };

    struct AppAckPayload {
        uint16_t msgId;
    };

    enum SendStatus : uint8_t {
        Queued,
        SentOk,
        SendFailed,
        Timeout,
        DroppedFull,
        DroppedOldest,
        TooLarge,
        Retrying,
        AppAckTimeout,
        AppAckReceived
    };

    using ReceiveCallback = void (*)(const uint8_t* mac, const uint8_t* data, size_t len, bool wasRetry);
    using SendResultCallback = void (*)(const uint8_t* mac, SendStatus status);
    using AppAckCallback = void (*)(const uint8_t* mac, uint16_t msgId);

    bool begin(const Config& cfg);

    bool begin(const char* groupName,
               bool canAcceptRegistrations = true,
               bool useEncryption = true,
               uint16_t maxQueueLength = 16);

    void end();

    bool sendTo(const uint8_t mac[6], const void* data, size_t len, uint32_t timeoutMs = kUseDefault);
    bool sendToAllPeers(const void* data, size_t len, uint32_t timeoutMs = kUseDefault);
    bool broadcast(const void* data, size_t len, uint32_t timeoutMs = kUseDefault);

    void onReceive(ReceiveCallback cb);
    void onSendResult(SendResultCallback cb);
    void onAppAck(AppAckCallback cb);

    bool addPeer(const uint8_t mac[6]);
    bool removePeer(const uint8_t mac[6]);
    bool hasPeer(const uint8_t mac[6]) const;

    void setAcceptRegistration(bool enable);

    bool sendRegistrationRequest();

    // Pair management (plain, no auth yet)
    bool initPeers(const uint8_t peers[][6], size_t count);

private:
    enum class Dest : uint8_t { Unicast, Broadcast };

    struct TxItem {
        uint16_t bufferIndex;
        uint16_t len;
        uint16_t msgId;
        uint16_t seq;
        Dest dest;
        PacketType pktType;
        bool isRetry;
        uint8_t mac[6];

        // App-level ACK tracking
        bool expectAck = false;
        uint32_t appAckDeadlineMs = 0;
    };

    struct PeerInfo {
        uint8_t mac[6];
        bool inUse = false;
        uint16_t lastMsgId = 0;
        uint16_t lastBroadcastBase = 0;
        uint64_t bcastWindow = 0; // bit0 = base+1 ... bit63 = base+64

        uint16_t lastJoinSeqBase = 0;
        uint64_t joinWindow = 0;
        uint8_t lastNonceB[kNonceLen]{};
    };

    Config config_{};
    ReceiveCallback onReceive_ = nullptr;
    SendResultCallback onSendResult_ = nullptr;
    AppAckCallback onAppAck_ = nullptr;

    struct DerivedKeys {
        uint8_t pmk[16]{};      // Primary Master Key for ESP-NOW encryption
        uint8_t lmk[16]{};      // Local Master Key for peers
        uint8_t keyAuth[16]{};  // Auth HMAC key (unused yet)
        uint8_t keyBcast[16]{}; // Broadcast auth key (unused yet)
        uint32_t groupId = 0;   // Public group id
    } derived_{};

    QueueHandle_t sendQueue_ = nullptr;
    TaskHandle_t sendTask_ = nullptr;
    TaskHandle_t selfTaskHandle_ = nullptr; // for notifications

    uint8_t* payloadPool_ = nullptr;
    bool* bufferUsed_ = nullptr;
    size_t poolCount_ = 0;

    TxItem currentTx_{};
    bool txInFlight_ = false;
    uint8_t retryCount_ = 0;
    uint32_t txDeadlineMs_ = 0;

    static constexpr uint8_t kMagic = 0xEB;
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 6; // magic(1)+ver(1)+type(1)+flags(1)+id(2: msgId or seq)

    uint16_t msgCounter_ = 0;
    uint16_t broadcastSeq_ = 0;

    static constexpr size_t kMaxPeers = 20;
    PeerInfo peers_[kMaxPeers];

    static EspNowBus* instance_;

    bool pendingJoin_ = false;
    uint8_t pendingNonceA_[kNonceLen]{};
    uint8_t storedNonceB_[kNonceLen]{};
    bool storedNonceBValid_ = false;
    uint32_t lastReseedMs_ = 0;

    // App-level ACK tracking
    uint16_t waitingAppAckId_ = 0;

    static void onSendStatic(const uint8_t* mac, esp_now_send_status_t status);
    static void onReceiveStatic(const uint8_t* mac, const uint8_t* data, int len);
    static void sendTaskTrampoline(void* arg);

    void sendTaskLoop();
    void handleSendComplete(bool ok, bool timedOut);
    bool sendNextIfIdle(TickType_t waitTicks);
    bool startSend(const TxItem& item);
    bool enqueueCommon(Dest dest, PacketType pktType, const uint8_t* mac, const void* data, size_t len, uint32_t timeoutMs);
    int findPeerIndex(const uint8_t mac[6]) const;
    int ensurePeer(const uint8_t mac[6]);
    uint8_t* bufferPtr(uint16_t idx);
    int16_t allocBuffer();
    void freeBuffer(uint16_t idx);
    bool deriveKeys(const char* groupName);
    void computeAuthTag(uint8_t* out, const uint8_t* msg, size_t len, const uint8_t* key);
    bool verifyAuthTag(const uint8_t* msg, size_t len, uint8_t pktType);
    bool acceptBroadcastSeq(PeerInfo& peer, uint16_t seq);
    bool acceptJoinSeq(PeerInfo& peer, uint16_t seq);
    void reseedCounters(uint32_t now);
};
