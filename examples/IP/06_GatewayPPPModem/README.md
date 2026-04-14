# 06_GatewayPPPModem

Gateway-side `EspNowIP` example using an AT-command cellular modem PPP uplink.

## What It Shows

- A modem uplink flow based on Arduino ESP32 `PPP.h`
- Passing `PPP.netif()` into `EspNowIPGateway`
- A PPP example intended for AT-modem style APN / PIN / UART pin / flow-control setup

## Intended Use

Use this when your uplink is a modem supported by `PPP.h`, such as SIM7600, SIM7070, SIM7000, BG96, or SIM800.

## Notes

- This is different from `05_GatewayPPPSerial`, which is for host-PC raw PPP
- `PPP.h` is `esp_modem` oriented, so this sample is for cellular modems that support AT commands and PPP
- Some cellular modems expose HTTP, MQTT, TCP, and similar features through dedicated AT commands, but this example uses PPP instead of those modem-specific command APIs
- Using PPP makes the uplink available as a `NetIF`, so routing / NAT / DNS / socket use can follow the same path as the Wi-Fi STA and Ethernet examples
- The tradeoff is extra framing and control overhead, so PPP can be less efficient than using a modem's direct AT-command data features
- Adjust `PPP_MODEM_APN`, `PPP_MODEM_PIN`, UART pins, flow control, and modem model for your hardware
- Once `PPP.netif()` is available, the NAT / routing path is the same as the Wi-Fi STA and Ethernet uplink examples
