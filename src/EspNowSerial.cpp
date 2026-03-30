#include "EspNowSerial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EspNowSerial *EspNowSerial::instance_ = nullptr;

namespace
{
    template <typename T>
    T minValue(T a, T b)
    {
        return a < b ? a : b;
    }
}

bool EspNowSerialPort::attach(EspNowSerial &hub)
{
    detach();
    hub_ = &hub;
    return true;
}

void EspNowSerialPort::resolveBinding()
{
    if (!hub_ || boundSession_ >= 0)
        return;

    switch (bindMode_)
    {
    case BindMode::FirstAvailable:
    {
        int idx = hub_->firstConnectedSession();
        if (idx >= 0)
        {
            hub_->portBind(*this, idx);
        }
        break;
    }
    case BindMode::ByMac:
    {
        int idx = hub_->findSessionByMac(desiredMac_);
        if (idx >= 0)
        {
            hub_->portBind(*this, idx);
        }
        break;
    }
    case BindMode::BySession:
        if (desiredSession_ < hub_->config_.sessionCount)
        {
            hub_->portBind(*this, static_cast<int>(desiredSession_));
        }
        break;
    case BindMode::None:
    default:
        break;
    }
}

void EspNowSerialPort::detach()
{
    if (hub_ && boundSession_ >= 0 && static_cast<size_t>(boundSession_) < hub_->config_.sessionCount)
    {
        hub_->sessions_[boundSession_].portBound = false;
    }
    hub_ = nullptr;
    boundSession_ = -1;
    bindMode_ = BindMode::None;
    memset(desiredMac_, 0, sizeof(desiredMac_));
    desiredSession_ = 0;
}

bool EspNowSerialPort::bind(const uint8_t mac[6])
{
    if (!hub_ || !mac)
        return false;
    memcpy(desiredMac_, mac, sizeof(desiredMac_));
    bindMode_ = BindMode::ByMac;
    int idx = hub_->findSessionByMac(mac);
    if (idx >= 0)
    {
        return hub_->portBind(*this, idx);
    }
    boundSession_ = -1;
    return true;
}

bool EspNowSerialPort::bindSession(size_t index)
{
    if (!hub_)
        return false;
    bindMode_ = BindMode::BySession;
    desiredSession_ = index;
    if (index >= hub_->config_.sessionCount)
        return false;
    return hub_->portBind(*this, static_cast<int>(index));
}

bool EspNowSerialPort::bindFirstAvailable()
{
    if (!hub_)
        return false;
    bindMode_ = BindMode::FirstAvailable;
    int idx = hub_->firstConnectedSession();
    if (idx >= 0)
    {
        return hub_->portBind(*this, idx);
    }
    boundSession_ = -1;
    return true;
}

void EspNowSerialPort::unbind()
{
    if (hub_ && boundSession_ >= 0 && static_cast<size_t>(boundSession_) < hub_->config_.sessionCount)
    {
        hub_->sessions_[boundSession_].portBound = false;
    }
    boundSession_ = -1;
    bindMode_ = BindMode::None;
}

bool EspNowSerialPort::connected() const
{
    if (!hub_)
        return false;
    const_cast<EspNowSerialPort *>(this)->resolveBinding();
    if (boundSession_ < 0 || static_cast<size_t>(boundSession_) >= hub_->config_.sessionCount)
        return false;
    const auto &session = hub_->sessions_[boundSession_];
    return session.inUse && session.connected;
}

bool EspNowSerialPort::bound() const
{
    if (hub_)
    {
        const_cast<EspNowSerialPort *>(this)->resolveBinding();
    }
    return hub_ && boundSession_ >= 0;
}

EspNowSerialPort::operator bool() const
{
    return connected();
}

int EspNowSerialPort::available()
{
    if (!hub_)
        return 0;
    hub_->pollIfNeeded();
    resolveBinding();
    if (!connected())
        return 0;
    return static_cast<int>(hub_->sessions_[boundSession_].rx.size);
}

