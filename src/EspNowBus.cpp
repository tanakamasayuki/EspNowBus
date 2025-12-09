#include "EspNowBus.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include "esp_log.h"

static const char *TAG = "EspNowBus";

EspNowBus *EspNowBus::instance_ = nullptr;

namespace
{
    esp_now_peer_info_t makePeerInfo(const uint8_t mac[6], bool encrypt, const uint8_t *lmk)
    {
        esp_now_peer_info_t info{};
        memcpy(info.peer_addr, mac, 6);
        info.ifidx = WIFI_IF_STA;
        info.channel = 0;
        if (encrypt && lmk)
        {
            info.encrypt = 1;
            memcpy(info.lmk, lmk, 16);
        }
        else
        {
            info.encrypt = 0;
        }
        return info;
    }
} // namespace

bool EspNowBus::begin(const Config &cfg)
{
    if (!cfg.groupName || cfg.maxQueueLength == 0 || cfg.maxPayloadBytes == 0)
    {
        ESP_LOGE(TAG, "invalid config (groupName/null or zero lengths)");
        return false;
    }
    // ensure single instance
    instance_ = this;
    config_ = cfg;

    uint16_t cap = config_.maxPayloadBytes;
#ifdef ESP_NOW_MAX_DATA_LEN_V2
    if (cap > ESP_NOW_MAX_DATA_LEN_V2)
        cap = ESP_NOW_MAX_DATA_LEN_V2;
#else
    if (cap > ESP_NOW_MAX_DATA_LEN)
        cap = ESP_NOW_MAX_DATA_LEN;
#endif
    if (cap < kHeaderSize + 4)
        cap = kHeaderSize + 4;
    if (cap != config_.maxPayloadBytes)
    {
        ESP_LOGW(TAG, "maxPayloadBytes clipped to %u", cap);
    }
    config_.maxPayloadBytes = cap;

    if (!deriveKeys(config_.groupName))
    {
        ESP_LOGE(TAG, "key derivation failed");
        return false;
    }

    // Wi-Fi channel: -1 = auto (hash from group), otherwise clip to 1-13
    if (config_.channel == -1)
    {
        config_.channel = static_cast<int8_t>((derived_.groupId % 13) + 1);
        ESP_LOGI(TAG, "auto channel -> %d", static_cast<int>(config_.channel));
    }
    else
    {
        if (config_.channel < 1)
            config_.channel = 1;
        if (config_.channel > 13)
            config_.channel = 13;
    }

#if defined(WIFI_PHY_RATE_MAX)
    if (config_.phyRate >= WIFI_PHY_RATE_MAX)
    {
        ESP_LOGW(TAG, "phyRate out of range, fallback to WIFI_PHY_RATE_1M_L");
        config_.phyRate = WIFI_PHY_RATE_1M_L;
    }
#endif

    if (config_.replayWindowBcast > 32)
        config_.replayWindowBcast = 32;

    WiFi.mode(WIFI_STA);
    esp_wifi_get_mac(WIFI_IF_STA, selfMac_);
    // Prime auto-join so the first loop run triggers immediately when enabled
    if (config_.autoJoinIntervalMs > 0)
        lastAutoJoinMs_ = millis() - config_.autoJoinIntervalMs;
    else
        lastAutoJoinMs_ = millis();
    esp_err_t chErr = esp_wifi_set_channel(static_cast<uint8_t>(config_.channel), WIFI_SECOND_CHAN_NONE);
    if (chErr != ESP_OK)
    {
        ESP_LOGW(TAG, "set channel failed ch=%d err=%d", static_cast<int>(config_.channel), static_cast<int>(chErr));
    }
    if (esp_now_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_now_init failed");
        return false;
    }
    if (config_.useEncryption)
    {
        esp_now_set_pmk(derived_.pmk);
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    // PHY レートは peer 追加時に per-peer で設定する
    esp_err_t rateErr = ESP_OK;
#else
    esp_err_t rateErr = esp_wifi_config_espnow_rate(WIFI_IF_STA, config_.phyRate);
#endif
    if (rateErr != ESP_OK)
    {
        ESP_LOGW(TAG, "set phy rate failed rate=%d err=%d", static_cast<int>(config_.phyRate), static_cast<int>(rateErr));
    }
#endif
    esp_now_register_send_cb(&EspNowBus::onSendStatic);
    esp_now_register_recv_cb(&EspNowBus::onReceiveStatic);

    // seed RNG for counters
    esp_fill_random(&msgCounter_, sizeof(msgCounter_));
    esp_fill_random(&broadcastSeq_, sizeof(broadcastSeq_));
    lastReseedMs_ = millis();

    // Ensure broadcast peer exists
    const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t bcastPeer = makePeerInfo(broadcastMac, false, nullptr);
    esp_now_add_peer(&bcastPeer);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    applyPeerRate(broadcastMac);
#endif

    // Allocate payload pool
    poolCount_ = config_.maxQueueLength;
    payloadPool_ = static_cast<uint8_t *>(heap_caps_malloc(config_.maxPayloadBytes * poolCount_, MALLOC_CAP_DEFAULT));
    bufferUsed_ = static_cast<bool *>(heap_caps_malloc(poolCount_ * sizeof(bool), MALLOC_CAP_DEFAULT));
    if (!payloadPool_ || !bufferUsed_)
    {
        ESP_LOGE(TAG, "buffer allocation failed");
        end();
        return false;
    }
    memset(bufferUsed_, 0, poolCount_);

    sendQueue_ = xQueueCreate(config_.maxQueueLength, sizeof(TxItem));
    if (!sendQueue_)
    {
        ESP_LOGE(TAG, "queue allocation failed");
        end();
        return false;
    }

    BaseType_t created = pdFAIL;
    if (config_.taskCore < 0)
    {
        created = xTaskCreate(&EspNowBus::sendTaskTrampoline, "EspNowBusSend", config_.taskStackSize, this,
                              config_.taskPriority, &sendTask_);
    }
    else
    {
        created = xTaskCreatePinnedToCore(&EspNowBus::sendTaskTrampoline, "EspNowBusSend", config_.taskStackSize, this,
                                          config_.taskPriority, &sendTask_, config_.taskCore);
    }
    if (created != pdPASS)
    {
        ESP_LOGE(TAG, "send task create failed");
        end();
        return false;
    }
    ESP_LOGI(TAG, "begin success (enc=%d, queue=%u, payload=%u, ch=%d, phy=%d)",
             config_.useEncryption, config_.maxQueueLength, config_.maxPayloadBytes,
             static_cast<int>(config_.channel), static_cast<int>(config_.phyRate));
    return true;
}

bool EspNowBus::begin(const char *groupName,
                      bool useEncryption,
                      uint16_t maxQueueLength)
{
    Config cfg;
    cfg.groupName = groupName;
    cfg.useEncryption = useEncryption;
    cfg.maxQueueLength = maxQueueLength;
    return begin(cfg);
}

void EspNowBus::end()
{
    if (sendTask_)
    {
        vTaskDelete(sendTask_);
        sendTask_ = nullptr;
    }
    if (sendQueue_)
    {
        vQueueDelete(sendQueue_);
        sendQueue_ = nullptr;
    }
    if (payloadPool_)
    {
        heap_caps_free(payloadPool_);
        payloadPool_ = nullptr;
    }
    if (bufferUsed_)
    {
        heap_caps_free(bufferUsed_);
        bufferUsed_ = nullptr;
    }
    instance_ = nullptr;
    esp_now_unregister_send_cb();
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    ESP_LOGI(TAG, "end complete");
}

bool EspNowBus::sendTo(const uint8_t mac[6], const void *data, size_t len, uint32_t timeoutMs)
{
    if (!mac)
        return false;
    ESP_LOGD(TAG, "sendTo mac=%02X:%02X:%02X:%02X:%02X:%02X len=%u timeout=%u",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             static_cast<unsigned>(len),
             static_cast<unsigned>(timeoutMs));
    return enqueueCommon(Dest::Unicast, PacketType::DataUnicast, mac, data, len, timeoutMs);
}

bool EspNowBus::sendToAllPeers(const void *data, size_t len, uint32_t timeoutMs)
{
    ESP_LOGD(TAG, "sendToAllPeers len=%u timeout=%u", static_cast<unsigned>(len), static_cast<unsigned>(timeoutMs));
    bool ok = true;
    for (size_t i = 0; i < kMaxPeers; ++i)
    {
        if (!peers_[i].inUse)
            continue;
        if (!sendTo(peers_[i].mac, data, len, timeoutMs))
        {
            ok = false;
        }
    }
    return ok;
}

bool EspNowBus::broadcast(const void *data, size_t len, uint32_t timeoutMs)
{
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ESP_LOGD(TAG, "broadcast len=%u timeout=%u", static_cast<unsigned>(len), static_cast<unsigned>(timeoutMs));
    return enqueueCommon(Dest::Broadcast, PacketType::DataBroadcast, bcast, data, len, timeoutMs);
}

void EspNowBus::onReceive(ReceiveCallback cb)
{
    onReceive_ = cb;
}

void EspNowBus::onSendResult(SendResultCallback cb)
{
    onSendResult_ = cb;
}

void EspNowBus::onAppAck(AppAckCallback cb)
{
    onAppAck_ = cb;
}

void EspNowBus::onJoinEvent(JoinEventCallback cb)
{
    onJoinEvent_ = cb;
}

bool EspNowBus::addPeer(const uint8_t mac[6])
{
    if (!mac)
        return false;
    int idx = findPeerIndex(mac);
    if (idx >= 0)
        return true;
    idx = ensurePeer(mac);
    if (idx < 0)
        return false;

    esp_now_peer_info_t info = makePeerInfo(mac, config_.useEncryption, derived_.lmk);
    esp_err_t err = esp_now_add_peer(&info);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST)
    {
        peers_[idx].inUse = false;
        ESP_LOGE(TAG, "add_peer failed err=%d", err);
        return false;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    applyPeerRate(mac);
#endif
    peers_[idx].lastSeenMs = millis();
    peers_[idx].heartbeatStage = 0;
    return true;
}

bool EspNowBus::removePeer(const uint8_t mac[6])
{
    if (!mac)
        return false;
    esp_now_del_peer(mac);
    int idx = findPeerIndex(mac);
    if (idx >= 0)
    {
        peers_[idx].inUse = false;
    }
    return true;
}

bool EspNowBus::hasPeer(const uint8_t mac[6]) const
{
    return findPeerIndex(mac) >= 0;
}

size_t EspNowBus::peerCount() const
{
    size_t cnt = 0;
    for (size_t i = 0; i < kMaxPeers; ++i)
    {
        if (peers_[i].inUse)
            ++cnt;
    }
    return cnt;
}

bool EspNowBus::getPeer(size_t index, uint8_t macOut[6]) const
{
    size_t cnt = 0;
    for (size_t i = 0; i < kMaxPeers; ++i)
    {
        if (!peers_[i].inUse)
            continue;
        if (cnt == index)
        {
            memcpy(macOut, peers_[i].mac, 6);
            return true;
        }
        ++cnt;
    }
    return false;
}

bool EspNowBus::sendJoinRequest(const uint8_t targetMac[6], uint32_t timeoutMs)
{
    const uint8_t *tgt = targetMac ? targetMac : kBroadcastMac;
    JoinReqPayload payload{};
    uint32_t t = millis();
    memcpy(payload.nonceA, &t, sizeof(t));
    esp_fill_random(payload.nonceA + sizeof(t), kNonceLen - sizeof(t));
    memcpy(pendingNonceA_, payload.nonceA, kNonceLen);
    if (storedNonceBValid_)
    {
        memcpy(payload.prevToken, storedNonceB_, kNonceLen);
    }
    else
    {
        memset(payload.prevToken, 0, kNonceLen);
    }
    memcpy(payload.targetMac, tgt, 6);
    pendingJoin_ = true;
    lastJoinReqMs_ = t;
    ESP_LOGD(TAG, "sendJoinRequest nonceA=%02X%02X... target=%02X:%02X:%02X:%02X:%02X:%02X",
             payload.nonceA[0], payload.nonceA[1],
             tgt[0], tgt[1], tgt[2], tgt[3], tgt[4], tgt[5]);
    return enqueueCommon(Dest::Broadcast, PacketType::ControlJoinReq, kBroadcastMac, &payload, sizeof(payload), timeoutMs);
}

bool EspNowBus::sendLeaveRequest(uint32_t timeoutMs)
{
    LeavePayload payload{};
    memcpy(payload.mac, selfMac_, 6);
    ESP_LOGI(TAG, "sendLeaveRequest mac=%02X:%02X:%02X:%02X:%02X:%02X",
             payload.mac[0], payload.mac[1], payload.mac[2], payload.mac[3], payload.mac[4], payload.mac[5]);
    bool ok = enqueueCommon(Dest::Broadcast, PacketType::ControlLeave, kBroadcastMac, &payload, sizeof(payload), timeoutMs);
    if (ok && onJoinEvent_)
    {
        onJoinEvent_(selfMac_, false, false);
    }
    return ok;
}

uint16_t EspNowBus::sendQueueFree() const
{
    if (!sendQueue_)
        return 0;
    return static_cast<uint16_t>(uxQueueSpacesAvailable(sendQueue_));
}

uint16_t EspNowBus::sendQueueSize() const
{
    if (!sendQueue_)
        return 0;
    return static_cast<uint16_t>(uxQueueMessagesWaiting(sendQueue_));
}

bool EspNowBus::initPeers(const uint8_t peers[][6], size_t count)
{
    bool ok = true;
    for (size_t i = 0; i < count; ++i)
    {
        if (!addPeer(peers[i]))
        {
            ok = false;
        }
    }
    return ok;
}

// --- internal helpers ---

int EspNowBus::findPeerIndex(const uint8_t mac[6]) const
{
    for (size_t i = 0; i < kMaxPeers; ++i)
    {
        if (peers_[i].inUse && memcmp(peers_[i].mac, mac, 6) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

int EspNowBus::ensurePeer(const uint8_t mac[6])
{
    int idx = findPeerIndex(mac);
    if (idx >= 0)
        return idx;
    for (size_t i = 0; i < kMaxPeers; ++i)
    {
        if (!peers_[i].inUse)
        {
            peers_[i].inUse = true;
            memcpy(peers_[i].mac, mac, 6);
            peers_[i].lastMsgId = 0;
            peers_[i].lastBroadcastBase = 0;
            peers_[i].bcastWindow = 0;
            peers_[i].lastSeenMs = millis();
            peers_[i].heartbeatStage = 0;
            peers_[i].nonceValid = false;
            if (config_.useEncryption)
            {
                esp_now_peer_info_t info = makePeerInfo(mac, true, derived_.lmk);
                if (esp_now_add_peer(&info) == ESP_OK)
                {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
                    applyPeerRate(mac);
#endif
                }
            }
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint8_t *EspNowBus::bufferPtr(uint16_t idx)
{
    if (!payloadPool_ || idx >= poolCount_)
        return nullptr;
    return payloadPool_ + (static_cast<size_t>(idx) * config_.maxPayloadBytes);
}

int16_t EspNowBus::allocBuffer()
{
    if (!bufferUsed_)
        return -1;
    for (size_t i = 0; i < poolCount_; ++i)
    {
        if (!bufferUsed_[i])
        {
            bufferUsed_[i] = true;
            return static_cast<int16_t>(i);
        }
    }
    return -1;
}

void EspNowBus::freeBuffer(uint16_t idx)
{
    if (!bufferUsed_ || idx >= poolCount_)
        return;
    bufferUsed_[idx] = false;
}

bool EspNowBus::enqueueCommon(Dest dest, PacketType pktType, const uint8_t *mac, const void *data, size_t len, uint32_t timeoutMs)
{
    // enforce payload size bounds by IDF version and header overhead
    uint16_t maxLen = config_.maxPayloadBytes;
#ifdef ESP_NOW_MAX_DATA_LEN_V2
    if (maxLen > ESP_NOW_MAX_DATA_LEN_V2)
        maxLen = ESP_NOW_MAX_DATA_LEN_V2;
#else
    if (maxLen > ESP_NOW_MAX_DATA_LEN)
        maxLen = ESP_NOW_MAX_DATA_LEN;
#endif
    if (maxLen < kHeaderSize + 4)
        maxLen = kHeaderSize + 4;

    if (xPortInIsrContext())
    {
        ESP_LOGE(TAG, "send called from ISR not supported");
        return false;
    }
    if (!sendQueue_)
        return false;
    const bool needsAuth = (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck || pktType == PacketType::ControlAppAck || pktType == PacketType::ControlHeartbeat || pktType == PacketType::ControlLeave);
    const size_t totalLen = kHeaderSize + (needsAuth ? (4 + kAuthTagLen) : 0) + len;
    if (totalLen > maxLen)
    {
        if (onSendResult_)
            onSendResult_(mac, SendStatus::TooLarge);
        ESP_LOGW(TAG, "payload too large (%u > %u)", static_cast<unsigned>(totalLen), maxLen);
        return false;
    }
    int16_t bufIdx = allocBuffer();
    if (bufIdx < 0)
    {
        if (onSendResult_)
            onSendResult_(mac, SendStatus::DroppedFull);
        ESP_LOGW(TAG, "queue full: drop");
        return false;
    }

    uint16_t msgId = 0;
    uint16_t seq = 0;
    if (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck)
    {
        seq = ++broadcastSeq_;
    }
    else
    {
        msgId = ++msgCounter_;
    }

    uint8_t *buf = bufferPtr(static_cast<uint16_t>(bufIdx));
    buf[0] = kMagic;
    buf[1] = kVersion;
    buf[2] = pktType;
    buf[3] = 0; // flags
    uint16_t idField = (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck) ? seq : msgId;
    buf[4] = static_cast<uint8_t>(idField & 0xFF);
    buf[5] = static_cast<uint8_t>((idField >> 8) & 0xFF);

    size_t cursor = kHeaderSize;
    if (needsAuth)
    {
        // groupId (4 bytes, LE)
        buf[cursor + 0] = static_cast<uint8_t>(derived_.groupId & 0xFF);
        buf[cursor + 1] = static_cast<uint8_t>((derived_.groupId >> 8) & 0xFF);
        buf[cursor + 2] = static_cast<uint8_t>((derived_.groupId >> 16) & 0xFF);
        buf[cursor + 3] = static_cast<uint8_t>((derived_.groupId >> 24) & 0xFF);
        cursor += 4;
    }

    memcpy(buf + cursor, data, len);
    cursor += len;

    if (needsAuth)
    {
        const bool useAuthKey = (pktType == PacketType::ControlJoinReq ||
                                 pktType == PacketType::ControlJoinAck ||
                                 pktType == PacketType::ControlAppAck ||
                                 pktType == PacketType::ControlHeartbeat ||
                                 pktType == PacketType::ControlLeave);
        const uint8_t *key = useAuthKey ? derived_.keyAuth : derived_.keyBcast;
        computeAuthTag(buf + cursor, buf, cursor, key);
        cursor += kAuthTagLen;
    }

    TxItem item{};
    item.bufferIndex = static_cast<uint16_t>(bufIdx);
    item.len = static_cast<uint16_t>(cursor);
    item.msgId = msgId;
    item.seq = seq;
    item.dest = dest;
    item.pktType = pktType;
    item.isRetry = false;
    memcpy(item.mac, mac, 6);
    if (config_.enableAppAck && pktType == PacketType::DataUnicast)
    {
        item.expectAck = true;
        item.appAckDeadlineMs = millis() + config_.txTimeoutMs;
    }
    else if (pktType == PacketType::ControlAppAck)
    {
        item.expectAck = false;
    }

    TickType_t ticks;
    if (timeoutMs == kUseDefault)
    {
        ticks = pdMS_TO_TICKS(config_.sendTimeoutMs);
    }
    else if (timeoutMs == portMAX_DELAY)
    {
        ticks = portMAX_DELAY;
    }
    else
    {
        ticks = pdMS_TO_TICKS(timeoutMs);
    }
    BaseType_t ok = xQueueSend(sendQueue_, &item, ticks);
    if (ok != pdPASS)
    {
        freeBuffer(item.bufferIndex);
        if (onSendResult_)
            onSendResult_(mac, SendStatus::DroppedFull);
        return false;
    }
    if (onSendResult_)
        onSendResult_(mac, SendStatus::Queued);
    ESP_LOGV(TAG, "enqueue pkt=%u dest=%u mac=%02X:%02X:%02X:%02X:%02X:%02X len=%u total=%u",
             static_cast<unsigned>(pktType), static_cast<unsigned>(dest),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             static_cast<unsigned>(len),
             static_cast<unsigned>(cursor));
    return true;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void EspNowBus::onSendStatic(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
#else
void EspNowBus::onSendStatic(const uint8_t *mac, esp_now_send_status_t status)
{
#endif
    if (!instance_ || !instance_->sendTask_)
        return;
    uint32_t val = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 2;
    BaseType_t hpw = pdFALSE;
    xTaskNotifyFromISR(instance_->sendTask_, val, eSetValueWithOverwrite, &hpw);
    if (hpw == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void EspNowBus::onReceiveStatic(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    const uint8_t *mac = info ? info->src_addr : nullptr;
#else
void EspNowBus::onReceiveStatic(const uint8_t *mac, const uint8_t *data, int len)
{
#endif
    if (!instance_ || len < static_cast<int>(kHeaderSize))
        return;
    const uint8_t *p = data;
    if (p[0] != kMagic || p[1] != kVersion)
        return;
    uint8_t type = p[2];
    bool isRetry = (p[3] & 0x01) != 0;
    uint16_t id = static_cast<uint16_t>(p[4]) | (static_cast<uint16_t>(p[5]) << 8);

    ESP_LOGV(TAG, "rx pkt type=%u len=%d id=%u retry=%d mac=%02X:%02X:%02X:%02X:%02X:%02X",
             static_cast<unsigned>(type), len, static_cast<unsigned>(id), isRetry,
             mac ? mac[0] : 0, mac ? mac[1] : 0, mac ? mac[2] : 0,
             mac ? mac[3] : 0, mac ? mac[4] : 0, mac ? mac[5] : 0);

    const bool needsAuth = (type == PacketType::DataBroadcast ||
                            type == PacketType::ControlJoinReq ||
                            type == PacketType::ControlJoinAck ||
                            type == PacketType::ControlAppAck ||
                            type == PacketType::ControlHeartbeat ||
                            type == PacketType::ControlLeave);
    if (needsAuth)
    {
        if (!instance_->verifyAuthTag(data, len, type))
        {
            ESP_LOGW(TAG, "auth fail or group mismatch type=%u mac=%02X:%02X:%02X:%02X:%02X:%02X",
                     static_cast<unsigned>(type),
                     mac ? mac[0] : 0, mac ? mac[1] : 0, mac ? mac[2] : 0,
                     mac ? mac[3] : 0, mac ? mac[4] : 0, mac ? mac[5] : 0);
            return; // auth failed or groupId mismatch
        }
    }

    size_t cursor = kHeaderSize;
    if (needsAuth)
    {
        cursor += 4; // groupId already checked
    }
    const uint8_t *payload = p + cursor;
    int payloadLen = len - static_cast<int>(cursor + (needsAuth ? kAuthTagLen : 0));

    int idx = (type == PacketType::ControlLeave) ? instance_->findPeerIndex(mac) : instance_->ensurePeer(mac);
    if (type == PacketType::DataUnicast)
    {
        if (idx >= 0)
        {
            instance_->peers_[idx].lastSeenMs = millis();
            instance_->peers_[idx].heartbeatStage = 0;
        }
        bool duplicate = (idx >= 0 && instance_->peers_[idx].lastMsgId == id);
        if (idx >= 0 && !duplicate)
            instance_->peers_[idx].lastMsgId = id;
        // Auto app-level ACK
        if (instance_->config_.enableAppAck)
        {
            AppAckPayload ack{};
            ack.msgId = id;
            instance_->enqueueCommon(Dest::Unicast, PacketType::ControlAppAck, mac, &ack, sizeof(ack), kUseDefault);
            if (instance_->onAppAck_)
            {
                instance_->onAppAck_(mac, id);
            }
        }
        if (duplicate)
        {
            ESP_LOGD(TAG, "rx unicast duplicate msgId=%u mac=%02X:%02X:%02X:%02X:%02X:%02X",
                     static_cast<unsigned>(id),
                     mac ? mac[0] : 0, mac ? mac[1] : 0, mac ? mac[2] : 0,
                     mac ? mac[3] : 0, mac ? mac[4] : 0, mac ? mac[5] : 0);
            return; // duplicate payload is dropped
        }
    }
    else if (type == PacketType::DataBroadcast)
    {
        if (idx >= 0)
        {
            instance_->peers_[idx].lastSeenMs = millis();
            instance_->peers_[idx].heartbeatStage = 0;
        }
        if (!instance_->acceptBroadcastSeq(mac, id))
        {
            ESP_LOGD(TAG, "rx bcast replay drop seq=%u mac=%02X:%02X:%02X:%02X:%02X:%02X",
                     static_cast<unsigned>(id),
                     mac ? mac[0] : 0, mac ? mac[1] : 0, mac ? mac[2] : 0,
                     mac ? mac[3] : 0, mac ? mac[4] : 0, mac ? mac[5] : 0);
            return;
        }
    }
    else if (type == PacketType::ControlJoinReq)
    {
        if (payloadLen < static_cast<int>(sizeof(JoinReqPayload)))
        {
            ESP_LOGW(TAG, "join req too short");
            return;
        }
        const JoinReqPayload *req = reinterpret_cast<const JoinReqPayload *>(payload);
        // target check
        if (memcmp(req->targetMac, kBroadcastMac, 6) != 0 && memcmp(req->targetMac, instance_->selfMac_, 6) != 0)
        {
            return; // not for us
        }
        size_t peers = instance_->peerCount();
        ESP_LOGD(TAG, "join req received from %02X:%02X:%02X:%02X:%02X:%02X peers=%u",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                 static_cast<unsigned>(peers));
        // store prev token if matches
        if (idx >= 0 && memcmp(req->prevToken, instance_->peers_[idx].lastNonceB, kNonceLen) == 0)
        {
            // resume path; nothing special
        }
        if (idx < 0)
            idx = instance_->ensurePeer(mac);
        JoinAckPayload ackPayload{};
        memcpy(ackPayload.nonceA, req->nonceA, kNonceLen); // echo nonceA
        esp_fill_random(ackPayload.nonceB, kNonceLen);     // nonceB
        memcpy(ackPayload.targetMac, mac, 6);
        if (idx >= 0)
        {
            memcpy(instance_->peers_[idx].lastNonceB, ackPayload.nonceB, kNonceLen);
            instance_->peers_[idx].nonceValid = true;
        }
        instance_->enqueueCommon(Dest::Broadcast, PacketType::ControlJoinAck, kBroadcastMac, &ackPayload, sizeof(ackPayload), kUseDefault);
        if (instance_->onJoinEvent_)
            instance_->onJoinEvent_(mac, true, false);
        return;
    }
    else if (type == PacketType::ControlJoinAck)
    {
        if (!instance_->pendingJoin_)
        {
            ESP_LOGW(TAG, "unsolicited join ack ignored");
            return;
        }
        if (payloadLen < static_cast<int>(sizeof(JoinAckPayload)))
        {
            ESP_LOGW(TAG, "join ack too short");
            return;
        }
        const JoinAckPayload *ack = reinterpret_cast<const JoinAckPayload *>(payload);
        if (memcmp(ack->targetMac, instance_->selfMac_, 6) != 0)
            return; // not for us
        if (memcmp(ack->nonceA, instance_->pendingNonceA_, kNonceLen) != 0)
        {
            ESP_LOGW(TAG, "join ack nonce mismatch");
            if (instance_->onJoinEvent_)
                instance_->onJoinEvent_(mac, false, true);
            return;
        }
        if (idx < 0)
        {
            idx = instance_->ensurePeer(mac);
        }
        if (idx >= 0)
        {
            memcpy(instance_->peers_[idx].lastNonceB, ack->nonceB, kNonceLen);
            instance_->peers_[idx].nonceValid = true;
            instance_->peers_[idx].lastSeenMs = millis();
            instance_->peers_[idx].heartbeatStage = 0;
        }
        memcpy(instance_->storedNonceB_, ack->nonceB, kNonceLen);
        instance_->storedNonceBValid_ = true;
        instance_->pendingJoin_ = false;
        ESP_LOGI(TAG, "join success, peer idx=%d", idx);
        if (instance_->onJoinEvent_)
            instance_->onJoinEvent_(mac, true, true);
        return;
    }
    else if (type == PacketType::ControlAppAck)
    {
        if (payloadLen < static_cast<int>(sizeof(AppAckPayload)))
            return;
        const AppAckPayload *ack = reinterpret_cast<const AppAckPayload *>(payload);
        if (idx >= 0 && !instance_->acceptAppAck(instance_->peers_[idx], ack->msgId))
        {
            ESP_LOGW(TAG, "app-ack replay drop msgId=%u mac=%02X:%02X:%02X:%02X:%02X:%02X",
                     ack->msgId,
                     mac ? mac[0] : 0, mac ? mac[1] : 0, mac ? mac[2] : 0,
                     mac ? mac[3] : 0, mac ? mac[4] : 0, mac ? mac[5] : 0);
            return;
        }
        if (idx >= 0)
        {
            instance_->peers_[idx].lastSeenMs = millis();
            instance_->peers_[idx].heartbeatStage = 0;
        }
        if (instance_->txInFlight_ && instance_->currentTx_.expectAck && ack->msgId == instance_->currentTx_.msgId)
        {
            if (instance_->onSendResult_)
                instance_->onSendResult_(mac, SendStatus::AppAckReceived);
            instance_->recordSendSuccess(mac);
            instance_->freeBuffer(instance_->currentTx_.bufferIndex);
            instance_->txInFlight_ = false;
            instance_->retryCount_ = 0;
        }
        else if (!instance_->txInFlight_)
        {
            ESP_LOGW(TAG, "app-ack late or no in-flight msgId=%u", ack->msgId);
        }
        if (instance_->onAppAck_)
        {
            instance_->onAppAck_(mac, ack->msgId);
        }
        return;
    }
    else if (type == PacketType::ControlHeartbeat)
    {
        if (payloadLen < static_cast<int>(sizeof(HeartbeatPayload)))
            return;
        const HeartbeatPayload *hb = reinterpret_cast<const HeartbeatPayload *>(payload);
        if (idx >= 0)
        {
            instance_->peers_[idx].lastSeenMs = millis();
            instance_->peers_[idx].heartbeatStage = 0;
        }
        if (hb->kind == 0)
        {
            // Ping -> respond Pong
            HeartbeatPayload pong{1};
            instance_->enqueueCommon(Dest::Unicast, PacketType::ControlHeartbeat, mac, &pong, sizeof(pong), kUseDefault);
        }
        return;
    }
    else if (type == PacketType::ControlLeave)
    {
        if (!mac)
            return;
        if (payloadLen < static_cast<int>(sizeof(LeavePayload)))
        {
            ESP_LOGW(TAG, "leave req too short");
            return;
        }
        const LeavePayload *lv = reinterpret_cast<const LeavePayload *>(payload);
        if (memcmp(lv->mac, mac, 6) != 0)
        {
            ESP_LOGW(TAG, "leave mac mismatch sender=%02X:%02X:%02X:%02X:%02X:%02X payload=%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                     lv->mac[0], lv->mac[1], lv->mac[2], lv->mac[3], lv->mac[4], lv->mac[5]);
            return;
        }
        if (idx < 0)
        {
            ESP_LOGW(TAG, "leave from unknown peer mac=%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return;
        }
        instance_->peers_[idx].lastSeenMs = millis();
        instance_->peers_[idx].heartbeatStage = 0;
        if (instance_->onJoinEvent_)
            instance_->onJoinEvent_(mac, false, false);
        instance_->removePeer(mac);
        return;
    }
    else
    {
        ESP_LOGW(TAG, "unknown packet type=%u mac=%02X:%02X:%02X:%02X:%02X:%02X",
                 static_cast<unsigned>(type),
                 mac ? mac[0] : 0, mac ? mac[1] : 0, mac ? mac[2] : 0,
                 mac ? mac[3] : 0, mac ? mac[4] : 0, mac ? mac[5] : 0);
        return;
    }
    if (instance_->onReceive_)
    {
        bool isBroadcast = (type == PacketType::DataBroadcast);
        instance_->onReceive_(mac, payload, static_cast<size_t>(payloadLen), isRetry, isBroadcast);
    }
}

void EspNowBus::sendTaskTrampoline(void *arg)
{
    auto *self = static_cast<EspNowBus *>(arg);
    self->selfTaskHandle_ = xTaskGetCurrentTaskHandle();
    self->sendTaskLoop();
}

bool EspNowBus::startSend(const TxItem &item)
{
    uint8_t *buf = bufferPtr(item.bufferIndex);
    if (!buf)
        return false;
    // update header flags/msgId for retry
    if (item.isRetry)
    {
        buf[3] |= 0x01; // isRetry flag
    }
    // Recompute auth tag if needed (flags change alters HMAC input)
    if (item.pktType == PacketType::DataBroadcast ||
        item.pktType == PacketType::ControlJoinReq ||
        item.pktType == PacketType::ControlJoinAck ||
        item.pktType == PacketType::ControlAppAck ||
        item.pktType == PacketType::ControlHeartbeat ||
        item.pktType == PacketType::ControlLeave)
    {
        const uint8_t *key = (item.pktType == PacketType::ControlJoinReq ||
                              item.pktType == PacketType::ControlJoinAck ||
                              item.pktType == PacketType::ControlAppAck ||
                              item.pktType == PacketType::ControlHeartbeat ||
                              item.pktType == PacketType::ControlLeave)
                                 ? derived_.keyAuth
                                 : derived_.keyBcast;
        if (item.len >= kHeaderSize + 4 + kAuthTagLen)
        {
            size_t tagOffset = static_cast<size_t>(item.len) - kAuthTagLen;
            computeAuthTag(buf + tagOffset, buf, tagOffset, key);
        }
    }
    const uint8_t *targetMac = item.mac;
    esp_err_t err = esp_now_send(targetMac, buf, item.len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_now_send failed err=%d mac=%02X:%02X:%02X:%02X:%02X:%02X len=%u",
                 static_cast<int>(err),
                 targetMac[0], targetMac[1], targetMac[2], targetMac[3], targetMac[4], targetMac[5],
                 static_cast<unsigned>(item.len));
        return false;
    }
    return true;
}

void EspNowBus::handleSendComplete(bool ok, bool timedOut)
{
    if (!txInFlight_)
        return;
    auto entry = currentTx_;
    if (ok)
    {
        if (entry.expectAck)
        {
            // Physical success; wait for app-ack to finalize
            txDeadlineMs_ = millis() + config_.txTimeoutMs;
            return;
        }
        if (onSendResult_)
            onSendResult_(entry.mac, SendStatus::SentOk);
        recordSendSuccess(entry.mac);
        freeBuffer(entry.bufferIndex);
        txInFlight_ = false;
        retryCount_ = 0;
    }
    else
    {
        if (retryCount_ < config_.maxRetries)
        {
            retryCount_++;
            currentTx_.isRetry = true;
            if (config_.retryDelayMs > 0)
            {
                vTaskDelay(pdMS_TO_TICKS(config_.retryDelayMs));
            }
            startSend(currentTx_);
            txDeadlineMs_ = millis() + config_.txTimeoutMs;
            if (onSendResult_)
                onSendResult_(entry.mac, SendStatus::Retrying);
            return;
        }
        if (onSendResult_)
            onSendResult_(entry.mac, timedOut ? SendStatus::Timeout : SendStatus::SendFailed);
        if (timedOut)
        {
            ESP_LOGW(TAG, "send timeout mac=%02X:%02X:%02X:%02X:%02X:%02X", entry.mac[0], entry.mac[1], entry.mac[2], entry.mac[3], entry.mac[4], entry.mac[5]);
        }
        else
        {
            ESP_LOGE(TAG, "send failed mac=%02X:%02X:%02X:%02X:%02X:%02X", entry.mac[0], entry.mac[1], entry.mac[2], entry.mac[3], entry.mac[4], entry.mac[5]);
        }
        recordSendFailure(entry.mac);
        freeBuffer(entry.bufferIndex);
        txInFlight_ = false;
        retryCount_ = 0;
    }
}

bool EspNowBus::sendNextIfIdle(TickType_t waitTicks)
{
    if (txInFlight_)
        return true;
    TxItem item{};
    if (xQueueReceive(sendQueue_, &item, waitTicks) == pdTRUE)
    {
        currentTx_ = item;
        retryCount_ = 0;
        txInFlight_ = startSend(item);
        txDeadlineMs_ = millis() + config_.txTimeoutMs;
        if (!txInFlight_)
        {
            freeBuffer(item.bufferIndex);
            if (onSendResult_)
                onSendResult_(item.mac, SendStatus::SendFailed);
            ESP_LOGE(TAG, "startSend failed mac=%02X:%02X:%02X:%02X:%02X:%02X",
                     item.mac[0], item.mac[1], item.mac[2], item.mac[3], item.mac[4], item.mac[5]);
        }
        return true;
    }
    return false;
}

void EspNowBus::sendTaskLoop()
{
    while (true)
    {
        uint32_t nowMs = millis();
        reseedCounters(nowMs);
        // Auto JOIN scheduler
        if (config_.autoJoinIntervalMs > 0 && (nowMs - lastAutoJoinMs_) >= config_.autoJoinIntervalMs)
        {
            lastAutoJoinMs_ = nowMs;
            sendJoinRequest();
        }
        // Heartbeat / liveness maintenance
        for (size_t i = 0; i < kMaxPeers; ++i)
        {
            auto &p = peers_[i];
            if (!p.inUse)
                continue;
            if (p.lastSeenMs == 0)
                p.lastSeenMs = nowMs;
            uint32_t elapsed = nowMs - p.lastSeenMs;
            uint32_t hb = config_.heartbeatIntervalMs;
            if (hb == 0)
                continue;
            if (elapsed >= hb * 3)
            {
                ESP_LOGW(TAG, "peer timeout drop mac=%02X:%02X:%02X:%02X:%02X:%02X",
                         p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5]);
                if (onJoinEvent_)
                    onJoinEvent_(p.mac, false, false); // treat as leave/timeout
                removePeer(p.mac);
                p.inUse = false;
                continue;
            }
            if (elapsed >= hb * 2)
            {
                if (p.heartbeatStage < 2)
                {
                    sendJoinRequest(p.mac);
                    p.heartbeatStage = 2;
                }
                continue;
            }
            if (elapsed >= hb)
            {
                if (p.heartbeatStage < 1)
                {
                    HeartbeatPayload ping{0};
                    enqueueCommon(Dest::Unicast, PacketType::ControlHeartbeat, p.mac, &ping, sizeof(ping), kUseDefault);
                    p.heartbeatStage = 1;
                }
            }
        }
        if (!txInFlight_)
        {
            sendNextIfIdle(pdMS_TO_TICKS(100));
            continue;
        }
        uint32_t deadline = txDeadlineMs_;
        TickType_t waitTicks = (deadline > nowMs) ? pdMS_TO_TICKS(deadline - nowMs) : 0;
        uint32_t notifyVal = 0;
        BaseType_t notified = xTaskNotifyWait(0, 0xFFFFFFFF, &notifyVal, waitTicks);
        if (notified == pdTRUE)
        {
            bool ok = notifyVal == 1;
            handleSendComplete(ok, false);
            continue;
        }
        // timeout
        if (millis() >= txDeadlineMs_)
        {
            handleSendComplete(false, true);
        }

        // app-ack timeout check
        if (txInFlight_ && currentTx_.expectAck)
        {
            uint32_t now2 = millis();
            if (now2 >= currentTx_.appAckDeadlineMs)
            {
                if (retryCount_ < config_.maxRetries)
                {
                    retryCount_++;
                    currentTx_.isRetry = true;
                    startSend(currentTx_);
                    currentTx_.appAckDeadlineMs = millis() + config_.txTimeoutMs;
                    if (onSendResult_)
                        onSendResult_(currentTx_.mac, SendStatus::Retrying);
                }
                else
                {
                    if (onSendResult_)
                        onSendResult_(currentTx_.mac, SendStatus::AppAckTimeout);
                    ESP_LOGW(TAG, "app-ack timeout mac=%02X:%02X:%02X:%02X:%02X:%02X", currentTx_.mac[0], currentTx_.mac[1], currentTx_.mac[2], currentTx_.mac[3], currentTx_.mac[4], currentTx_.mac[5]);
                    recordSendFailure(currentTx_.mac);
                    freeBuffer(currentTx_.bufferIndex);
                    txInFlight_ = false;
                    retryCount_ = 0;
                }
            }
        }
    }
}

bool EspNowBus::deriveKeys(const char *groupName)
{
    uint8_t secret[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char *>(groupName), strlen(groupName));
    mbedtls_sha256_finish(&ctx, secret);
    mbedtls_sha256_free(&ctx);

    auto derive = [&](const char *label, uint8_t *out, size_t outLen)
    {
        uint8_t digest[32];
        mbedtls_sha256_context c;
        mbedtls_sha256_init(&c);
        mbedtls_sha256_starts(&c, 0);
        mbedtls_sha256_update(&c, reinterpret_cast<const unsigned char *>(label), strlen(label));
        mbedtls_sha256_update(&c, secret, sizeof(secret));
        mbedtls_sha256_finish(&c, digest);
        memcpy(out, digest, outLen);
        mbedtls_sha256_free(&c);
    };

    derive("pmk", derived_.pmk, sizeof(derived_.pmk));
    derive("lmk", derived_.lmk, sizeof(derived_.lmk));
    derive("auth", derived_.keyAuth, sizeof(derived_.keyAuth));
    derive("bcast", derived_.keyBcast, sizeof(derived_.keyBcast));
    uint8_t gid[4];
    derive("gid", gid, sizeof(gid));
    derived_.groupId = static_cast<uint32_t>(gid[0]) |
                       (static_cast<uint32_t>(gid[1]) << 8) |
                       (static_cast<uint32_t>(gid[2]) << 16) |
                       (static_cast<uint32_t>(gid[3]) << 24);
    return true;
}

void EspNowBus::computeAuthTag(uint8_t *out, const uint8_t *msg, size_t len, const uint8_t *key)
{
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, info, 1) != 0)
    {
        mbedtls_md_free(&ctx);
        memset(out, 0, kAuthTagLen);
        return;
    }
    mbedtls_md_hmac_starts(&ctx, key, kAuthTagLen);
    mbedtls_md_hmac_update(&ctx, msg, len);
    uint8_t full[32];
    mbedtls_md_hmac_finish(&ctx, full);
    memcpy(out, full, kAuthTagLen);
    mbedtls_md_free(&ctx);
}

bool EspNowBus::verifyAuthTag(const uint8_t *msg, size_t len, uint8_t pktType)
{
    if (len < kHeaderSize + 4 + kAuthTagLen)
        return false;
    const uint8_t *groupPtr = msg + kHeaderSize;
    uint32_t gid = static_cast<uint32_t>(groupPtr[0]) |
                   (static_cast<uint32_t>(groupPtr[1]) << 8) |
                   (static_cast<uint32_t>(groupPtr[2]) << 16) |
                   (static_cast<uint32_t>(groupPtr[3]) << 24);
    if (gid != derived_.groupId)
        return false;
    const uint8_t *key = (pktType == PacketType::ControlJoinReq ||
                          pktType == PacketType::ControlJoinAck ||
                          pktType == PacketType::ControlAppAck ||
                          pktType == PacketType::ControlHeartbeat ||
                          pktType == PacketType::ControlLeave)
                             ? derived_.keyAuth
                             : derived_.keyBcast;
    size_t tagOffset = len - kAuthTagLen;
    uint8_t calc[kAuthTagLen];
    computeAuthTag(calc, msg, tagOffset, key);
    return memcmp(calc, msg + tagOffset, kAuthTagLen) == 0;
}

void EspNowBus::reseedCounters(uint32_t now)
{
    if (now - lastReseedMs_ < kReseedIntervalMs)
        return;
    lastReseedMs_ = now;
    esp_fill_random(&msgCounter_, sizeof(msgCounter_));
    esp_fill_random(&broadcastSeq_, sizeof(broadcastSeq_));
    ESP_LOGI(TAG, "reseed counters");
}

void EspNowBus::recordSendFailure(const uint8_t mac[6])
{
    (void)mac;
}

void EspNowBus::recordSendSuccess(const uint8_t mac[6])
{
    (void)mac;
}

int EspNowBus::findSenderIndex(const uint8_t mac[6]) const
{
    if (!mac)
        return -1;
    for (size_t i = 0; i < kMaxSenders; ++i)
    {
        if (senders_[i].inUse && memcmp(senders_[i].mac, mac, 6) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

int EspNowBus::ensureSender(const uint8_t mac[6])
{
    int idx = findSenderIndex(mac);
    if (idx >= 0)
    {
        senders_[idx].lastUsedMs = millis();
        return idx;
    }
    // find free
    uint32_t now = millis();
    int freeIdx = -1;
    for (size_t i = 0; i < kMaxSenders; ++i)
    {
        if (!senders_[i].inUse)
        {
            freeIdx = static_cast<int>(i);
            break;
        }
    }
    if (freeIdx < 0)
    {
        // evict oldest
        uint32_t oldest = UINT32_MAX;
        int oldestIdx = 0;
        for (size_t i = 0; i < kMaxSenders; ++i)
        {
            if (senders_[i].lastUsedMs < oldest)
            {
                oldest = senders_[i].lastUsedMs;
                oldestIdx = static_cast<int>(i);
            }
        }
        freeIdx = oldestIdx;
    }
    auto &s = senders_[freeIdx];
    memcpy(s.mac, mac, 6);
    s.inUse = true;
    s.base = 0;
    s.window = 0;
    s.lastUsedMs = now;
    return freeIdx;
}

bool EspNowBus::acceptBroadcastSeq(const uint8_t mac[6], uint16_t seq)
{
    if (config_.replayWindowBcast == 0)
        return true;
    int idx = ensureSender(mac);
    if (idx < 0)
        return true;
    auto &s = senders_[idx];
    s.lastUsedMs = millis();
    uint16_t window = config_.replayWindowBcast;
    uint16_t base = s.base;
    uint16_t dist = static_cast<uint16_t>(seq - base);
    if (dist == 0)
        return false;
    if (dist <= window)
    {
        uint32_t bit = 1UL << (dist - 1);
        if (s.window & bit)
            return false;
        s.window |= bit;
        return true;
    }
    uint16_t shift = dist - 1;
    if (shift >= 32)
    {
        s.window = 0;
    }
    else
    {
        s.window <<= shift;
    }
    s.window |= 1UL;
    s.base = seq;
    return true;
}

bool EspNowBus::acceptAppAck(PeerInfo &peer, uint16_t msgId)
{
    // Simple check: accept if not equal to last seen; update last
    if (peer.lastAppAckId == msgId)
    {
        return false;
    }
    peer.lastAppAckId = msgId;
    return true;
}

bool EspNowBus::applyPeerRate(const uint8_t mac[6])
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    if (!mac)
        return false;
    esp_now_rate_config_t rateCfg{};
    rateCfg.rate = config_.phyRate;
    rateCfg.ersu = false;
    rateCfg.dcm = false;
    if (config_.phyRate < WIFI_PHY_RATE_48M)
    {
        rateCfg.phymode = WIFI_PHY_MODE_11B;
    }
    else if (config_.phyRate < WIFI_PHY_RATE_MCS0_LGI)
    {
        rateCfg.phymode = WIFI_PHY_MODE_11G;
    }
    else if (config_.phyRate < WIFI_PHY_RATE_LORA_250K)
    {
        rateCfg.phymode = WIFI_PHY_MODE_HT20;
    }
    else
    {
        // default to 1M if unsupported
        ESP_LOGW(TAG, "unsupported phyRate=%d, defaulting to 1M", static_cast<int>(config_.phyRate));
        rateCfg.rate = WIFI_PHY_RATE_1M_L;
        rateCfg.phymode = WIFI_PHY_MODE_11B;
    }

    esp_err_t err = esp_now_set_peer_rate_config(mac, &rateCfg);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "set peer rate failed rate=%d err=%d", static_cast<int>(config_.phyRate), static_cast<int>(err));
        return false;
    }
    return true;
#else
    (void)mac;
    return false;
#endif
}
