# IP Examples

`EspNowIP` example set.

## Examples

- [`01_DeviceBasic`](01_DeviceBasic): Minimal device-side `EspNowIP` setup. Brings up the virtual IP interface, prints link / lease state, and acts as the base for later device examples.
- [`02_DeviceConnectivityCheck`](02_DeviceConnectivityCheck): Device-side connectivity diagnostics covering `gateway ping`, DNS, NTP, and HTTP.
- [`03_GatewayWiFiSTA`](03_GatewayWiFiSTA): Gateway-side example using Wi-Fi STA as the uplink. Intended mainly for testing and validation.
- [`04_GatewayEthernet`](04_GatewayEthernet): Gateway-side example using wired Ethernet as the uplink. The current reference candidate is `LilyGo T-Internet-COM`.
- [`05_GatewayPPPSerial`](05_GatewayPPPSerial): Gateway-side example using a physical UART PPP link to a PC as the uplink. Intended as an alternative to Wi-Fi uplink when the host is connected over USB serial.

## Notes

- These examples build today, and the Wi-Fi STA and Ethernet uplink paths are validated as PoC implementations.
- Device-side examples assume the gateway advertises through `EspNowBus`, then `EspNowIP` tries `IP session` establishment on the resulting `Bus session`.

## Choosing an Uplink

- Gateway uplinks are selected on the assumption that they can be exposed as an `esp_netif`.
- The initial set is limited to these three:
  - `Wi-Fi STA`: easy to test, but not ideal for production because the effective `ESP-NOW` channel is constrained by the STA side.
  - `Ethernet`: a strong practical option for real deployments.
  - `UART PPP`: easy to try with common lab hardware, but requires PPP peer setup on the host side.
- Cellular uplinks may become candidates if they can be integrated as an `esp_netif` via something like `esp_modem`, but that is considered too high-friction for the initial implementation.
