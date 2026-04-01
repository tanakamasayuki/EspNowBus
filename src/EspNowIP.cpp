#include "EspNowIP.h"

#include <string.h>
#include <lwip/lwip_napt.h>

EspNowIP *EspNowIP::instance_ = nullptr;
EspNowIPGateway *EspNowIPGateway::instance_ = nullptr;

namespace
{
constexpr uint8_t kGatewayHostOctet = 1;
constexpr uint8_t kLeasePoolStart = 100;
constexpr uint8_t kLeasePoolEnd = 199;
constexpr int kHelloRetryIntervalMs = 500;

esp_ip4_addr_t makeIp4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    esp_ip4_addr_t ip{};
    esp_netif_set_ip4_addr(&ip, a, b, c, d);
    return ip;
}

uint16_t frameType(const void *data, size_t len)
{
    if (!data || len < 14)
        return 0;
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    return static_cast<uint16_t>((bytes[12] << 8) | bytes[13]);
}
} // namespace

bool EspNowIP::begin(const Config &cfg)
{
    if (!cfg.groupName)
        return false;

    end();
    config_ = cfg;
    instance_ = this;

    EspNowBus::Config busCfg{};
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
    busCfg.autoJoinIntervalMs = cfg.autoJoinIntervalMs;
    busCfg.heartbeatIntervalMs = cfg.heartbeatIntervalMs;
    busCfg.taskCore = cfg.taskCore;
    busCfg.taskPriority = cfg.taskPriority;
    busCfg.taskStackSize = cfg.taskStackSize;
    busCfg.replayWindowBcast = cfg.replayWindowBcast;

    bus_.onJoinEvent(&EspNowIP::onJoinEventStatic);
    bus_.onReceive(&EspNowIP::onReceiveStatic);
    if (!bus_.begin(busCfg))
    {
        instance_ = nullptr;
        return false;
    }

    if (!createNetif())
    {
        bus_.end(false, true);
        instance_ = nullptr;
        return false;
    }

    running_ = true;
    return true;
}

void EspNowIP::end()
{
    if (running_)
    {
        bus_.end(false, true);
    }
    running_ = false;
    activeSession_ = -1;
    lastHelloAttemptMs_ = 0;
    clearLease();
    memset(sessions_, 0, sizeof(sessions_));
    destroyNetif();
    if (instance_ == this)
        instance_ = nullptr;
}

void EspNowIP::poll()
{
    if (!running_)
        return;

    syncBusPeers();
    tryHello();
}

esp_netif_t *EspNowIP::netif()
{
    return netif_;
}

bool EspNowIP::linkUp() const
{
    return running_ && activeSession_ >= 0 && hasLease_;
}

bool EspNowIP::hasLease() const
{
    return hasLease_;
}

void EspNowIP::onJoinEventStatic(const uint8_t mac[6], bool accepted, bool isAck)
{
    if (instance_)
        instance_->onJoinEvent(mac, accepted, isAck);
}

void EspNowIP::onReceiveStatic(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
    if (instance_)
        instance_->onReceive(mac, data, len, wasRetry, isBroadcast);
}

void EspNowIP::onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck)
{
    if (!mac)
        return;

    if (accepted)
    {
        int idx = ensureSession(mac);
        if (idx >= 0)
            sessions_[idx].ready = true;
        return;
    }

    if (isAck)
        return;

    int idx = findSessionByMac(mac);
    if (idx >= 0 && idx == activeSession_)
    {
        activeSession_ = -1;
        clearLease();
    }
    removeSessionByMac(mac);
}

