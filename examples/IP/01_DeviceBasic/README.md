# 01_DeviceBasic

Minimal device-side `EspNowIP` example.

## What It Shows

- `EspNowIP` device initialization
- Virtual IP interface bring-up
- Link / lease state observation
- The baseline structure for later device-side examples

## Intended Use

Use this as the first smoke test for device-side `Bus session -> IP session` establishment and lease application.

## Notes

- `EspNowBus` chooses a channel automatically from `groupName` by default. In normal use, keep that default and only force the same channel on every node when the environment requires it.
- When used with `03_GatewayWiFiSTA`, the device-side `EspNowBus` channel must match the STA channel shown by the gateway.
- This example keeps the device side silent with `cfg.autoJoinIntervalMs = 0`, expecting the gateway side to advertise.
- Example (normally left commented out):

```cpp
// EspNowIP::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.autoJoinIntervalMs = 0;      // Device stays silent; gateway advertises
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```
