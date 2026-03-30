#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <Print.h>
#include <stdarg.h>

#include "EspNowBus.h"

class EspNowSerial;

class EspNowSerialPort : public Stream
{
public:
    EspNowSerialPort() = default;

    bool attach(EspNowSerial &hub);
    void detach();

    bool bind(const uint8_t mac[6]);
    bool bindSession(size_t index);
    bool bindFirstAvailable();
    void unbind();

    bool connected() const;
    bool bound() const;
    operator bool() const;

    int available() override;
    int availableForWrite();
    int peek() override;
    int read() override;
    size_t read(uint8_t *buffer, size_t size);
    size_t readBytes(uint8_t *buffer, size_t length);
    void flush() override;
    size_t write(uint8_t b) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    size_t printf(const char *format, ...);
    size_t vprintf(const char *format, va_list args);

    using Print::write;

private:
    enum class BindMode : uint8_t
    {
        None,
        FirstAvailable,
        ByMac,
        BySession
    };

    EspNowSerial *hub_ = nullptr;
    int boundSession_ = -1;
    BindMode bindMode_ = BindMode::None;
    uint8_t desiredMac_[6]{};
    size_t desiredSession_ = 0;
    void resolveBinding();

    friend class EspNowSerial;
};

class EspNowSerial
{
public:
    struct Config
    {
        const char *groupName = nullptr;
        uint8_t endpointId = 0;
        size_t sessionCount = 8;
        size_t rxBufferSize = 512;
        size_t txBufferSize = 512;
        bool autoReconnect = true;
        bool advertise = true;

        // EspNowBus transport settings
        bool useEncryption = true;
        // ESP-NOW v2 default. Effective Serial payload is smaller because
        // EspNowBus and EspNowSerial add headers on top.
        uint16_t maxPayloadBytes = EspNowBus::kMaxPayloadDefault;
        bool enablePeerAuth = true;
        bool enableAppAck = true;
        int8_t channel = -1;
        wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L;
        uint16_t maxQueueLength = 16;
        uint32_t sendTimeoutMs = 50;
        uint8_t maxRetries = 1;
        uint16_t retryDelayMs = 0;
        uint32_t txTimeoutMs = 120;
        uint32_t autoJoinIntervalMs = 30000;
        uint32_t heartbeatIntervalMs = 10000;
        int8_t taskCore = ARDUINO_RUNNING_CORE;
        UBaseType_t taskPriority = 3;
        uint16_t taskStackSize = 4096;
        uint16_t replayWindowBcast = 32;
    };

    EspNowSerial() = default;

    bool begin(const Config &cfg);
    void end();
    void poll();

    size_t sessionCapacity() const;
    bool sessionInUse(size_t index) const;
    bool sessionConnected(size_t index) const;
    int sessionAvailable(size_t index) const;
    bool sessionMac(size_t index, uint8_t macOut[6]) const;
    bool hasSession(const uint8_t mac[6]) const;

private:
    struct RingBuffer
    {
        uint8_t *data = nullptr;
        size_t capacity = 0;
        size_t head = 0;
        size_t tail = 0;
        size_t size = 0;
    };

    struct Session
    {
        bool inUse = false;
        bool connected = false;
        bool portBound = false;
        uint8_t mac[6]{};
        uint32_t sessionNonce = 0;
        RingBuffer rx;
        RingBuffer tx;
    };

    static constexpr uint8_t kProtocolIdSerial = 0x01;
    static constexpr uint8_t kProtocolVersion = 1;
    enum PacketType : uint8_t
    {
        SerialHello = 1,
        SerialHelloAck = 2,
        SerialData = 3,
        SerialClose = 4,
    };

#pragma pack(push, 1)
    struct AppHeader
    {
        uint8_t protocolId;
        uint8_t protocolVer;
        uint8_t packetType;
        uint8_t flags;
    };

    struct SerialDataHeader
    {
        uint8_t endpointId;
        uint8_t reserved;
        uint32_t sessionNonce;
    };
#pragma pack(pop)

    static EspNowSerial *instance_;

    Config config_{};
    EspNowBus bus_{};
    Session *sessions_ = nullptr;
    uint32_t lastPollMs_ = 0;
    bool inPoll_ = false;
    bool running_ = false;

    static void onReceiveStatic(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast);
    static void onJoinEventStatic(const uint8_t mac[6], bool accepted, bool isAck);

    void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast);
    void onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck);

    bool allocSessions();
    void freeSessions();
    bool initRing(RingBuffer &rb, size_t capacity);
    void freeRing(RingBuffer &rb);
    size_t ringWrite(RingBuffer &rb, const uint8_t *data, size_t len);
    size_t ringRead(RingBuffer &rb, uint8_t *data, size_t len, bool consume);
    int ringPeekByte(const RingBuffer &rb) const;
    void syncPeers();
    int findSessionByMac(const uint8_t mac[6]) const;
    int ensureSession(const uint8_t mac[6]);
    void markSessionDisconnected(const uint8_t mac[6]);
    void resolvePortBindings();
    size_t maxChunkPayload() const;
    bool sendChunk(Session &session, const uint8_t *payload, size_t len);
    void pollIfNeeded();
    bool portBind(EspNowSerialPort &port, int sessionIndex);
    int firstConnectedSession() const;

    friend class EspNowSerialPort;
};
