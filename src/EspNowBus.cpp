#include "EspNowBus.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include "esp_log.h"

static const char* TAG = "EspNowBus";

EspNowBus* EspNowBus::instance_ = nullptr;

namespace {
esp_now_peer_info_t makePeerInfo(const uint8_t mac[6], bool encrypt, const uint8_t* lmk) {
    esp_now_peer_info_t info{};
    memcpy(info.peer_addr, mac, 6);
    info.ifidx = WIFI_IF_STA;
    info.channel = 0;
    if (encrypt && lmk) {
        info.encrypt = 1;
        memcpy(info.lmk, lmk, 16);
    } else {
        info.encrypt = 0;
    }
    return info;
}
} // namespace

bool EspNowBus::begin(const Config& cfg) {
    if (!cfg.groupName || cfg.maxQueueLength == 0 || cfg.maxPayloadBytes == 0) {
        ESP_LOGE(TAG, "invalid config (groupName/null or zero lengths)");
        return false;
    }
    // ensure single instance
    instance_ = this;
    config_ = cfg;

    if (!deriveKeys(config_.groupName)) {
        ESP_LOGE(TAG, "key derivation failed");
        return false;
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed");
        return false;
    }
    if (config_.useEncryption) {
        esp_now_set_pmk(derived_.pmk);
    }
    esp_now_register_send_cb(&EspNowBus::onSendStatic);
    esp_now_register_recv_cb(&EspNowBus::onReceiveStatic);

    // seed RNG for counters
    esp_fill_random(&msgCounter_, sizeof(msgCounter_));
    esp_fill_random(&broadcastSeq_, sizeof(broadcastSeq_));
    lastReseedMs_ = millis();

    // Ensure broadcast peer exists
    const uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    esp_now_peer_info_t bcastPeer = makePeerInfo(broadcastMac, false, nullptr);
    esp_now_add_peer(&bcastPeer);

    // Allocate payload pool
    poolCount_ = config_.maxQueueLength;
    payloadPool_ = static_cast<uint8_t*>(heap_caps_malloc(config_.maxPayloadBytes * poolCount_, MALLOC_CAP_DEFAULT));
    bufferUsed_ = static_cast<bool*>(heap_caps_malloc(poolCount_ * sizeof(bool), MALLOC_CAP_DEFAULT));
    if (!payloadPool_ || !bufferUsed_) {
        ESP_LOGE(TAG, "buffer allocation failed");
        end();
        return false;
    }
    memset(bufferUsed_, 0, poolCount_);

    sendQueue_ = xQueueCreate(config_.maxQueueLength, sizeof(TxItem));
    if (!sendQueue_) {
        ESP_LOGE(TAG, "queue allocation failed");
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
        ESP_LOGE(TAG, "send task create failed");
        end();
        return false;
    }
    ESP_LOGI(TAG, "begin success (enc=%d, queue=%u, payload=%u)", config_.useEncryption, config_.maxQueueLength, config_.maxPayloadBytes);
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
    ESP_LOGI(TAG, "end complete");
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

void EspNowBus::onAppAck(AppAckCallback cb) {
    onAppAck_ = cb;
}

bool EspNowBus::addPeer(const uint8_t mac[6]) {
    if (!mac) return false;
    int idx = findPeerIndex(mac);
    if (idx >= 0) return true;
    idx = ensurePeer(mac);
    if (idx < 0) return false;

    esp_now_peer_info_t info = makePeerInfo(mac, config_.useEncryption, derived_.lmk);
    esp_err_t err = esp_now_add_peer(&info);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        peers_[idx].inUse = false;
        ESP_LOGE(TAG, "add_peer failed err=%d", err);
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
    JoinReqPayload payload{};
    uint32_t t = millis();
    memcpy(payload.nonceA, &t, sizeof(t));
    esp_fill_random(payload.nonceA + sizeof(t), kNonceLen - sizeof(t));
    memcpy(pendingNonceA_, payload.nonceA, kNonceLen);
    if (storedNonceBValid_) {
        memcpy(payload.prevToken, storedNonceB_, kNonceLen);
    }
    pendingJoin_ = true;
    return enqueueCommon(Dest::Broadcast, PacketType::ControlJoinReq, bcast, &payload, sizeof(payload), kUseDefault);
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
            if (config_.useEncryption) {
                esp_now_peer_info_t info = makePeerInfo(mac, true, derived_.lmk);
                esp_now_add_peer(&info);
            }
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
    const bool needsAuth = (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck || pktType == PacketType::ControlAppAck);
    const size_t totalLen = kHeaderSize + (needsAuth ? (4 + kAuthTagLen) : 0) + len;
    if (totalLen > config_.maxPayloadBytes) {
        if (onSendResult_) onSendResult_(mac, SendStatus::TooLarge);
        ESP_LOGW(TAG, "payload too large (%u > %u)", static_cast<unsigned>(totalLen), config_.maxPayloadBytes);
        return false;
    }
    int16_t bufIdx = allocBuffer();
    if (bufIdx < 0) {
        if (onSendResult_) onSendResult_(mac, SendStatus::DroppedFull);
        ESP_LOGW(TAG, "queue full: drop");
        return false;
    }

    uint16_t msgId = 0;
    uint16_t seq = 0;
    if (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck) {
        seq = ++broadcastSeq_;
    } else {
        msgId = ++msgCounter_;
    }

    uint8_t* buf = bufferPtr(static_cast<uint16_t>(bufIdx));
    buf[0] = kMagic;
    buf[1] = kVersion;
    buf[2] = pktType;
    buf[3] = 0; // flags
    uint16_t idField = (pktType == PacketType::DataBroadcast || pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck) ? seq : msgId;
    buf[4] = static_cast<uint8_t>(idField & 0xFF);
    buf[5] = static_cast<uint8_t>((idField >> 8) & 0xFF);

    size_t cursor = kHeaderSize;
    if (needsAuth) {
        // groupId (4 bytes, LE)
        buf[cursor + 0] = static_cast<uint8_t>(derived_.groupId & 0xFF);
        buf[cursor + 1] = static_cast<uint8_t>((derived_.groupId >> 8) & 0xFF);
        buf[cursor + 2] = static_cast<uint8_t>((derived_.groupId >> 16) & 0xFF);
        buf[cursor + 3] = static_cast<uint8_t>((derived_.groupId >> 24) & 0xFF);
        cursor += 4;
    }

    memcpy(buf + cursor, data, len);
    cursor += len;

    if (needsAuth) {
        const uint8_t* key = (pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck) ? derived_.keyAuth : derived_.keyBcast;
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
    if (config_.enableAppAck && pktType == PacketType::DataUnicast) {
        item.expectAck = true;
        item.appAckDeadlineMs = millis() + config_.txTimeoutMs;
    } else if (pktType == PacketType::ControlAppAck) {
        item.expectAck = false;
    }

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

    const bool needsAuth = (type == PacketType::DataBroadcast || type == PacketType::ControlJoinReq || type == PacketType::ControlJoinAck || type == PacketType::ControlAppAck);
    if (needsAuth) {
        if (!instance_->verifyAuthTag(data, len, type)) {
            ESP_LOGW(TAG, "auth fail or group mismatch type=%u", type);
            return; // auth failed or groupId mismatch
        }
    }

    size_t cursor = kHeaderSize;
    if (needsAuth) {
        cursor += 4; // groupId already checked
    }
    const uint8_t* payload = p + cursor;
    int payloadLen = len - static_cast<int>(cursor + (needsAuth ? kAuthTagLen : 0));

    int idx = instance_->ensurePeer(mac);
    if (type == PacketType::DataUnicast) {
        if (idx >= 0 && instance_->peers_[idx].lastMsgId == id) {
            return; // duplicate
        }
        if (idx >= 0) instance_->peers_[idx].lastMsgId = id;
        // Auto app-level ACK
        if (instance_->config_.enableAppAck) {
            AppAckPayload ack{};
            ack.msgId = id;
            instance_->enqueueCommon(Dest::Unicast, PacketType::ControlAppAck, mac, &ack, sizeof(ack), kUseDefault);
            if (instance_->onAppAck_) {
                instance_->onAppAck_(mac, id);
            }
        }
    } else if (type == PacketType::DataBroadcast) {
        if (idx >= 0 && !instance_->acceptBroadcastSeq(instance_->peers_[idx], id)) {
            return;
        }
    } else if (type == PacketType::ControlJoinReq) {
        if (idx >= 0 && instance_->config_.canAcceptRegistrations) {
            // Drop duplicate JOIN by join window
            if (!instance_->acceptJoinSeq(instance_->peers_[idx], id)) {
                ESP_LOGW(TAG, "join replay drop");
                return;
            }
            instance_->addPeer(mac);
            if (payloadLen < static_cast<int>(sizeof(JoinReqPayload))) {
                ESP_LOGW(TAG, "join req too short");
                return;
            }
            const JoinReqPayload* req = reinterpret_cast<const JoinReqPayload*>(payload);
            bool resumed = memcmp(req->prevToken, instance_->peers_[idx].lastNonceB, kNonceLen) == 0;
            if (instance_->storedNonceBValid_ && !resumed) {
                ESP_LOGW(TAG, "join prevToken mismatch, treating as fresh");
                memset(instance_->peers_[idx].lastNonceB, 0, kNonceLen);
                storedNonceBValid_ = false;
            }
            JoinAckPayload ackPayload{};
            memcpy(ackPayload.nonceA, req->nonceA, kNonceLen); // echo nonceA
            esp_fill_random(ackPayload.nonceB, kNonceLen);     // nonceB
            memcpy(instance_->peers_[idx].lastNonceB, ackPayload.nonceB, kNonceLen);
            instance_->enqueueCommon(Dest::Unicast, PacketType::ControlJoinAck, mac, &ackPayload, sizeof(ackPayload), kUseDefault);
            ESP_LOGI(TAG, "join ack sent resumed=%d", resumed);
        }
        return;
    } else if (type == PacketType::ControlJoinAck) {
        if (!instance_->pendingJoin_) {
            ESP_LOGW(TAG, "unsolicited join ack ignored");
            return;
        }
        if (idx < 0) {
            idx = instance_->ensurePeer(mac);
            if (idx < 0) {
                ESP_LOGE(TAG, "ensurePeer failed for ack");
                return;
            }
        }
        if (payloadLen < static_cast<int>(sizeof(JoinAckPayload))) {
            ESP_LOGW(TAG, "join ack too short");
            return;
        }
        const JoinAckPayload* ack = reinterpret_cast<const JoinAckPayload*>(payload);
        if (memcmp(ack->nonceA, instance_->pendingNonceA_, kNonceLen) == 0) {
            instance_->pendingJoin_ = false; // authenticated responder (keyAuth + nonceA match)
            memcpy(instance_->peers_[idx].lastNonceB, ack->nonceB, kNonceLen);
            memcpy(instance_->storedNonceB_, ack->nonceB, kNonceLen);
            instance_->storedNonceBValid_ = true;
            ESP_LOGI(TAG, "join success, peer idx=%d", idx);
        } else {
            ESP_LOGW(TAG, "join ack nonce mismatch");
        }
        return;
    } else if (type == PacketType::ControlAppAck) {
        if (payloadLen < static_cast<int>(sizeof(AppAckPayload))) return;
        const AppAckPayload* ack = reinterpret_cast<const AppAckPayload*>(payload);
        if (instance_->txInFlight_ && instance_->currentTx_.expectAck && ack->msgId == instance_->currentTx_.msgId) {
            if (instance_->onSendResult_) instance_->onSendResult_(mac, SendStatus::AppAckReceived);
            instance_->freeBuffer(instance_->currentTx_.bufferIndex);
            instance_->txInFlight_ = false;
            instance_->retryCount_ = 0;
        }
        if (instance_->onAppAck_) {
            instance_->onAppAck_(mac, ack->msgId);
        }
        return;
    } else {
        ESP_LOGW(TAG, "unknown packet type=%u", type);
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
        if (entry.expectAck) {
            // Physical success; wait for app-ack to finalize
            txDeadlineMs_ = millis() + config_.txTimeoutMs;
            return;
        }
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
        if (timedOut) {
            ESP_LOGW(TAG, "send timeout mac=%02X:%02X:%02X:%02X:%02X:%02X", entry.mac[0], entry.mac[1], entry.mac[2], entry.mac[3], entry.mac[4], entry.mac[5]);
        } else {
            ESP_LOGE(TAG, "send failed mac=%02X:%02X:%02X:%02X:%02X:%02X", entry.mac[0], entry.mac[1], entry.mac[2], entry.mac[3], entry.mac[4], entry.mac[5]);
        }
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
        uint32_t nowMs = millis();
        reseedCounters(nowMs);
        if (!txInFlight_) {
            sendNextIfIdle(portMAX_DELAY);
            continue;
        }
        uint32_t deadline = txDeadlineMs_;
        TickType_t waitTicks = (deadline > nowMs) ? pdMS_TO_TICKS(deadline - nowMs) : 0;
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

        // app-ack timeout check
        if (txInFlight_ && currentTx_.expectAck) {
            uint32_t now2 = millis();
            if (now2 >= currentTx_.appAckDeadlineMs) {
                if (retryCount_ < config_.maxRetries) {
                    retryCount_++;
                    currentTx_.isRetry = true;
                    startSend(currentTx_);
                    currentTx_.appAckDeadlineMs = millis() + config_.txTimeoutMs;
                    if (onSendResult_) onSendResult_(currentTx_.mac, SendStatus::Retrying);
                } else {
                    if (onSendResult_) onSendResult_(currentTx_.mac, SendStatus::AppAckTimeout);
                    ESP_LOGW(TAG, "app-ack timeout mac=%02X:%02X:%02X:%02X:%02X:%02X", currentTx_.mac[0], currentTx_.mac[1], currentTx_.mac[2], currentTx_.mac[3], currentTx_.mac[4], currentTx_.mac[5]);
                    freeBuffer(currentTx_.bufferIndex);
                    txInFlight_ = false;
                    retryCount_ = 0;
                }
            }
        }
    }
}

bool EspNowBus::deriveKeys(const char* groupName) {
    uint8_t secret[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts_ret(&ctx, 0) != 0) return false;
    if (mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const unsigned char*>(groupName), strlen(groupName)) != 0) return false;
    if (mbedtls_sha256_finish_ret(&ctx, secret) != 0) return false;
    mbedtls_sha256_free(&ctx);

    auto derive = [&](const char* label, uint8_t* out, size_t outLen) {
        uint8_t digest[32];
        mbedtls_sha256_context c;
        mbedtls_sha256_init(&c);
        mbedtls_sha256_starts_ret(&c, 0);
        mbedtls_sha256_update_ret(&c, reinterpret_cast<const unsigned char*>(label), strlen(label));
        mbedtls_sha256_update_ret(&c, secret, sizeof(secret));
        mbedtls_sha256_finish_ret(&c, digest);
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

void EspNowBus::computeAuthTag(uint8_t* out, const uint8_t* msg, size_t len, const uint8_t* key) {
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, info, 1) != 0) {
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

bool EspNowBus::verifyAuthTag(const uint8_t* msg, size_t len, uint8_t pktType) {
    if (len < kHeaderSize + 4 + kAuthTagLen) return false;
    const uint8_t* groupPtr = msg + kHeaderSize;
    uint32_t gid = static_cast<uint32_t>(groupPtr[0]) |
                   (static_cast<uint32_t>(groupPtr[1]) << 8) |
                   (static_cast<uint32_t>(groupPtr[2]) << 16) |
                   (static_cast<uint32_t>(groupPtr[3]) << 24);
    if (gid != derived_.groupId) return false;
    const uint8_t* key = (pktType == PacketType::ControlJoinReq || pktType == PacketType::ControlJoinAck || pktType == PacketType::ControlAppAck) ? derived_.keyAuth : derived_.keyBcast;
    size_t tagOffset = len - kAuthTagLen;
    uint8_t calc[kAuthTagLen];
    computeAuthTag(calc, msg, tagOffset, key);
    return memcmp(calc, msg + tagOffset, kAuthTagLen) == 0;
}

void EspNowBus::reseedCounters(uint32_t now) {
    if (now - lastReseedMs_ < kReseedIntervalMs) return;
    lastReseedMs_ = now;
    esp_fill_random(&msgCounter_, sizeof(msgCounter_));
    esp_fill_random(&broadcastSeq_, sizeof(broadcastSeq_));
    ESP_LOGI(TAG, "reseed counters");
}

bool EspNowBus::acceptBroadcastSeq(PeerInfo& peer, uint16_t seq) {
    // Simple sliding window of size kReplayWindow with wrap-around handling
    uint16_t base = peer.lastBroadcastBase;
    uint16_t dist = static_cast<uint16_t>(seq - base);
    if (dist == 0) {
        return false; // duplicate
    }
    if (dist <= kReplayWindow) {
        uint64_t bit = 1ULL << (dist - 1);
        if (peer.bcastWindow & bit) {
            return false; // seen
        }
        peer.bcastWindow |= bit;
        return true;
    }
    // advance window
    if (dist > kReplayWindow) {
        uint16_t shift = dist - 1;
        if (shift >= 64) {
            peer.bcastWindow = 0;
        } else {
            peer.bcastWindow <<= shift;
        }
        peer.bcastWindow |= 1ULL;
        peer.lastBroadcastBase = seq;
        return true;
    }
    return true;
}

bool EspNowBus::acceptJoinSeq(PeerInfo& peer, uint16_t seq) {
    uint16_t base = peer.lastJoinSeqBase;
    uint16_t dist = static_cast<uint16_t>(seq - base);
    if (dist == 0) return false;
    if (dist <= kNonceWindow) {
        uint64_t bit = 1ULL << ((dist - 1) % 64);
        if (peer.joinWindow & bit) {
            return false;
        }
        peer.joinWindow |= bit;
        return true;
    }
    uint16_t shift = dist - 1;
    if (shift >= 64) {
        peer.joinWindow = 0;
    } else {
        peer.joinWindow <<= shift;
    }
    peer.joinWindow |= 1ULL;
    peer.lastJoinSeqBase = seq;
    return true;
}