void EspNowIP::onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
    (void)wasRetry;
    if (!mac || !data || isBroadcast || len < sizeof(AppHeader))
        return;

    const AppHeader *app = reinterpret_cast<const AppHeader *>(data);
    if (app->protocolId != kProtocolIdIp || app->protocolVer != kProtocolVersion)
        return;

    switch (app->packetType)
    {
    case IpControlLease:
    {
        if (len < sizeof(AppHeader) + sizeof(LeasePayload))
            return;

        int idx = ensureSession(mac);
        if (idx < 0)
            return;

        const LeasePayload *lease = reinterpret_cast<const LeasePayload *>(data + sizeof(AppHeader));
        if (!applyLease(*lease))
            return;

        sessions_[idx].ready = true;
        sessions_[idx].leaseOk = true;
        activateSession(idx);
        break;
    }
    case IpData:
        receiveIpData(mac, data + sizeof(AppHeader), len - sizeof(AppHeader));
        break;
    default:
        break;
    }
}

int EspNowIP::findSessionByMac(const uint8_t mac[6]) const
{
    for (size_t i = 0; i < kMaxCandidates; ++i)
    {
        if (!sessions_[i].inUse)
            continue;
        if (memcmp(sessions_[i].mac, mac, 6) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

int EspNowIP::ensureSession(const uint8_t mac[6])
{
    int idx = findSessionByMac(mac);
    if (idx >= 0)
        return idx;

    for (size_t i = 0; i < kMaxCandidates; ++i)
    {
        if (sessions_[i].inUse)
            continue;
        sessions_[i].inUse = true;
        sessions_[i].ready = false;
        sessions_[i].helloSent = false;
        sessions_[i].leaseOk = false;
        memcpy(sessions_[i].mac, mac, 6);
        return static_cast<int>(i);
    }
    return -1;
}

void EspNowIP::removeSessionByMac(const uint8_t mac[6])
{
    int idx = findSessionByMac(mac);
    if (idx < 0)
        return;
    memset(&sessions_[idx], 0, sizeof(sessions_[idx]));
}

void EspNowIP::syncBusPeers()
{
    for (size_t i = 0; i < bus_.peerCount(); ++i)
    {
        uint8_t mac[6]{};
        if (!bus_.getPeer(i, mac))
            continue;
        int idx = ensureSession(mac);
        if (idx >= 0)
            sessions_[idx].ready = true;
    }
}

void EspNowIP::tryHello()
{
    if (activeSession_ >= 0 && activeSession_ < static_cast<int>(kMaxCandidates) && sessions_[activeSession_].inUse && sessions_[activeSession_].leaseOk)
        return;

    uint32_t now = millis();
    if (lastHelloAttemptMs_ != 0 && (now - lastHelloAttemptMs_) < kHelloRetryIntervalMs)
        return;

    for (size_t i = 0; i < kMaxCandidates; ++i)
    {
        if (!sessions_[i].inUse || !sessions_[i].ready)
            continue;
        if (sessions_[i].leaseOk)
        {
            activateSession(static_cast<int>(i));
            return;
        }
        if (sessions_[i].helloSent)
            continue;

        AppHeader app{};
        app.protocolId = kProtocolIdIp;
        app.protocolVer = kProtocolVersion;
        app.packetType = IpControlHello;
        app.flags = 0;

        HelloPayload hello{};
        hello.maxReassemblyBytes = config_.maxReassemblyBytes;
        hello.mtu = config_.mtu;

        uint8_t buffer[sizeof(AppHeader) + sizeof(HelloPayload)]{};
        memcpy(buffer, &app, sizeof(app));
        memcpy(buffer + sizeof(app), &hello, sizeof(hello));
        if (bus_.sendTo(sessions_[i].mac, buffer, sizeof(buffer), EspNowBus::kUseDefault))
        {
            sessions_[i].helloSent = true;
            lastHelloAttemptMs_ = now;
        }
        break;
    }
}

void EspNowIP::activateSession(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(kMaxCandidates))
        return;
    if (!sessions_[idx].inUse || !sessions_[idx].leaseOk)
        return;
    activeSession_ = idx;
    hasLease_ = true;
    updateNetifLink(true);
    if (netif_)
        esp_netif_set_default_netif(netif_);
}

bool EspNowIP::sendIpDataToActive(const void *data, size_t len)
{
    if (!data || len == 0 || activeSession_ < 0 || activeSession_ >= static_cast<int>(kMaxCandidates))
        return false;
    if (!sessions_[activeSession_].inUse || !sessions_[activeSession_].leaseOk)
        return false;

    const size_t total = sizeof(AppHeader) + len;
    if (total > config_.maxPayloadBytes)
        return false;

    uint8_t *buffer = static_cast<uint8_t *>(malloc(total));
    if (!buffer)
        return false;

    AppHeader app{};
    app.protocolId = kProtocolIdIp;
    app.protocolVer = kProtocolVersion;
    app.packetType = IpData;
    app.flags = 0;
    log_d("[EspNowIP] tx active mac=%02X:%02X:%02X:%02X:%02X:%02X len=%u ethType=0x%04X",
          sessions_[activeSession_].mac[0], sessions_[activeSession_].mac[1], sessions_[activeSession_].mac[2],
          sessions_[activeSession_].mac[3], sessions_[activeSession_].mac[4], sessions_[activeSession_].mac[5],
          static_cast<unsigned>(len), frameType(data, len));
    memcpy(buffer, &app, sizeof(app));
    memcpy(buffer + sizeof(app), data, len);
    bool ok = bus_.sendTo(sessions_[activeSession_].mac, buffer, total, EspNowBus::kUseDefault);
    free(buffer);
    return ok;
}

void EspNowIP::receiveIpData(const uint8_t *mac, const uint8_t *payload, size_t len)
{
    if (!netif_ || !payload || len == 0 || activeSession_ < 0 || !hasLease_)
        return;
    if (memcmp(sessions_[activeSession_].mac, mac, 6) != 0)
        return;

    log_d("[EspNowIP] rx active mac=%02X:%02X:%02X:%02X:%02X:%02X len=%u ethType=0x%04X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
          static_cast<unsigned>(len), frameType(payload, len));

    void *copy = malloc(len);
    if (!copy)
        return;
    memcpy(copy, payload, len);
    esp_netif_receive(netif_, copy, len, copy);
}

bool EspNowIP::createNetif()
{
    destroyNetif();
    esp_netif_init();

    memset(&driver_, 0, sizeof(driver_));
    driver_.owner = this;
    driver_.base.post_attach = &EspNowIP::netifPostAttach;

    memset(&netifInherent_, 0, sizeof(netifInherent_));
    netifInherent_.flags = ESP_NETIF_FLAG_AUTOUP;
    netifInherent_.get_ip_event = 0;
    netifInherent_.lost_ip_event = 0;
    netifInherent_.if_key = config_.ifKey;
    netifInherent_.if_desc = config_.ifDesc;
    netifInherent_.route_prio = 5;

    memset(&netifConfig_, 0, sizeof(netifConfig_));
    netifConfig_.base = &netifInherent_;
    netifConfig_.driver = nullptr;
    netifConfig_.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;

    netif_ = esp_netif_new(&netifConfig_);
    if (!netif_)
        return false;

    if (esp_netif_attach(netif_, &driver_) != ESP_OK)
    {
        destroyNetif();
        return false;
    }

    uint8_t mac[6]{};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
        esp_netif_set_mac(netif_, mac);

    return true;
}

void EspNowIP::destroyNetif()
{
    if (!netif_)
        return;
    updateNetifLink(false);
    esp_netif_destroy(netif_);
    netif_ = nullptr;
    memset(&driver_, 0, sizeof(driver_));
    memset(&driverIfConfig_, 0, sizeof(driverIfConfig_));
    memset(&netifInherent_, 0, sizeof(netifInherent_));
    memset(&netifConfig_, 0, sizeof(netifConfig_));
}

void EspNowIP::clearLease()
{
    hasLease_ = false;
    memset(&lease_, 0, sizeof(lease_));
    updateNetifLink(false);
}

bool EspNowIP::applyLease(const LeasePayload &lease)
{
    if (!netif_ || lease.deviceIpv4 == 0 || lease.gatewayIpv4 == 0 || lease.netmaskIpv4 == 0)
        return false;

    LeaseState next{};
    next.valid = true;
    next.ipInfo.ip.addr = lease.deviceIpv4;
    next.ipInfo.gw.addr = lease.gatewayIpv4;
    next.ipInfo.netmask.addr = lease.netmaskIpv4;
    next.dns1.ip.type = ESP_IPADDR_TYPE_V4;
    next.dns1.ip.u_addr.ip4.addr = lease.dns1Ipv4;
    next.dns2.ip.type = ESP_IPADDR_TYPE_V4;
    next.dns2.ip.u_addr.ip4.addr = lease.dns2Ipv4;
    next.mtu = lease.mtu;
    next.leaseSeconds = lease.leaseSeconds;

    esp_netif_dhcpc_stop(netif_);
    if (esp_netif_set_ip_info(netif_, &next.ipInfo) != ESP_OK)
        return false;
    if (lease.dns1Ipv4 != 0)
        esp_netif_set_dns_info(netif_, ESP_NETIF_DNS_MAIN, &next.dns1);
    if (lease.dns2Ipv4 != 0)
        esp_netif_set_dns_info(netif_, ESP_NETIF_DNS_BACKUP, &next.dns2);

    lease_ = next;
    return true;
}

void EspNowIP::updateNetifLink(bool up)
{
    if (!netif_)
        return;

    if (up)
    {
        esp_netif_action_start(netif_, nullptr, 0, nullptr);
        esp_netif_action_connected(netif_, nullptr, 0, nullptr);
    }
    else
    {
        esp_netif_action_disconnected(netif_, nullptr, 0, nullptr);
        esp_netif_action_stop(netif_, nullptr, 0, nullptr);
    }
}

esp_err_t EspNowIP::netifPostAttach(esp_netif_t *netif, esp_netif_iodriver_handle h)
{
    if (!netif || !h)
        return ESP_ERR_INVALID_ARG;

    NetifDriver *driver = static_cast<NetifDriver *>(h);
    driver->base.netif = netif;

    esp_netif_driver_ifconfig_t ifcfg{};
    ifcfg.handle = h;
    ifcfg.transmit = &EspNowIP::netifTransmit;
    ifcfg.driver_free_rx_buffer = &EspNowIP::netifFreeRxBuffer;
    if (esp_netif_set_driver_config(netif, &ifcfg) != ESP_OK)
        return ESP_ERR_ESP_NETIF_DRIVER_ATTACH_FAILED;

    if (driver->owner)
        driver->owner->driverIfConfig_ = ifcfg;
    return ESP_OK;
}

esp_err_t EspNowIP::netifTransmit(void *h, void *buffer, size_t len)
{
    NetifDriver *driver = static_cast<NetifDriver *>(h);
    if (!driver || !driver->owner || !buffer || len == 0)
        return ESP_ERR_INVALID_ARG;

    EspNowIP *owner = driver->owner;
    if (owner->activeSession_ < 0 || !owner->hasLease_)
        return ESP_ERR_ESP_NETIF_TX_FAILED;
    return owner->sendIpDataToActive(buffer, len) ? ESP_OK : ESP_ERR_ESP_NETIF_TX_FAILED;
}

void EspNowIP::netifFreeRxBuffer(void *h, void *buffer)
{
    (void)h;
    free(buffer);
}

bool EspNowIPGateway::begin(const Config &cfg)
{
    if (!cfg.groupName)
        return false;

    end();
    config_ = cfg;
    instance_ = this;

    EspNowBus::Config busCfg{};
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
    busCfg.autoJoinIntervalMs = cfg.autoJoinIntervalMs;
    busCfg.heartbeatIntervalMs = cfg.heartbeatIntervalMs;
    busCfg.taskCore = cfg.taskCore;
    busCfg.taskPriority = cfg.taskPriority;
    busCfg.taskStackSize = cfg.taskStackSize;
    busCfg.replayWindowBcast = cfg.replayWindowBcast;

    bus_.onReceive(&EspNowIPGateway::onReceiveStatic);
    if (!bus_.begin(busCfg))
        return false;

    if (!createNetif())
    {
        bus_.end(false, true);
        return false;
    }
    if (!configureNetif())
    {
        destroyNetif();
        bus_.end(false, true);
        return false;
    }
    enableUplinkNat();

    running_ = true;
    return true;
}

void EspNowIPGateway::end()
{
    if (running_)
    {
        bus_.end(false, true);
    }
    running_ = false;
    memset(leases_, 0, sizeof(leases_));
    destroyNetif();
    if (instance_ == this)
        instance_ = nullptr;
}

void EspNowIPGateway::poll()
{
    if (!running_)
        return;
}

void EspNowIPGateway::onReceiveStatic(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
    if (instance_)
        instance_->onReceive(mac, data, len, wasRetry, isBroadcast);
}

void EspNowIPGateway::onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast)
{
    (void)wasRetry;
    if (!running_ || !mac || !data || isBroadcast || len < sizeof(EspNowIP::AppHeader))
        return;

    const EspNowIP::AppHeader *app = reinterpret_cast<const EspNowIP::AppHeader *>(data);
    if (app->protocolId != EspNowIP::kProtocolIdIp || app->protocolVer != EspNowIP::kProtocolVersion)
        return;

    switch (app->packetType)
    {
    case EspNowIP::IpControlHello:
        if (len < sizeof(EspNowIP::AppHeader) + sizeof(EspNowIP::HelloPayload))
            return;
        sendLease(mac);
        break;
    case EspNowIP::IpData:
        receiveIpData(mac, data + sizeof(EspNowIP::AppHeader), len - sizeof(EspNowIP::AppHeader));
        break;
    default:
        break;
    }
}

bool EspNowIPGateway::sendLease(const uint8_t mac[6]) const
{
    int idx = const_cast<EspNowIPGateway *>(this)->ensureLease(mac);
    if (idx < 0)
        return false;

    EspNowIP::AppHeader app{};
    app.protocolId = EspNowIP::kProtocolIdIp;
    app.protocolVer = EspNowIP::kProtocolVersion;
    app.packetType = EspNowIP::IpControlLease;
    app.flags = 0;

    const LeaseEntry &entry = leases_[idx];
    esp_ip4_addr_t gatewayIp = makeIp4(config_.subnetOctet1, config_.subnetOctet2, config_.subnetOctet3, kGatewayHostOctet);
    esp_ip4_addr_t deviceIp = makeIp4(config_.subnetOctet1, config_.subnetOctet2, config_.subnetOctet3, entry.hostOctet);
    esp_ip4_addr_t netmask = makeIp4(255, 255, 255, 0);
    esp_netif_dns_info_t dns1{};
    esp_netif_dns_info_t dns2{};

    if (config_.uplink)
    {
        esp_netif_get_dns_info(config_.uplink, ESP_NETIF_DNS_MAIN, &dns1);
        esp_netif_get_dns_info(config_.uplink, ESP_NETIF_DNS_BACKUP, &dns2);
    }

    EspNowIP::LeasePayload lease{};
    lease.deviceIpv4 = deviceIp.addr;
    lease.gatewayIpv4 = gatewayIp.addr;
    lease.netmaskIpv4 = netmask.addr;
    lease.dns1Ipv4 = dns1.ip.type == ESP_IPADDR_TYPE_V4 ? dns1.ip.u_addr.ip4.addr : 0;
    lease.dns2Ipv4 = dns2.ip.type == ESP_IPADDR_TYPE_V4 ? dns2.ip.u_addr.ip4.addr : 0;
    lease.mtu = config_.mtu;
    lease.leaseSeconds = static_cast<uint16_t>(config_.leaseSeconds > 0xFFFF ? 0xFFFF : config_.leaseSeconds);

    uint8_t buffer[sizeof(EspNowIP::AppHeader) + sizeof(EspNowIP::LeasePayload)]{};
    memcpy(buffer, &app, sizeof(app));
    memcpy(buffer + sizeof(app), &lease, sizeof(lease));
    log_d("[EspNowIPGW] lease mac=%02X:%02X:%02X:%02X:%02X:%02X ip=" IPSTR " gw=" IPSTR " dns1=" IPSTR " dns2=" IPSTR,
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
          IP2STR(&deviceIp), IP2STR(&gatewayIp),
          IP2STR(&dns1.ip.u_addr.ip4), IP2STR(&dns2.ip.u_addr.ip4));
    return const_cast<EspNowBus &>(bus_).sendTo(mac, buffer, sizeof(buffer), EspNowBus::kUseDefault);
}

void EspNowIPGateway::receiveIpData(const uint8_t mac[6], const uint8_t *payload, size_t len)
{
    if (!netif_ || !payload || len == 0)
        return;
    ensureLease(mac);
    log_d("[EspNowIPGW] rx from mac=%02X:%02X:%02X:%02X:%02X:%02X len=%u ethType=0x%04X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
          static_cast<unsigned>(len), frameType(payload, len));

    void *copy = malloc(len);
    if (!copy)
        return;
    memcpy(copy, payload, len);
    esp_netif_receive(netif_, copy, len, copy);
}

int EspNowIPGateway::findLeaseByMac(const uint8_t mac[6]) const
{
    for (size_t i = 0; i < config_.maxDevices && i < (sizeof(leases_) / sizeof(leases_[0])); ++i)
    {
        if (!leases_[i].inUse)
            continue;
        if (memcmp(leases_[i].mac, mac, 6) == 0)
            return static_cast<int>(i);
    }
    return -1;
}

int EspNowIPGateway::findLeaseByIpv4(uint32_t ipv4) const
{
    for (size_t i = 0; i < config_.maxDevices && i < (sizeof(leases_) / sizeof(leases_[0])); ++i)
    {
        if (!leases_[i].inUse)
            continue;
        if (leases_[i].ipv4 == ipv4)
            return static_cast<int>(i);
    }
    return -1;
}

int EspNowIPGateway::ensureLease(const uint8_t mac[6])
{
    int idx = findLeaseByMac(mac);
    if (idx >= 0)
        return idx;

    const size_t limit = min(static_cast<size_t>(config_.maxDevices), sizeof(leases_) / sizeof(leases_[0]));
    for (size_t i = 0; i < limit; ++i)
    {
        if (leases_[i].inUse)
            continue;
        leases_[i].inUse = true;
        memcpy(leases_[i].mac, mac, 6);
        leases_[i].hostOctet = static_cast<uint8_t>(kLeasePoolStart + i);
        leases_[i].ipv4 = ESP_IP4TOADDR(config_.subnetOctet1, config_.subnetOctet2, config_.subnetOctet3, leases_[i].hostOctet);
        if (leases_[i].hostOctet > kLeasePoolEnd)
        {
            memset(&leases_[i], 0, sizeof(leases_[i]));
            return -1;
        }
        return static_cast<int>(i);
    }
    return -1;
}

bool EspNowIPGateway::createNetif()
{
    destroyNetif();
    esp_netif_init();

    memset(&driver_, 0, sizeof(driver_));
    driver_.owner = this;
    driver_.base.post_attach = &EspNowIPGateway::netifPostAttach;

    memset(&netifInherent_, 0, sizeof(netifInherent_));
    netifInherent_.flags = ESP_NETIF_FLAG_AUTOUP;
    netifInherent_.get_ip_event = 0;
    netifInherent_.lost_ip_event = 0;
    netifInherent_.if_key = config_.ifKey;
    netifInherent_.if_desc = config_.ifDesc;
    netifInherent_.route_prio = 10;

    memset(&netifConfig_, 0, sizeof(netifConfig_));
    netifConfig_.base = &netifInherent_;
    netifConfig_.driver = nullptr;
    netifConfig_.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;

    netif_ = esp_netif_new(&netifConfig_);
    if (!netif_)
        return false;
    if (esp_netif_attach(netif_, &driver_) != ESP_OK)
    {
        destroyNetif();
        return false;
    }

    uint8_t mac[6]{};
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK)
        esp_netif_set_mac(netif_, mac);
    return true;
}

