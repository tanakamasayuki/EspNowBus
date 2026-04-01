# EspNowIP Specification

## 1. Terms
- `device`
  - A node that uses `EspNowIP` and connects to a gateway.
- `gateway`
  - A node that uses `EspNowIPGateway`, accepts devices, and forwards traffic to the uplink network via `IPv4 NAT`.
- `uplink`
  - The upstream IP network connected to the gateway, including the Internet.
- `gateway service`
  - A local IP service provided by the gateway itself, such as an HTTP API, diagnostic API, or configuration UI.
- `gateway candidate`
  - A gateway that is advertising on `ESP-NOW` and is a possible target for establishing an `EspNowBus` session from a device.
- `active gateway`
  - The gateway that currently has an active `EspNowIP` `IP session` on the device.
- `Bus session`
  - A lower-layer connection established by `EspNowBus`.
- `IP session`
  - A higher-layer logical link established by `EspNowIP`.
- `link context`
  - The context holding `IP session`, lease, and send/receive state between one device and one gateway.
- `control plane`
  - Control traffic used for lease, MTU, keepalive, and connection-state management.
- `data plane`
  - Traffic carrying IPv4 packet payloads.

## 2. Purpose
`EspNowIP` is a layer that carries `IP` traffic over `ESP-NOW` using `EspNowBus` as its internal transport.  
On the device side, it provides a virtual `NetIF` visible to `esp_netif` / `lwIP`; on the gateway side, it forwards that virtual link to the uplink-side IP network via `IPv4 NAT`.

This specification assumes the following:

- Reuse as much of the existing `EspNowBus` functionality as possible
- The device side should use existing IP APIs such as `socket`, `HTTP`, and `MQTT` as-is
- The gateway side accepts multiple devices and forwards them to the uplink via `IPv4 NAT`
- The gateway side uses `routing + IPv4 NAT`

The goals of this specification are:

- Reach an uplink IP network over `ESP-NOW` even on devices that cannot easily use normal Wi-Fi or in noisy environments
- Allow applications to use a network interface that feels close to a normal IP interface, even if throughput is low
- Support ordinary IP-based applications in use cases that tolerate reconnect times on the order of seconds to tens of seconds

Primary target use cases are:

- Reconnect-tolerant client traffic such as MQTT
- Data collection and uploads on intervals of tens of seconds to minutes
- Configuration, diagnostics, and maintenance traffic via gateway services

This specification does not primarily target:

- Real-time control with continuously low-latency requirements
- Continuous streaming
- Traffic that cannot tolerate outages of even a few seconds

## 3. Basic Model

### 3.1 Layer Separation
- `EspNowBus`
  - Discovering gateway candidates
  - JOIN / re-JOIN
  - Managing destination MAC addresses
  - ESP-NOW send/receive
  - Serialized unicast transmission
  - App-ACK-based retry
  - Duplicate suppression
  - Heartbeat and disconnect detection
- `EspNowIP`
  - Trying an `IP session` on each `Bus session`
  - Per-device IP link management
  - Providing an `esp_netif` custom I/O driver
  - IP packet fragmentation and reassembly
  - Address lease management
  - Control plane to the gateway
- `esp_netif` / `lwIP`
  - IPv4 processing
  - Socket API
  - Routing
  - Higher-level network functions such as DNS
- `EspNowIPGateway`
  - Accepting device-side logical links
  - Forwarding to the uplink-side interface
  - Executing `IPv4 NAT`

### 3.2 Topology
- The gateway advertises using `EspNowBus`
- The device receives the gateway advertisement and establishes an `EspNowBus` connection
- A gateway can accept multiple devices on the `ESP-NOW` side
- A device can hold multiple `Bus sessions`
- The device tries whether it can establish an `IP session` on each `Bus session` in sequence
- The device keeps only one successful `IP session` active
- A device may reach only the gateway itself and the gateway's uplink-side network
- The gateway does not forward IPv4 traffic between devices

### 3.3 Data Plane and Control Plane
- Data plane
  - Carry IPv4 packets in `ESP-NOW` unicast payloads
- Control plane
  - Address lease
  - MTU notification
  - Keepalive / link-state updates
  - Gateway presence confirmation
  - Reachability to gateway services

### 3.4 Out of Scope
The following are out of scope:

- IPv6
- Mesh / multi-hop / routing protocols
- Transparent Ethernet bridging
- Forwarding raw 802.11 / Ethernet L2 frames
- DHCP passthrough
- Broadcast / multicast forwarding
- STP / LLDP / mDNS reflection / SSDP relay
- A general-purpose VPN-like implementation over `ESP-NOW`

