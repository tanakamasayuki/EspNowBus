# 03_GatewayEthernet

Gateway-side `EspNowIP` example for a wired Ethernet uplink.

The current reference candidate is `LilyGo T-Internet-COM`. This scaffold assumes `ETH.h` with `LAN8720 RMII`.

## What It Shows

- Passing an Ethernet `esp_netif` obtained from `ETH.h` into the gateway uplink
- Reusing the same `routing + IPv4 NAT` gateway model with wired uplink
- A gateway option without the Wi-Fi STA channel constraint

## Intended Use

Use this as a more deployment-friendly gateway example when a wired Ethernet uplink is available.

## Current Direction

- The current reference candidate is `T-Internet-COM`
- The sketch uses Arduino ESP32 `ETH.h` instead of `M5_Ethernet.h`
- The expected path is: bring up `ETH`, obtain `ETH.netif()`, then pass it to `EspNowIPGateway`