int EspNowSerialPort::availableForWrite()
{
    if (!hub_)
        return 0;
    hub_->pollIfNeeded();
    resolveBinding();
    if (!connected())
        return 0;
    const auto &tx = hub_->sessions_[boundSession_].tx;
    return static_cast<int>(tx.capacity - tx.size);
}

int EspNowSerialPort::peek()
{
    if (!hub_)
        return -1;
    hub_->pollIfNeeded();
    resolveBinding();
    if (!connected())
        return -1;
    return hub_->ringPeekByte(hub_->sessions_[boundSession_].rx);
}

int EspNowSerialPort::read()
{
    if (!hub_)
        return -1;
    hub_->pollIfNeeded();
    resolveBinding();
    if (!connected())
        return -1;
    uint8_t byte = 0;
    return hub_->ringRead(hub_->sessions_[boundSession_].rx, &byte, 1, true) == 1 ? byte : -1;
}

size_t EspNowSerialPort::read(uint8_t *buffer, size_t size)
{
    if (!hub_ || !buffer || size == 0)
        return 0;
    hub_->pollIfNeeded();
    resolveBinding();
    if (!connected())
        return 0;
    return hub_->ringRead(hub_->sessions_[boundSession_].rx, buffer, size, true);
}

size_t EspNowSerialPort::readBytes(uint8_t *buffer, size_t length)
{
    return read(buffer, length);
}

void EspNowSerialPort::flush()
{
    if (!hub_)
        return;
    resolveBinding();
    for (uint8_t i = 0; i < 4; ++i)
    {
        hub_->poll();
        if (!connected() || hub_->sessions_[boundSession_].tx.size == 0)
            return;
        delay(1);
    }
}

size_t EspNowSerialPort::write(uint8_t b)
{
    return write(&b, 1);
}

size_t EspNowSerialPort::write(const uint8_t *buffer, size_t size)
{
    if (!hub_ || !buffer || size == 0)
        return 0;
    hub_->pollIfNeeded();
    resolveBinding();
    if (!connected())
        return 0;
    return hub_->ringWrite(hub_->sessions_[boundSession_].tx, buffer, size);
}

size_t EspNowSerialPort::printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    size_t written = vprintf(format, args);
    va_end(args);
    return written;
}

size_t EspNowSerialPort::vprintf(const char *format, va_list args)
{
    if (!hub_ || !format)
        return 0;

    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (len <= 0)
        return 0;

    char *buf = static_cast<char *>(malloc(static_cast<size_t>(len) + 1));
    if (!buf)
        return 0;

    vsnprintf(buf, static_cast<size_t>(len) + 1, format, args);
    size_t written = write(reinterpret_cast<const uint8_t *>(buf), static_cast<size_t>(len));
    free(buf);
    return written;
}

bool EspNowSerial::begin(const Config &cfg)
{
    if (!cfg.groupName || cfg.sessionCount == 0 || cfg.rxBufferSize == 0 || cfg.txBufferSize == 0)
        return false;

    end();
    config_ = cfg;
    instance_ = this;

    if (!allocSessions())
    {
        end();
        return false;
    }

    EspNowBus::Config busCfg;
    busCfg.groupName = cfg.groupName;
    busCfg.useEncryption = cfg.useEncryption;
    busCfg.enablePeerAuth = cfg.enablePeerAuth;
    busCfg.enableAppAck = cfg.enableAppAck;
    busCfg.channel = cfg.channel;
    busCfg.phyRate = cfg.phyRate;
    busCfg.maxQueueLength = cfg.maxQueueLength;
    busCfg.maxPayloadBytes = cfg.maxPayloadBytes;
    busCfg.sendTimeoutMs = cfg.sendTimeoutMs;
    busCfg.maxRetries = cfg.maxRetries;
    busCfg.retryDelayMs = cfg.retryDelayMs;
    busCfg.txTimeoutMs = cfg.txTimeoutMs;
    busCfg.autoJoinIntervalMs = cfg.advertise ? cfg.autoJoinIntervalMs : 0;
    busCfg.heartbeatIntervalMs = cfg.heartbeatIntervalMs;
    busCfg.taskCore = cfg.taskCore;
    busCfg.taskPriority = cfg.taskPriority;
    busCfg.taskStackSize = cfg.taskStackSize;
    busCfg.replayWindowBcast = cfg.replayWindowBcast;

    bus_.onReceive(&EspNowSerial::onReceiveStatic);
    bus_.onJoinEvent(&EspNowSerial::onJoinEventStatic);
    if (!bus_.begin(busCfg))
    {
        end();
        return false;
    }

    running_ = true;
    lastPollMs_ = millis();
    return true;
}