## 4. Design Assumptions

### 4.1 Assumptions
- `ESP-NOW` is connectionless and requires separate application-layer delivery guarantees
- `ESP-NOW` send and receive callbacks run on Wi-Fi tasks, so heavy processing must be deferred to lower-priority task context
- `ESP-NOW` is assumed to support up to 1470 bytes per packet
- Older `ESP-NOW` environments limited to 250 bytes are discouraged
- An `esp_netif` custom I/O driver must connect `transmit`, `driver_free_rx_buffer`, and `esp_netif_receive()`
- Packet processing in `lwIP` is handled through `esp_netif`

### 4.2 Chosen Architecture
- The logical link is an `L3 point-to-point IPv4 link`
- The gateway uplink side uses only `routing + IPv4 NAT`
- `L2 transparent bridge` is out of scope

Reasoning:
- `ESP-NOW` is not a shared Ethernet-like L2 and has payload-length and broadcast constraints
- The device-side requirement is IP reachability, not full Ethernet segment reproduction
- `esp_netif` / `lwIP` map more naturally to `routing + NAT`

## 5. Logical Link Model

### 5.1 Device Side
- `EspNowIP` provides an `esp_netif` custom I/O driver
- From `esp_netif`, `EspNowIP` appears as a virtual interface with packet I/O
- The interface type is point-to-point IPv4, not Ethernet-compatible
- `EspNowIP` attempts to establish an `IP session` on already-established `Bus sessions`
- The effective peer node is only the `active gateway`

### 5.2 Gateway Side
- The gateway holds one logical link context per device
- Each context keeps:
  - Device MAC
  - Link state
  - IPv4 address leased to the device
  - Fragment reassembly state
  - NAT forwarding state

### 5.3 `Bus session` and `IP session`
- A `Bus session` is a lower-layer connection established by `EspNowBus`
- An `IP session` is a higher-layer connection that becomes usable after `EspNowIP` control plane succeeds
- A `Bus session` may exist even if no `IP session` has been established yet
- `EspNowIP` may hold multiple candidate `Bus sessions`
- `EspNowIP` activates the first successful `IP session`

### 5.4 Connection Sequence
1. A gateway advertises using `EspNowBus`
2. A device receives the advertisement and establishes an `EspNowBus` connection
3. One or more `Bus sessions` are created on the device
4. `EspNowIP` tries to establish `IpControlHello` and `IpControlLease` on each `Bus session`
5. The first successful `Bus session` is used to establish the `IP session`
6. The successful gateway becomes the `active gateway`
7. `Bus sessions` that fail `IP session` establishment remain as candidates, and the next `Bus session` is tried

Gateway redundancy policy:
- When redundancy is used, deploy two gateways and let the device try the above sequence on available `Bus sessions`
- If the `active gateway` is lost, the device attempts reconnection using another `Bus session`
- Recovery of uplink connectivity should target roughly within one minute
- During reconnection, temporary uplink outage is expected
- This behavior is not suitable for highly real-time traffic; it targets reconnect-tolerant use cases

### 5.5 Link Up / Down
- A completed `EspNowBus` JOIN is treated as `Bus session` established
- Completing `IpControlHello` and `IpControlLease` is treated as `IP session` link up
- An `IP session` becomes link down when the `Bus session` corresponding to the `active gateway` is removed by `EspNowBus` auto-purge
- `EspNowBus` auto-purge is defined by timeout departure or `ControlLeave` reception notified through `onJoinEvent(mac, false, false)`
- When the `active gateway` is lost, the device discards all current `IP session` state
- Discarded state includes lease, incomplete fragments, active-gateway selection state, and `esp_netif` link state
- After discarding state, the device returns to the initial connection sequence and retries `IP session` establishment across available `Bus sessions`

## 6. Address Model

### 6.1 Policy
- IPv4 only
- The device obtains addresses through a gateway-specific lease protocol
- DHCP is not used
- The subnet is fixed as `/24`
- Normal deployments are expected to work without changing address settings
- If uplink-side network conflicts must be avoided, only the upper three octets may be changed in gateway configuration

### 6.2 Lease Contents
- Device IPv4 address
- Gateway IPv4 address
- Netmask
- Default gateway
- DNS server 1
- DNS server 2
- Link MTU
- Lease time

