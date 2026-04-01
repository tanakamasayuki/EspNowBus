# 04_GatewayPPPSerial

Gateway-side `EspNowIP` example using physical UART PPP as the uplink.

## What It Shows

- A `lwIP PPPoS + esp_netif PPP` oriented structure
- The gateway-side skeleton for a physical UART raw-PPP uplink
- Reusing the same gateway-side NAT model as other uplink types

## Intended Use

Use this as an alternative gateway example when Wi-Fi uplink is unavailable or intentionally avoided.

## Current Direction

- The target topology is `ESP32 UART <-> host PC PPP peer`
- `PPP.h` is AT-modem oriented, so it is not used for this scenario
- The intended path is to create `ESP_NETIF_DEFAULT_PPP()`, then bind a `ppp_pcb` with `pppapi_pppos_create()`
- UART RX should feed `pppos_input_tcpip()`, and PPP TX should be written from the PPPoS output callback
- Once that raw PPP bring-up exists, the resulting `esp_netif_t*` can be passed into `EspNowIPGateway` just like the validated Wi-Fi STA uplink path
- The sketch uses an `EspNowPPPoS` helper that accepts any already-initialized `Stream`
- Serial initialization is the user's responsibility
- Logging stays off by default and can be enabled by passing a `Print*` logger
- If `pppIo` and `debugOut` point to the same stream, debug logging is disabled automatically to avoid corrupting PPP traffic