void EspNowSerial::end()
{
    if (running_)
    {
        bus_.end(false, true);
    }
    running_ = false;
    freeSessions();
    instance_ = nullptr;
}

void EspNowSerial::poll()
{
    if (!running_ || inPoll_)
        return;

    inPoll_ = true;
    syncPeers();
    resolvePortBindings();

    const size_t chunkMax = maxChunkPayload();
    if (chunkMax > 0)
    {
        uint8_t *scratch = static_cast<uint8_t *>(malloc(chunkMax));
        if (scratch)
        {
            for (size_t i = 0; i < config_.sessionCount; ++i)
            {
                auto &session = sessions_[i];
                if (!session.inUse || !session.connected || session.tx.size == 0)
                    continue;

                size_t count = ringRead(session.tx, scratch, chunkMax, false);
                if (count == 0)
                    continue;
                if (sendChunk(session, scratch, count))
                {
                    ringRead(session.tx, scratch, count, true);
                }
            }
            free(scratch);
        }
    }

    lastPollMs_ = millis();
    inPoll_ = false;
}

size_t EspNowSerial::sessionCapacity() const
{
    return config_.sessionCount;
}

bool EspNowSerial::sessionInUse(size_t index) const
{
    return index < config_.sessionCount && sessions_ && sessions_[index].inUse;
}

bool EspNowSerial::sessionConnected(size_t index) const
{
    return index < config_.sessionCount && sessions_ && sessions_[index].inUse && sessions_[index].connected;
}

int EspNowSerial::sessionAvailable(size_t index) const
{
    if (index >= config_.sessionCount || !sessions_ || !sessions_[index].inUse)
        return 0;
    return static_cast<int>(sessions_[index].rx.size);
}

bool EspNowSerial::sessionMac(size_t index, uint8_t macOut[6]) const
{
    if (!macOut || index >= config_.sessionCount || !sessions_ || !sessions_[index].inUse)
        return false;
    memcpy(macOut, sessions_[index].mac, 6);
    return true;
}

bool EspNowSerial::hasSession(const uint8_t mac[6]) const
{
    return findSessionByMac(mac) >= 0;
}

void EspNowSerial::onReceiveStatic(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
    if (instance_)
    {
        instance_->onReceive(mac, data, len, wasRetry, isBroadcast);
    }
}

void EspNowSerial::onJoinEventStatic(const uint8_t mac[6], bool accepted, bool isAck)
{
    if (instance_)
    {
        instance_->onJoinEvent(mac, accepted, isAck);
    }
}

void EspNowSerial::onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
    (void)wasRetry;
    if (isBroadcast || !mac || len < sizeof(AppHeader) + sizeof(SerialDataHeader))
        return;

    const auto *app = reinterpret_cast<const AppHeader *>(data);
    if (app->protocolId != kProtocolIdSerial || app->protocolVer != kProtocolVersion || app->packetType != SerialData)
        return;

    const auto *hdr = reinterpret_cast<const SerialDataHeader *>(data + sizeof(AppHeader));
    if (hdr->endpointId != config_.endpointId)
        return;

    size_t payloadOffset = sizeof(AppHeader) + sizeof(SerialDataHeader);
    if (len < payloadOffset)
        return;

    int idx = ensureSession(mac);
    if (idx < 0)
        return;

    auto &session = sessions_[idx];
    session.connected = true;
    ringWrite(session.rx, data + payloadOffset, len - payloadOffset);
}