### 6.3 Address Layout
- The default subnet is `10.201.0.0/24`
- The default gateway address is `10.201.0.1`
- Device addresses are uniquely leased by the gateway from the same `/24`
- Host-part allocation policy is fixed on the gateway side; there is no per-device address configuration
- When changing the subnet, only the upper three octets are configurable, for example `10.a.b.0/24`
- Normal deployments should use the default subnet and only change it when the uplink-side network conflicts

## 7. IP Protocol

### 7.1 Upper Header
An IP-specific upper-layer header is placed at the beginning of the Bus user payload.

```cpp
struct EspNowBusAppHeader {
    uint8_t protocolId;   // 0x02 = IP
    uint8_t protocolVer;  // 1
    uint8_t packetType;
    uint8_t flags;
};
```

### 7.2 Packet Types

```cpp
enum EspNowIpPacketType : uint8_t {
    IpControlHello = 1,
    IpControlLease = 2,
    IpControlKeepAlive = 3,
    IpData = 4,
};
```

### 7.3 `IpControlHello`
Purpose:
- Protocol version check
- Gateway presence confirmation
- MTU / reassembly capability notification

```cpp
struct IpControlHello {
    uint16_t maxReassemblyBytes;
    uint16_t mtu;
    uint8_t  capabilityFlags;
    uint8_t  reserved;
};
```

### 7.4 `IpControlLease`
Purpose:
- Notify IPv4 link information from gateway to device

```cpp
struct IpControlLease {
    uint32_t deviceIpv4;
    uint32_t gatewayIpv4;
    uint32_t netmaskIpv4;
    uint32_t dns1Ipv4;
    uint32_t dns2Ipv4;
    uint16_t mtu;
    uint16_t leaseSeconds;
};
```

### 7.5 `IpData`
Purpose:
- Carry IPv4 packet bodies

```cpp
struct IpDataHeader {
    uint16_t packetId;
    uint16_t fragmentOffset;
    uint16_t totalLength;
    uint8_t  fragmentIndex;
    uint8_t  fragmentCount;
    uint8_t  reserved0;
    uint8_t  reserved1;
};
```

- `packetId` identifies a reassembly unit
- `fragmentOffset` is the byte offset within the original IP packet
- `totalLength` is the original full IP packet length
- `fragmentCount` is the total number of fragments for one packet

## 8. Data Model

### 8.1 Netif Abstraction
- The user-visible object is an `esp_netif` capable of sending and receiving IP packets
- Packet boundaries are preserved
- A stream abstraction is not used

### 8.2 Fragmentation
- The amount of raw IP data that fits in one Bus payload is:

```text
ipPayloadMax =
  EspNowBus.Config.maxPayloadBytes
  - EspNowBus header (6 bytes)
  - EspNowBusAppHeader (4 bytes)
  - IpDataHeader (10 bytes)
```

- Under current assumptions, `ipPayloadMax = Config.maxPayloadBytes - 20`
- `EspNowIP` fragments IPv4 packets received from `esp_netif` / `lwIP` into `ipPayloadMax` units for transmission
- The receiver reassembles by `packetId` and passes the completed packet to `esp_netif_receive()`
- Older `ESP-NOW` environments limited to 250 bytes would greatly increase fragment count and are effectively out of scope for this specification

### 8.3 MTU
- The default MTU is `1420 bytes`
- Gateway and device operate on the assumption of `1420 bytes`
- MTU negotiation is not performed
- The underlying `ESP-NOW` transport assumes the 1470-byte limit
- Normal operation should aim to avoid IP fragmentation as much as possible
- IPv4 packets larger than `1420 bytes` are fragmented into `IpData` fragments for transmission

### 8.4 Ordering Guarantees
- Under `EspNowBus`'s `1 in-flight / app-ACK / retry` model, large fragment reordering is not expected within the same device-gateway path
- `EspNowIP` keeps a fragment reassembly buffer and accepts out-of-order fragments
- Packets exceeding reassembly timeout are discarded

## 9. `esp_netif` Integration

### 9.1 Custom I/O Driver
- `EspNowIP` implements an `esp_netif` custom I/O driver
- The driver side connects:
  - `transmit`
  - `driver_free_rx_buffer`
  - `post_attach`

### 9.2 Transmit Path
- `transmit()` is called from `lwIP` / `esp_netif`
- `EspNowIP` fragments the IPv4 packet if needed and sends it to the gateway using `EspNowBus.sendTo()`
- Callbacks do not perform heavy work; necessary state transitions are handled in internal queue / task context

### 9.3 Receive Path
- `IpData` is received in the `EspNowBus` receive callback
- Fragments are reassembled if needed
- Completed packets are passed to `esp_netif_receive()`

