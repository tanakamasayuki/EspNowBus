#include "EspNowBus.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_heap_caps.h>
#include <string.h>

EspNowBus* EspNowBus::instance_ = nullptr;

namespace {
constexpr esp_now_peer_info_t makePeerInfo(const uint8_t mac[6], bool encrypt) {
    esp_now_peer_info_t info{};
    memcpy(info.peer_addr, mac, 6);
    info.ifidx = WIFI_IF_STA;
    info.channel = 0;
    info.encrypt = encrypt ? 1 : 0; // NOTE: key 未設定のため encrypt=false で使用する想定
    return info;
}
} // namespace

bool EspNowBus::begin(const Config& cfg) {
    if (!cfg.groupName || cfg.maxQueueLength == 0 || cfg.maxPayloadBytes == 0) {
        return false;
    }
    // ensure single instance
    instance_ = this;
    config_ = cfg;

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        return false;
    }
    esp_now_register_send_cb(&EspNowBus::onSendStatic);
    esp_now_register_recv_cb(&EspNowBus::onReceiveStatic);

    // Ensure broadcast peer exists
    const uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_now_peer_info_t bcastPeer = makePeerInfo(broadcastMac, false);
    esp_now_add_peer(&bcastPeer);

    // Allocate payload pool
    poolCount_ = config_.maxQueueLength;
    payloadPool_ = static_cast<uint8_t*>(heap_caps_malloc(config_.maxPayloadBytes * poolCount_, MALLOC_CAP_DEFAULT));
    bufferUsed_ = static_cast<bool*>(heap_caps_malloc(poolCount_ * sizeof(bool), MALLOC_CAP_DEFAULT));
    if (!payloadPool_ || !bufferUsed_) {
        end();
        return false;
    }
    memset(bufferUsed_, 0, poolCount_);

    sendQueue_ = xQueueCreate(config_.maxQueueLength, sizeof(TxItem));
    if (!sendQueue_) {
        end();
        return false;
    }

    BaseType_t created = pdFAIL;
    if (config_.taskCore < 0) {
        created = xTaskCreate(&EspNowBus::sendTaskTrampoline, "EspNowBusSend", config_.taskStackSize, this,
                              config_.taskPriority, &sendTask_);
    } else {
        created = xTaskCreatePinnedToCore(&EspNowBus::sendTaskTrampoline, "EspNowBusSend", config_.taskStackSize, this,
                                          config_.taskPriority, &sendTask_, config_.taskCore);
    }
    if (created != pdPASS) {
        end();
        return false;
    }
    return true;
}

bool EspNowBus::begin(const char* groupName,
                      bool canAcceptRegistrations,
                      bool useEncryption,
                      uint16_t maxQueueLength) {
    Config cfg;
    cfg.groupName = groupName;
    cfg.canAcceptRegistrations = canAcceptRegistrations;
    cfg.useEncryption = useEncryption;
    cfg.maxQueueLength = maxQueueLength;
    return begin(cfg);
}

void EspNowBus::end() {
    if (sendTask_) {
        vTaskDelete(sendTask_);
        sendTask_ = nullptr;
    }
    if (sendQueue_) {
        vQueueDelete(sendQueue_);
        sendQueue_ = nullptr;
    }
    if (payloadPool_) {
        heap_caps_free(payloadPool_);
        payloadPool_ = nullptr;
    }
    if (bufferUsed_) {
        heap_caps_free(bufferUsed_);
        bufferUsed_ = nullptr;
    }
    instance_ = nullptr;
    esp_now_unregister_send_cb();
    esp_now_unregister_recv_cb();
    esp_now_deinit();
}

bool EspNowBus::sendTo(const uint8_t mac[6], const void* data, size_t len, uint32_t timeoutMs) {
    if (!mac) return false;
    return enqueueCommon(Dest::Unicast, PacketType::DataUnicast, mac, data, len, timeoutMs);
}

bool EspNowBus::sendToAllPeers(const void* data, size_t len, uint32_t timeoutMs) {
    bool ok = true;
    for (size_t i = 0; i < kMaxPeers; ++i) {
        if (!peers_[i].inUse) continue;
        if (!sendTo(peers_[i].mac, data, len, timeoutMs)) {
            ok = false;
        }
    }
    return ok;
}

bool EspNowBus::broadcast(const void* data, size_t len, uint32_t timeoutMs) {
    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    return enqueueCommon(Dest::Broadcast, PacketType::DataBroadcast, bcast, data, len, timeoutMs);
}

void EspNowBus::onReceive(ReceiveCallback cb) {
    onReceive_ = cb;
}

void EspNowBus::onSendResult(SendResultCallback cb) {
    onSendResult_ = cb;
}

bool EspNowBus::addPeer(const uint8_t mac[6]) {
    if (!mac) return false;
    int idx = findPeerIndex(mac);
    if (idx >= 0) return true;
    idx = ensurePeer(mac);
    if (idx < 0) return false;

    // NOTE: encryption未実装のため encrypt=false で登録
    esp_now_peer_info_t info = makePeerInfo(mac, false);
    esp_err_t err = esp_now_add_peer(&info);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        peers_[idx].inUse = false;
        return false;
    }
    return true;
}