void EspNowIPGateway::destroyNetif()
{
    if (!netif_)
        return;
    updateNetifLink(false);
    esp_netif_destroy(netif_);
    netif_ = nullptr;
    memset(&driver_, 0, sizeof(driver_));
    memset(&driverIfConfig_, 0, sizeof(driverIfConfig_));
    memset(&netifInherent_, 0, sizeof(netifInherent_));
    memset(&netifConfig_, 0, sizeof(netifConfig_));
}

bool EspNowIPGateway::configureNetif()
{
    if (!netif_)
        return false;

    esp_netif_dhcpc_stop(netif_);
    esp_netif_ip_info_t info{};
    info.ip = makeIp4(config_.subnetOctet1, config_.subnetOctet2, config_.subnetOctet3, kGatewayHostOctet);
    info.gw = info.ip;
    info.netmask = makeIp4(255, 255, 255, 0);
    if (esp_netif_set_ip_info(netif_, &info) != ESP_OK)
        return false;

    esp_netif_dns_info_t copiedDns{};
    if (config_.uplink)
    {
        if (esp_netif_get_dns_info(config_.uplink, ESP_NETIF_DNS_MAIN, &copiedDns) == ESP_OK &&
            copiedDns.ip.u_addr.ip4.addr != 0)
        {
            esp_netif_set_dns_info(netif_, ESP_NETIF_DNS_MAIN, &copiedDns);
        }

        // Keep the real internet-facing uplink as the default route owner.
        esp_netif_set_default_netif(config_.uplink);
    }

    updateNetifLink(true);
    log_i("[EspNowIPGW] bus netif ip=" IPSTR " gw=" IPSTR " mask=" IPSTR " dns=" IPSTR " uplink=%p",
          IP2STR(&info.ip), IP2STR(&info.gw), IP2STR(&info.netmask),
          IP2STR(&copiedDns.ip.u_addr.ip4), config_.uplink);
    return true;
}