### 9.4 Netif State
- Until lease acquisition completes, the interface is treated as carrier-down equivalent
- Completing `IpControlHello` and `IpControlLease` brings the link up
- If the `Bus session` corresponding to the `active gateway` is removed by `EspNowBus` auto-purge, the link becomes down
- Link-down is reflected to `esp_netif`

## 10. Gateway Forwarding

### 10.1 Forwarding Mode
- The gateway performs only `routing + IPv4 NAT`
- `routing only` is not used
- `L2 bridge` is not used

### 10.2 NAT
- The gateway enables `IPv4 NAPT` on the uplink interface
- In `lwIP`, `IP forwarding` and `IPv4 NAPT` are enabled
- Inbound port forwarding is not used

### 10.3 Gateway Responsibilities
- Act as the default gateway from the device's point of view
- Distribute uplink-side DNS servers to devices
- Expose its own gateway-service endpoint
- Keep local link state even if uplink connectivity is absent

### 10.4 Broadcast / Multicast
- IPv4 broadcast is not forwarded
- ARP stays local to the link under the point-to-point model
- Multicast is not forwarded

## 11. Buffering

### 11.1 TX
- Keep one in-progress packet context per device
- Limit send queue length
- Send fragments of MTU-exceeding packets sequentially per packet

### 11.2 RX
- Keep one fragment reassembly buffer per device
- Limit the number of packets being reassembled simultaneously
- Drop packets as whole units on timeout / overflow

## 12. Execution Model

### 12.1 `poll()`
- Call `EspNowIP::poll()` from `loop()`
- `poll()` is non-blocking
- `poll()` advances:
  - Gateway/device state machine
  - Reassembly-timeout cleanup
  - Lease updates
  - Pending TX flush

### 12.2 Automatic Auxiliary Poll
- Explicit `poll()` is the primary path
- `EspNowIP` state-query APIs and data-operation APIs may call a lightweight internal poll when needed
- Lightweight poll is limited to:
  - Confirming loss of the `active gateway`
  - Lease / timeout checks
  - Advancing one step of the connection sequence
- Heavy work such as large reassembly cleanup or repeatedly trying multiple candidates belongs in normal `poll()`
- Automatic poll is an auxiliary mechanism and does not replace explicit `poll()`

### 12.3 Task Separation
- `ESP-NOW` callbacks stop at queue insertion
- `esp_netif_receive()` calls and lease/state updates run in normal task context

## 13. API Specification

### 13.1 Device Side

```cpp
class EspNowIP {
public:
    struct Config {
        const char* groupName = nullptr;
        const char* ifKey = "enip0";
        const char* ifDesc = "ESP-NOW IP";
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
    };

    bool begin(const Config& cfg);
    void end();
    void poll();

    esp_netif_t* netif();
    bool linkUp() const;
    bool hasLease() const;
};
```

### 13.2 Gateway Side

```cpp
class EspNowIPGateway {
public:
    struct Config {
        const char* groupName = nullptr;
        esp_netif_t* uplink = nullptr;
        uint16_t mtu = 1420;
        uint8_t maxDevices = 6;
        uint32_t leaseSeconds = 3600;
        uint8_t subnetOctet1 = 10;
        uint8_t subnetOctet2 = 201;
        uint8_t subnetOctet3 = 0;
    };

    bool begin(const Config& cfg);
    void end();
    void poll();
};
```

## 14. Responsibility Boundary with `EspNowBus`

### 14.1 What `EspNowIP` Does Not Reimplement
- Gateway-candidate discovery
- JOIN
- Encryption
- App-ACK
- Packet retry
- Physical send-failure detection
- Heartbeat

### 14.2 What `EspNowIP` Adds
- `esp_netif` custom driver
- IPv4 packet fragmentation / reassembly
- Lease / MTU / keepalive control plane
- Gateway-side NAT forwarding context

## 15. Implementation Scope
- IPv4 only
- Device-side `esp_netif` custom I/O driver
- Gateway-side `routing + IPv4 NAT`
- Proprietary lease protocol
- Unicast only
- Single active gateway assumption

## 16. References
- ESP-NETIF Developer's manual  
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif_driver.html
- ESP-IDF Programming Guide: ESP-NOW  
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
- ESP-IDF Programming Guide: lwIP  
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-guides/lwip.html
- ESP-IDF Project Configuration: `CONFIG_LWIP_IP_FORWARD` / `CONFIG_LWIP_IPV4_NAPT`  
  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s2/api-reference/kconfig.html