bool EspNowBus::removePeer(const uint8_t mac[6]) {
    if (!mac) return false;
    esp_now_del_peer(mac);
    int idx = findPeerIndex(mac);
    if (idx >= 0) {
        peers_[idx].inUse = false;
    }
    return true;
}

bool EspNowBus::hasPeer(const uint8_t mac[6]) const {
    return findPeerIndex(mac) >= 0;
}

void EspNowBus::setAcceptRegistration(bool enable) {
    config_.canAcceptRegistrations = enable;
}

bool EspNowBus::sendRegistrationRequest() {
    static const uint8_t bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    const uint8_t dummy = 0;
    return enqueueCommon(Dest::Broadcast, PacketType::ControlJoinReq, bcast, &dummy, sizeof(dummy), kUseDefault);
}

bool EspNowBus::initPeers(const uint8_t peers[][6], size_t count) {
    bool ok = true;
    for (size_t i = 0; i < count; ++i) {
        if (!addPeer(peers[i])) {
            ok = false;
        }
    }
    return ok;
}

// --- internal helpers ---

int EspNowBus::findPeerIndex(const uint8_t mac[6]) const {
    for (size_t i = 0; i < kMaxPeers; ++i) {
        if (peers_[i].inUse && memcmp(peers_[i].mac, mac, 6) == 0) return static_cast<int>(i);
    }
    return -1;
}

int EspNowBus::ensurePeer(const uint8_t mac[6]) {
    int idx = findPeerIndex(mac);
    if (idx >= 0) return idx;
    for (size_t i = 0; i < kMaxPeers; ++i) {
        if (!peers_[i].inUse) {
            peers_[i].inUse = true;
            memcpy(peers_[i].mac, mac, 6);
            peers_[i].lastMsgId = 0;
            peers_[i].lastBroadcastSeq = 0;
            return static_cast<int>(i);
        }
    }
    return -1;
}

uint8_t* EspNowBus::bufferPtr(uint16_t idx) {
    if (!payloadPool_ || idx >= poolCount_) return nullptr;
    return payloadPool_ + (static_cast<size_t>(idx) * config_.maxPayloadBytes);
}

int16_t EspNowBus::allocBuffer() {
    if (!bufferUsed_) return -1;
    for (size_t i = 0; i < poolCount_; ++i) {
        if (!bufferUsed_[i]) {
            bufferUsed_[i] = true;
            return static_cast<int16_t>(i);
        }
    }
    return -1;
}

void EspNowBus::freeBuffer(uint16_t idx) {
    if (!bufferUsed_ || idx >= poolCount_) return;
    bufferUsed_[idx] = false;
}

bool EspNowBus::enqueueCommon(Dest dest, PacketType pktType, const uint8_t* mac, const void* data, size_t len, uint32_t timeoutMs) {
    if (!sendQueue_) return false;
    const size_t totalLen = kHeaderSize + len;
    if (totalLen > config_.maxPayloadBytes) {
        if (onSendResult_) onSendResult_(mac, SendStatus::TooLarge);
        return false;
    }
    int16_t bufIdx = allocBuffer();
    if (bufIdx < 0) {
        if (onSendResult_) onSendResult_(mac, SendStatus::DroppedFull);
        return false;
    }

    uint16_t msgId = 0;
    uint16_t seq = 0;
    if (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq) {
        seq = ++broadcastSeq_;
    } else {
        msgId = ++msgCounter_;
    }

    uint8_t* buf = bufferPtr(static_cast<uint16_t>(bufIdx));
    buf[0] = kMagic;
    buf[1] = kVersion;
    buf[2] = pktType;
    buf[3] = 0; // flags
    uint16_t idField = (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq) ? seq : msgId;
    buf[4] = static_cast<uint8_t>(idField & 0xFF);
    buf[5] = static_cast<uint8_t>((idField >> 8) & 0xFF);

    memcpy(buf + kHeaderSize, data, len);

    TxItem item{};
    item.bufferIndex = static_cast<uint16_t>(bufIdx);
    item.len = static_cast<uint16_t>(totalLen);
    item.msgId = msgId;
    item.seq = seq;
    item.dest = dest;
    item.pktType = pktType;
    item.isRetry = false;
    memcpy(item.mac, mac, 6);

    TickType_t ticks;
    if (timeoutMs == kUseDefault) {
        ticks = pdMS_TO_TICKS(config_.sendTimeoutMs);
    } else if (timeoutMs == portMAX_DELAY) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(timeoutMs);
    }
    BaseType_t ok = xQueueSend(sendQueue_, &item, ticks);
    if (ok != pdPASS) {
        freeBuffer(item.bufferIndex);
        if (onSendResult_) onSendResult_(mac, SendStatus::DroppedFull);
        return false;
    }
    if (onSendResult_) onSendResult_(mac, SendStatus::Queued);
    return true;
}

