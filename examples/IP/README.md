# IP Examples

`EspNowIP` example set.

## Examples

- [`01_DeviceBasic`](01_DeviceBasic): Minimal device-side `EspNowIP` setup. Brings up the virtual IP interface, prints link / lease state, and acts as the base for later device examples.
- [`02_DeviceConnectivityCheck`](02_DeviceConnectivityCheck): Device-side connectivity diagnostics covering `gateway ping`, DNS, NTP, and HTTP.
- [`03_GatewayWiFiSTA`](03_GatewayWiFiSTA): Gateway-side example using Wi-Fi STA as the uplink. Intended mainly for testing and validation.
- [`04_GatewayEthernet`](04_GatewayEthernet): Gateway-side example using wired Ethernet as the uplink. The current reference candidate is `LilyGo T-Internet-COM`.
- [`05_GatewayPPPSerial`](05_GatewayPPPSerial): Gateway-side example using a physical UART PPP link to a PC as the uplink. Intended as an alternative to Wi-Fi uplink when the host is connected over USB serial.
- [`06_GatewayPPPModem`](06_GatewayPPPModem): Gateway-side example using `PPP.h` for a cellular modem PPP uplink.

## Notes

- These examples build today, and the Wi-Fi STA and Ethernet uplink paths are validated as PoC implementations.
- Device-side examples assume the gateway advertises through `EspNowBus`, then `EspNowIP` tries `IP session` establishment on the resulting `Bus session`.

## How IP Is Realized

- `EspNowIP` is an IP layer built on top of `EspNowBus`. On the device side it behaves as a virtual `NetIF` implemented through an `esp_netif` custom I/O driver.
- From the device-side `lwIP` point of view, `EspNowIP` looks like a normal network interface, so ARP, ICMP, UDP, TCP, and similar packets can be sent and received through it.
- A device first creates an `EspNowBus` `Bus session`, then establishes an `IP session` on top of it through `Hello` / `Lease`. Once the lease is applied, the virtual device-side `NetIF` receives its IPv4 address, gateway, and DNS settings.
- On the gateway side, `EspNowIPGateway` also owns a bus-side `esp_netif` and feeds incoming `IpData` packets into that interface. From the device's point of view, it is sending traffic to a gateway such as `10.x.x.1`.
- The gateway also has an uplink-side `esp_netif`. If the uplink can be exposed as `esp_netif` (Wi-Fi STA, Ethernet, PPP, and similar cases), the bus side and uplink side can be connected inside the same ESP32 IP stack.
- That allows the gateway to treat the bus-side interface as the inside interface and the uplink as the outside interface for `routing + NAT`. `EspNowIP` works because it does not try to extend ESP-NOW as a transparent Ethernet bridge; instead it creates a virtual `NetIF` and uses the ESP32 as an L3 router.
- In other words, this is an `L3 gateway`, not an `L2 bridge`. ARP and ICMP between the device and gateway terminate locally, while outbound traffic is routed toward the uplink and NATed when needed.
- When the uplink is integrated through `esp_netif`, socket, DNS, routing, and NAT handling can share the same path. The tradeoff is extra copies and control overhead because packets pass through the ESP32-side `lwIP` stack.
- By contrast, using a modem or communication chip's own built-in IP stack can reduce ESP32 load, but those chip-local stacks are often not exposed as `esp_netif`, which makes them a poor fit for an `EspNowIPGateway` design that wants to connect multiple interfaces through a common IP stack.

## Constraints and Operational Notes

- The `EspNowIP` topology is a star: `device -> gateway -> uplink`. The gateway advertises through `EspNowBus`, and the device establishes an `IP session` on top of the resulting `Bus session`.
- A device can normally reach only the gateway itself and the network behind the gateway uplink. Device-to-device IP forwarding is not provided.
- Multiple gateways can exist in the same environment, but the selected gateway is not fixed. The device tries to establish an `IP session` on available `Bus session` candidates and uses the first one that succeeds.
- When connectivity to the active gateway is lost, the `EspNowBus` timeout / auto-purge path tears down the current `IP session`, and the device reconnects using whatever gateway candidates are available at that time.
- This means multiple gateways can provide redundancy, but the reconnect target is not guaranteed. It is a disconnect-and-reconnect model, not a seamless failover model.
- `EspNowBus` already performs retransmission and app-ack handling, and `TCP` adds its own handshake and retransmission behavior on top. As a result, `TCP` carries noticeable overhead. This is acceptable for low-rate reconnect-friendly traffic such as `HTTP` or `MQTT`, but it is not a good fit for low-latency or throughput-sensitive workloads.
- Heavy debug logging, especially over a serial console, reduces practical throughput significantly. The example logging is tuned for debugging and is not intended as a production default.
- The current implementation is primarily a configuration example and PoC. Basic Wi-Fi STA and Ethernet uplink paths are validated, but throughput tuning, long-duration operation, detailed failover behavior, and full fragmentation / reassembly validation are still limited.
- In practice, treat the current examples and default settings as proof that the architecture works, then adjust logging, reconnect behavior, and traffic expectations before using the design in a real deployment.

## Choosing an Uplink

- Gateway uplinks are selected on the assumption that they can be exposed as an `esp_netif`.
- When an uplink is exposed as `esp_netif`, it can be connected to the `EspNowIPGateway` bus-side `NetIF` inside the same ESP32 IP stack.
- That keeps the gateway-side `routing + NAT` design the same even when the uplink type changes.
- The current set covers these four:
- `Wi-Fi STA`: easy to test, but not ideal for production because the effective `ESP-NOW` channel is constrained by the STA side.
- `Ethernet`: a strong practical option for real deployments.
- `UART PPP`: easy to try with common lab hardware, but requires PPP peer setup on the host side.
- `PPP.h` compatible cellular modem: see `06_GatewayPPPModem`.
