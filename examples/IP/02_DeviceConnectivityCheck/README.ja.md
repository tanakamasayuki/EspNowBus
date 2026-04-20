# 02_DeviceConnectivityCheck

`EspNowIP` の疎通診断をまとめて行う device 側サンプルです。

## このサンプルで確認できること

- `gateway` への ICMP ping
- DNS 名前解決
- NTP への UDP 通信
- HTTP GET による TCP 通信

## 想定用途

`01_DeviceBasic` で lease が取れたあとに、実際の IP 通信がどこまで通るかを段階的に確認します。

## 補足

- 宛先はサンプル内の固定値です。
- 実験時はこのサンプルを `examples/IP/temp/` へコピーしてから編集する運用を想定しています。
- `gateway ping` は `10.201.0.1` を使います。
- 外向き確認は `example.com`、`pool.ntp.org`、`http://example.com/` を使います。
- device 側は `cfg.autoJoinIntervalMs = 0` で自分からは募集せず、gateway 側の募集を受けて lease 取得する前提です。