void EspNowIPGateway::updateNetifLink(bool up)
{
    if (!netif_)
        return;
    if (up)
    {
        esp_netif_action_start(netif_, nullptr, 0, nullptr);
        esp_netif_action_connected(netif_, nullptr, 0, nullptr);
    }
    else
    {
        esp_netif_action_disconnected(netif_, nullptr, 0, nullptr);
        esp_netif_action_stop(netif_, nullptr, 0, nullptr);
    }
}

bool EspNowIPGateway::enableUplinkNat()
{
    if (!config_.uplink || !netif_)
    {
        log_w("[EspNowIPGW] uplink or bus netif not set; NAT disabled");
        return false;
    }

    const esp_err_t err = esp_netif_napt_enable(netif_);
    if (err != ESP_OK)
    {
        log_w("[EspNowIPGW] esp_netif_napt_enable failed err=0x%x bus=%p uplink=%p", err, netif_, config_.uplink);
        return false;
    }

    esp_netif_ip_info_t busInfo{};
    esp_netif_ip_info_t uplinkInfo{};
    esp_netif_dns_info_t uplinkDns{};
    if (esp_netif_get_ip_info(netif_, &busInfo) == ESP_OK)
    {
        log_i("[EspNowIPGW] bus NAT ip=" IPSTR " gw=" IPSTR " mask=" IPSTR,
              IP2STR(&busInfo.ip), IP2STR(&busInfo.gw), IP2STR(&busInfo.netmask));
    }
    if (esp_netif_get_ip_info(config_.uplink, &uplinkInfo) == ESP_OK)
    {
        esp_netif_get_dns_info(config_.uplink, ESP_NETIF_DNS_MAIN, &uplinkDns);
        log_i("[EspNowIPGW] uplink default ip=" IPSTR " gw=" IPSTR " mask=" IPSTR " dns=" IPSTR,
              IP2STR(&uplinkInfo.ip), IP2STR(&uplinkInfo.gw), IP2STR(&uplinkInfo.netmask),
              IP2STR(&uplinkDns.ip.u_addr.ip4));
    }
    else
    {
        log_i("[EspNowIPGW] uplink default set, but ip info unavailable");
    }
    log_i("[EspNowIPGW] NAT enabled on bus=%p uplink=%p", netif_, config_.uplink);
    return true;
}