void EspNowSerial::onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck)
{
    (void)isAck;
    if (!mac)
        return;
    if (accepted)
    {
        int idx = ensureSession(mac);
        if (idx >= 0)
            sessions_[idx].connected = true;
        return;
    }
    markSessionDisconnected(mac);
}

bool EspNowSerial::allocSessions()
{
    sessions_ = static_cast<Session *>(calloc(config_.sessionCount, sizeof(Session)));
    if (!sessions_)
        return false;

    for (size_t i = 0; i < config_.sessionCount; ++i)
    {
        if (!initRing(sessions_[i].rx, config_.rxBufferSize) || !initRing(sessions_[i].tx, config_.txBufferSize))
        {
            freeSessions();
            return false;
        }
        esp_fill_random(&sessions_[i].sessionNonce, sizeof(sessions_[i].sessionNonce));
    }
    return true;
}

void EspNowSerial::freeSessions()
{
    if (!sessions_)
        return;
    for (size_t i = 0; i < config_.sessionCount; ++i)
    {
        freeRing(sessions_[i].rx);
        freeRing(sessions_[i].tx);
    }
    free(sessions_);
    sessions_ = nullptr;
}

bool EspNowSerial::initRing(RingBuffer &rb, size_t capacity)
{
    rb.data = static_cast<uint8_t *>(malloc(capacity));
    if (!rb.data)
        return false;
    rb.capacity = capacity;
    rb.head = 0;
    rb.tail = 0;
    rb.size = 0;
    return true;
}

void EspNowSerial::freeRing(RingBuffer &rb)
{
    free(rb.data);
    rb.data = nullptr;
    rb.capacity = 0;
    rb.head = 0;
    rb.tail = 0;
    rb.size = 0;
}

size_t EspNowSerial::ringWrite(RingBuffer &rb, const uint8_t *data, size_t len)
{
    if (!rb.data || !data || len == 0)
        return 0;

    size_t writable = minValue(len, rb.capacity - rb.size);
    for (size_t i = 0; i < writable; ++i)
    {
        rb.data[rb.head] = data[i];
        rb.head = (rb.head + 1) % rb.capacity;
    }
    rb.size += writable;
    return writable;
}

size_t EspNowSerial::ringRead(RingBuffer &rb, uint8_t *data, size_t len, bool consume)
{
    if (!rb.data || !data || len == 0)
        return 0;

    size_t readable = minValue(len, rb.size);
    size_t tail = rb.tail;
    for (size_t i = 0; i < readable; ++i)
    {
        data[i] = rb.data[tail];
        tail = (tail + 1) % rb.capacity;
    }
    if (consume)
    {
        rb.tail = tail;
        rb.size -= readable;
    }
    return readable;
}

int EspNowSerial::ringPeekByte(const RingBuffer &rb) const
{
    if (!rb.data || rb.size == 0)
        return -1;
    return rb.data[rb.tail];
}

void EspNowSerial::syncPeers()
{
    for (size_t i = 0; i < config_.sessionCount; ++i)
    {
        if (!sessions_[i].inUse)
            continue;
        sessions_[i].connected = bus_.hasPeer(sessions_[i].mac);
    }

    uint8_t mac[6];
    for (size_t i = 0; i < bus_.peerCount(); ++i)
    {
        if (bus_.getPeer(i, mac))
        {
            int idx = ensureSession(mac);
            if (idx >= 0)
                sessions_[idx].connected = true;
        }
    }
}

