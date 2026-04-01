# IP Examples

`EspNowIP` example set.

## Examples

- [`01_DeviceBasic`](01_DeviceBasic): Minimal device-side `EspNowIP` setup. Brings up the virtual IP interface, prints link / lease state, and acts as the base for later device examples.
- [`02_GatewayWiFiSTA`](02_GatewayWiFiSTA): Gateway-side example using Wi-Fi STA as the uplink. Intended mainly for bring-up and testing.
- [`03_GatewayW5500Ethernet`](03_GatewayW5500Ethernet): Gateway-side example using W5500 Ethernet as the uplink. Intended as a more deployment-friendly wired uplink example.
- [`04_GatewayPPPSerial`](04_GatewayPPPSerial): Gateway-side example using a physical UART PPP link to a PC as the uplink. Intended as an alternative to Wi-Fi uplink when the host is connected over USB serial.
- [`05_DeviceConnectivityCheck`](05_DeviceConnectivityCheck): Device-side connectivity diagnostics covering `gateway ping`, DNS, NTP, and HTTP.

## Notes

- These examples build today, but the `EspNowIP` data plane and uplink NAT are still only minimally implemented.
- Device-side examples assume the gateway advertises through `EspNowBus`, then `EspNowIP` tries `IP session` establishment on the resulting `Bus session`.

## Choosing an Uplink

- Gateway uplinks are selected on the assumption that they can be exposed as an `esp_netif`.
- The initial set is limited to these three:
  - `Wi-Fi STA`: easy to test, but not ideal for production because the effective `ESP-NOW` channel is constrained by the STA side.
  - `W5500 / Ethernet`: a strong practical option for real deployments.
  - `UART PPP`: easy to try with common lab hardware, but requires PPP peer setup on the host side.
- Cellular uplinks may become candidates if they can be integrated as an `esp_netif` via something like `esp_modem`, but that is considered too high-friction for the initial implementation.
