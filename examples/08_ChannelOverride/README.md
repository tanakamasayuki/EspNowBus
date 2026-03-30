# 08_ChannelOverride

Example that explicitly sets the Wi-Fi channel.

## What It Shows

- `cfg.channel`
- Reporting the actual Wi-Fi channel with `esp_wifi_get_channel`
- Periodic broadcast on the selected channel

## How to Use

Flash the same sketch to two or more boards and confirm that they communicate on the configured channel.