void EspNowBus::onSendStatic(const uint8_t* mac, esp_now_send_status_t status) {
    if (!instance_ || !instance_->sendTask_) return;
    uint32_t val = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 2;
    BaseType_t hpw = pdFALSE;
    xTaskNotifyFromISR(instance_->sendTask_, val, eSetValueWithOverwrite, &hpw);
    if (hpw == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void EspNowBus::onReceiveStatic(const uint8_t* mac, const uint8_t* data, int len) {
    if (!instance_ || len < static_cast<int>(kHeaderSize)) return;
    const uint8_t* p = data;
    if (p[0] != kMagic || p[1] != kVersion) return;
    uint8_t type = p[2];
    bool isRetry = (p[3] & 0x01) != 0;
    uint16_t id = static_cast<uint16_t>(p[4]) | (static_cast<uint16_t>(p[5]) << 8);
    const uint8_t* payload = p + kHeaderSize;
    int payloadLen = len - static_cast<int>(kHeaderSize);

    int idx = instance_->ensurePeer(mac);
    if (type == PacketType::DataUnicast) {
        if (idx >= 0 && instance_->peers_[idx].lastMsgId == id) {
            return; // duplicate
        }
        if (idx >= 0) instance_->peers_[idx].lastMsgId = id;
    } else if (type == PacketType::DataBroadcast) {
        if (idx >= 0 && instance_->peers_[idx].lastBroadcastSeq == id) {
            return;
        }
        if (idx >= 0) instance_->peers_[idx].lastBroadcastSeq = id;
    } else {
        // Control packets not yet handled
        return;
    }
    if (instance_->onReceive_) {
        instance_->onReceive_(mac, payload, static_cast<size_t>(payloadLen), isRetry);
    }
}

void EspNowBus::sendTaskTrampoline(void* arg) {
    auto* self = static_cast<EspNowBus*>(arg);
    self->selfTaskHandle_ = xTaskGetCurrentTaskHandle();
    self->sendTaskLoop();
}

bool EspNowBus::startSend(const TxItem& item) {
    uint8_t* buf = bufferPtr(item.bufferIndex);
    if (!buf) return false;
    // update header flags/msgId for retry
    if (item.isRetry) {
        buf[3] |= 0x01; // isRetry flag
    }
    const uint8_t* targetMac = item.mac;
    esp_err_t err = esp_now_send(targetMac, buf, item.len);
    return err == ESP_OK;
}

void EspNowBus::handleSendComplete(bool ok, bool timedOut) {
    if (!txInFlight_) return;
    auto entry = currentTx_;
    if (ok) {
        if (onSendResult_) onSendResult_(entry.mac, SendStatus::SentOk);
        freeBuffer(entry.bufferIndex);
        txInFlight_ = false;
        retryCount_ = 0;
    } else {
        if (retryCount_ < config_.maxRetries) {
            retryCount_++;
            currentTx_.isRetry = true;
            if (config_.retryDelayMs > 0) {
                vTaskDelay(pdMS_TO_TICKS(config_.retryDelayMs));
            }
            startSend(currentTx_);
            txDeadlineMs_ = millis() + config_.txTimeoutMs;
            if (onSendResult_) onSendResult_(entry.mac, SendStatus::Retrying);
            return;
        }
        if (onSendResult_) onSendResult_(entry.mac, timedOut ? SendStatus::Timeout : SendStatus::SendFailed);
        freeBuffer(entry.bufferIndex);
        txInFlight_ = false;
        retryCount_ = 0;
    }
}

bool EspNowBus::sendNextIfIdle(TickType_t waitTicks) {
    if (txInFlight_) return true;
    TxItem item{};
    if (xQueueReceive(sendQueue_, &item, waitTicks) == pdTRUE) {
        currentTx_ = item;
        retryCount_ = 0;
        txInFlight_ = startSend(item);
        txDeadlineMs_ = millis() + config_.txTimeoutMs;
        if (!txInFlight_) {
            freeBuffer(item.bufferIndex);
            if (onSendResult_) onSendResult_(item.mac, SendStatus::SendFailed);
        }
        return true;
    }
    return false;
}

void EspNowBus::sendTaskLoop() {
    while (true) {
        if (!txInFlight_) {
            sendNextIfIdle(portMAX_DELAY);
            continue;
        }
        uint32_t now = millis();
        uint32_t deadline = txDeadlineMs_;
        TickType_t waitTicks = (deadline > now) ? pdMS_TO_TICKS(deadline - now) : 0;
        uint32_t notifyVal = 0;
        BaseType_t notified = xTaskNotifyWait(0, 0xFFFFFFFF, &notifyVal, waitTicks);
        if (notified == pdTRUE) {
            bool ok = notifyVal == 1;
            handleSendComplete(ok, false);
            continue;
        }
        // timeout
        if (millis() >= txDeadlineMs_) {
            handleSendComplete(false, true);
        }
    }
}
