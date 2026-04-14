# 03_GatewayWiFiSTA

Gateway-side `EspNowIP` example using Wi-Fi STA as the uplink.

## What It Shows

- `EspNowIPGateway` initialization
- Wi-Fi STA uplink preparation
- The baseline `routing + IPv4 NAT` gateway structure
- Printing the STA channel used by the uplink

## Limitations

- This example is easy to test but is not the best production-oriented uplink option.
- `EspNowBus` normally derives its channel automatically from `groupName`. In normal use, keep that default, but in this example the effective channel is constrained by the STA uplink, so every node may need the same channel set explicitly.
- With `ESP-NOW` + `Wi-Fi STA`, the effective ESP-NOW channel is constrained by the connected STA channel.
- If the AP channel changes, roaming occurs, or multiple APs use different channels, the ESP-NOW side may stop linking correctly until the device side is aligned again.

## Intended Use

Use this as a development and validation example for the Wi-Fi STA uplink path.

## Test Procedure

1. Configure the gateway with your Wi-Fi STA SSID and password.
2. Boot the gateway and wait for the serial log to print the STA channel.
3. Configure the device-side `EspNowBus` channel to the same channel shown by the gateway.
4. Start the device-side example and confirm that `Bus session` and then `IP session` can be established.

Example (normally left commented out):

```cpp
// EspNowIPGateway::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```