int EspNowSerial::findSessionByMac(const uint8_t mac[6]) const
{
    if (!sessions_ || !mac)
        return -1;
    for (size_t i = 0; i < config_.sessionCount; ++i)
    {
        if (sessions_[i].inUse && memcmp(sessions_[i].mac, mac, 6) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

int EspNowSerial::ensureSession(const uint8_t mac[6])
{
    int idx = findSessionByMac(mac);
    if (idx >= 0)
        return idx;

    for (size_t i = 0; i < config_.sessionCount; ++i)
    {
        if (!sessions_[i].inUse)
        {
            sessions_[i].inUse = true;
            sessions_[i].connected = true;
            memcpy(sessions_[i].mac, mac, 6);
            esp_fill_random(&sessions_[i].sessionNonce, sizeof(sessions_[i].sessionNonce));
            return static_cast<int>(i);
        }
    }
    return -1;
}

void EspNowSerial::markSessionDisconnected(const uint8_t mac[6])
{
    int idx = findSessionByMac(mac);
    if (idx < 0)
        return;
    sessions_[idx].connected = false;
    esp_fill_random(&sessions_[idx].sessionNonce, sizeof(sessions_[idx].sessionNonce));
}

void EspNowSerial::resolvePortBindings()
{
    // Binding is resolved lazily from port-side API or by explicit bind requests.
}

size_t EspNowSerial::maxChunkPayload() const
{
    const size_t overhead = EspNowBus::kHeaderSize + sizeof(AppHeader) + sizeof(SerialDataHeader);
    if (config_.maxPayloadBytes <= overhead)
        return 0;
    return config_.maxPayloadBytes - overhead;
}

bool EspNowSerial::sendChunk(Session &session, const uint8_t *payload, size_t len)
{
    if (!payload || len == 0)
        return false;

    const size_t totalLen = sizeof(AppHeader) + sizeof(SerialDataHeader) + len;
    uint8_t *buf = static_cast<uint8_t *>(malloc(totalLen));
    if (!buf)
        return false;

    auto *app = reinterpret_cast<AppHeader *>(buf);
    app->protocolId = kProtocolIdSerial;
    app->protocolVer = kProtocolVersion;
    app->packetType = SerialData;
    app->flags = 0;

    auto *hdr = reinterpret_cast<SerialDataHeader *>(buf + sizeof(AppHeader));
    hdr->endpointId = config_.endpointId;
    hdr->reserved = 0;
    hdr->sessionNonce = session.sessionNonce;

    memcpy(buf + sizeof(AppHeader) + sizeof(SerialDataHeader), payload, len);
    bool ok = bus_.sendTo(session.mac, buf, totalLen);
    free(buf);
    return ok;
}

void EspNowSerial::pollIfNeeded()
{
    if (!running_ || inPoll_)
        return;
    uint32_t now = millis();
    if (now - lastPollMs_ < 1)
        return;
    poll();
}

bool EspNowSerial::portBind(EspNowSerialPort &port, int sessionIndex)
{
    if (sessionIndex < 0 || static_cast<size_t>(sessionIndex) >= config_.sessionCount)
        return false;
    auto &session = sessions_[sessionIndex];
    if (!session.inUse)
        return false;

    if (port.boundSession_ >= 0 && static_cast<size_t>(port.boundSession_) < config_.sessionCount)
    {
        sessions_[port.boundSession_].portBound = false;
    }
    if (session.portBound && port.boundSession_ != sessionIndex)
    {
        return false;
    }

    port.boundSession_ = sessionIndex;
    session.portBound = true;
    return true;
}

int EspNowSerial::firstConnectedSession() const
{
    for (size_t i = 0; i < config_.sessionCount; ++i)
    {
        if (sessions_[i].inUse && sessions_[i].connected && !sessions_[i].portBound)
            return static_cast<int>(i);
    }
    return -1;
}
