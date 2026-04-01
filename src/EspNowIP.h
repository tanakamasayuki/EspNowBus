#pragma once

#include <Arduino.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include "EspNowBus.h"

class EspNowIP
{
public:
    struct Config
    {
        const char *groupName = nullptr;
        const char *ifKey = "enip0";
        const char *ifDesc = "ESP-NOW IP";
        bool useEncryption = true;
        bool enablePeerAuth = true;
        bool enableAppAck = true;
        int8_t channel = -1;
        wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L;
        uint16_t maxPayloadBytes = EspNowBus::kMaxPayloadDefault;
        uint16_t mtu = 1420;
        uint16_t maxReassemblyBytes = 1536;
        uint8_t maxReassemblyPackets = 4;
        uint32_t leaseTimeoutMs = 60000;
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

    EspNowIP() = default;

    bool begin(const Config &cfg);
    void end();
    void poll();

    esp_netif_t *netif();
    bool linkUp() const;
    bool hasLease() const;

private:
    static constexpr uint8_t kProtocolIdIp = 0x02;
    static constexpr uint8_t kProtocolVersion = 1;
    enum PacketType : uint8_t
    {
        IpControlHello = 1,
        IpControlLease = 2,
        IpControlKeepAlive = 3,
        IpData = 4,
    };

#pragma pack(push, 1)
    struct AppHeader
    {
        uint8_t protocolId;
        uint8_t protocolVer;
        uint8_t packetType;
        uint8_t flags;
    };

    struct HelloPayload
    {
        uint16_t maxReassemblyBytes;
        uint16_t mtu;
        uint8_t capabilityFlags;
        uint8_t reserved;
    };

    struct LeasePayload
    {
        uint32_t deviceIpv4;
        uint32_t gatewayIpv4;
        uint32_t netmaskIpv4;
        uint32_t dns1Ipv4;
        uint32_t dns2Ipv4;
        uint16_t mtu;
        uint16_t leaseSeconds;
    };
#pragma pack(pop)

    struct SessionCandidate
    {
        bool inUse = false;
        bool ready = false;
        bool helloSent = false;
        bool leaseOk = false;
        uint8_t mac[6]{};
    };

    struct LeaseState
    {
        bool valid = false;
        esp_netif_ip_info_t ipInfo{};
        esp_netif_dns_info_t dns1{};
        esp_netif_dns_info_t dns2{};
        uint16_t mtu = 0;
        uint16_t leaseSeconds = 0;
    };

    struct NetifDriver
    {
        esp_netif_driver_base_t base{};
        EspNowIP *owner = nullptr;
    };

    static constexpr size_t kMaxCandidates = 8;
    static EspNowIP *instance_;

    Config config_{};
    EspNowBus bus_{};
    SessionCandidate sessions_[kMaxCandidates]{};
    LeaseState lease_{};
    esp_netif_t *netif_ = nullptr;
    esp_netif_inherent_config_t netifInherent_{};
    esp_netif_config_t netifConfig_{};
    esp_netif_driver_ifconfig_t driverIfConfig_{};
    NetifDriver driver_{};
    int activeSession_ = -1;
    bool hasLease_ = false;
    bool running_ = false;
    uint32_t lastHelloAttemptMs_ = 0;

    static void onJoinEventStatic(const uint8_t mac[6], bool accepted, bool isAck);
    static void onReceiveStatic(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast);
    void onJoinEvent(const uint8_t mac[6], bool accepted, bool isAck);
    void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast);
    int findSessionByMac(const uint8_t mac[6]) const;
    int ensureSession(const uint8_t mac[6]);
    void removeSessionByMac(const uint8_t mac[6]);
    void syncBusPeers();
    void tryHello();
    void activateSession(int idx);
    bool sendIpDataToActive(const void *data, size_t len);
    void receiveIpData(const uint8_t *mac, const uint8_t *payload, size_t len);
    bool createNetif();
    void destroyNetif();
    void clearLease();
    bool applyLease(const LeasePayload &lease);
    void updateNetifLink(bool up);
    static esp_err_t netifPostAttach(esp_netif_t *netif, esp_netif_iodriver_handle h);
    static esp_err_t netifTransmit(void *h, void *buffer, size_t len);
    static void netifFreeRxBuffer(void *h, void *buffer);

    friend class EspNowIPGateway;
};

class EspNowIPGateway
{
public:
    struct Config
    {
        const char *groupName = nullptr;
        esp_netif_t *uplink = nullptr;
        bool useEncryption = true;
        bool enablePeerAuth = true;
        bool enableAppAck = true;
        int8_t channel = -1;
        wifi_phy_rate_t phyRate = WIFI_PHY_RATE_11M_L;
        uint16_t maxPayloadBytes = EspNowBus::kMaxPayloadDefault;
        uint16_t mtu = 1420;
        uint8_t maxDevices = 6;
        uint32_t leaseSeconds = 3600;
        const char *ifKey = "enipg0";
        const char *ifDesc = "ESP-NOW IP GW";
        uint8_t subnetOctet1 = 10;
        uint8_t subnetOctet2 = 201;
        uint8_t subnetOctet3 = 0;
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

    EspNowIPGateway() = default;

    bool begin(const Config &cfg);
    void end();
    void poll();

private:
    struct LeaseEntry
    {
        bool inUse = false;
        uint8_t mac[6]{};
        uint8_t hostOctet = 0;
        uint32_t ipv4 = 0;
    };

    struct NetifDriver
    {
        esp_netif_driver_base_t base{};
        EspNowIPGateway *owner = nullptr;
    };

    static EspNowIPGateway *instance_;

    Config config_{};
    EspNowBus bus_{};
    LeaseEntry leases_[32]{};
    esp_netif_t *netif_ = nullptr;
    esp_netif_inherent_config_t netifInherent_{};
    esp_netif_config_t netifConfig_{};
    esp_netif_driver_ifconfig_t driverIfConfig_{};
    NetifDriver driver_{};
    bool running_ = false;

    static void onReceiveStatic(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast);
    void onReceive(const uint8_t *mac, const uint8_t *data, size_t len, bool wasRetry, bool isBroadcast);
    bool sendLease(const uint8_t mac[6]) const;
    void receiveIpData(const uint8_t mac[6], const uint8_t *payload, size_t len);
    int findLeaseByMac(const uint8_t mac[6]) const;
    int findLeaseByIpv4(uint32_t ipv4) const;
    int ensureLease(const uint8_t mac[6]);
    bool createNetif();
    void destroyNetif();
    bool configureNetif();
    void updateNetifLink(bool up);
    bool enableUplinkNat();
    bool sendFrameToMac(const uint8_t mac[6], const void *data, size_t len);
    static esp_err_t netifPostAttach(esp_netif_t *netif, esp_netif_iodriver_handle h);
    static esp_err_t netifTransmit(void *h, void *buffer, size_t len);
    static void netifFreeRxBuffer(void *h, void *buffer);
};