bool EspNowIPGateway::sendFrameToMac(const uint8_t mac[6], const void *data, size_t len)
{
    if (!data || len == 0)
        return false;
    const size_t total = sizeof(EspNowIP::AppHeader) + len;
    if (total > config_.maxPayloadBytes)
        return false;

    uint8_t *buffer = static_cast<uint8_t *>(malloc(total));
    if (!buffer)
        return false;

    EspNowIP::AppHeader app{};
    app.protocolId = EspNowIP::kProtocolIdIp;
    app.protocolVer = EspNowIP::kProtocolVersion;
    app.packetType = EspNowIP::IpData;
    app.flags = 0;
    memcpy(buffer, &app, sizeof(app));
    memcpy(buffer + sizeof(app), data, len);

    bool ok = false;
    static const uint8_t kBroadcastMac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    if (memcmp(mac, kBroadcastMac, 6) == 0)
        ok = bus_.broadcast(buffer, total, EspNowBus::kUseDefault);
    else
        ok = bus_.sendTo(mac, buffer, total, EspNowBus::kUseDefault);
    free(buffer);
    return ok;
}

esp_err_t EspNowIPGateway::netifPostAttach(esp_netif_t *netif, esp_netif_iodriver_handle h)
{
    if (!netif || !h)
        return ESP_ERR_INVALID_ARG;
    NetifDriver *driver = static_cast<NetifDriver *>(h);
    driver->base.netif = netif;

    esp_netif_driver_ifconfig_t ifcfg{};
    ifcfg.handle = h;
    ifcfg.transmit = &EspNowIPGateway::netifTransmit;
    ifcfg.driver_free_rx_buffer = &EspNowIPGateway::netifFreeRxBuffer;
    if (esp_netif_set_driver_config(netif, &ifcfg) != ESP_OK)
        return ESP_ERR_ESP_NETIF_DRIVER_ATTACH_FAILED;
    if (driver->owner)
        driver->owner->driverIfConfig_ = ifcfg;
    return ESP_OK;
}

esp_err_t EspNowIPGateway::netifTransmit(void *h, void *buffer, size_t len)
{
    NetifDriver *driver = static_cast<NetifDriver *>(h);
    if (!driver || !driver->owner || !buffer || len < 14)
        return ESP_ERR_INVALID_ARG;

    const uint8_t *frame = static_cast<const uint8_t *>(buffer);
    const uint8_t *dstMac = frame;
    log_d("[EspNowIPGW] tx dst=%02X:%02X:%02X:%02X:%02X:%02X len=%u ethType=0x%04X",
          dstMac[0], dstMac[1], dstMac[2], dstMac[3], dstMac[4], dstMac[5],
          static_cast<unsigned>(len), frameType(buffer, len));
    return driver->owner->sendFrameToMac(dstMac, buffer, len) ? ESP_OK : ESP_ERR_ESP_NETIF_TX_FAILED;
}

void EspNowIPGateway::netifFreeRxBuffer(void *h, void *buffer)
{
    (void)h;
    free(buffer);
}
