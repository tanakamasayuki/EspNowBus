# Changelog / 変更履歴

## Unreleased
- (EN) Added `EspNowIP` and `EspNowIPGateway` as an IPv4-over-ESP-NOW layer on top of `EspNowBus`, including `esp_netif` integration, a minimal `Hello` / `Lease` control plane, `IpData` transport, lease application on the device side, and gateway-side IPv4 NAT over an uplink `esp_netif`
- (JA) `EspNowBus` 上の IPv4-over-ESP-NOW 層として `EspNowIP` と `EspNowIPGateway` を追加し、`esp_netif` 連携、最小の `Hello` / `Lease` control plane、`IpData` transport、device 側 lease 適用、gateway 側 uplink `esp_netif` に対する IPv4 NAT を実装
- (EN) Added a raw `lwIP PPPoS` helper to the PPP gateway example so an already-initialized `Stream` can be used to build a PPP `esp_netif` without making PPPoS part of the core library API
- (JA) PPP gateway サンプル側に raw `lwIP PPPoS` ヘルパーを追加し、初期化済み `Stream` から PPP `esp_netif` を構築できるようにしつつ、PPPoS をライブラリ本体 API には含めない構成にした
- (EN) Added and validated `examples/IP`, including device bring-up, Wi-Fi STA gateway, Ethernet gateway, PPP gateway scaffold, and a connectivity check example that verifies gateway ping, DNS, internet ping, NTP, and HTTP
- (JA) `examples/IP` を追加し、device bring-up、Wi-Fi STA gateway、Ethernet gateway、PPP gateway scaffold、gateway ping / DNS / internet ping / NTP / HTTP を確認する connectivity check サンプルを整備
- (EN) Added IP documentation: `SPEC.ip.ja.md`, `SPEC.ip.md`, related README links, and example READMEs for the new IP examples
- (JA) `SPEC.ip.ja.md`、`SPEC.ip.md`、README からの導線、各 IP example の README など、IP 向けドキュメントを追加
- (EN) Improved `EspNowBus` channel selection so a connected Wi-Fi STA channel becomes the effective ESP-NOW channel, even when auto-channel or an explicit channel was configured, and added clearer configured/effective channel logs
- (JA) `EspNowBus` の channel 選択を改善し、Wi-Fi STA 接続中は auto 設定時・明示設定時を問わず接続中 AP の channel を実効値として採用するよう変更し、設定値と実効値が分かるログを追加

## 1.1.0
- (EN) Added `EspNowSerial` and `EspNowSerialPort` as a Serial over ESP-NOW layer built on top of `EspNowBus`, with `Stream` / `Print` compatible APIs, fixed session slots, stable session indices, and session binding helpers such as `bind(mac)`, `bindSession(index)`, and `bindFirstAvailable()`
- (JA) `EspNowBus` 上に Serial over ESP-NOW 層として `EspNowSerial` と `EspNowSerialPort` を追加し、`Stream` / `Print` 互換 API、固定 session slot、stable な session index、`bind(mac)` / `bindSession(index)` / `bindFirstAvailable()` などの session bind API を実装
- (EN) Added Serial examples under `examples/Serial`, including basic pairing, controller bridge, silent device, multi-session monitor, and an `ESP32SerialCtl` integration example
- (JA) `examples/Serial` に Serial 系サンプルを追加し、基本接続、controller bridge、silent device、multi-session monitor、`ESP32SerialCtl` 連携例を収録
- (EN) Added Serial-specific documentation, including `SPEC.serial.ja.md`, example README / README.ja files, and root README links for `EspNowSerial`
- (JA) `SPEC.serial.ja.md`、各 example の README / README.ja、ルート README からの導線など、`EspNowSerial` 向けドキュメントを追加
- (EN) `peerCount()`, `getPeer()`, and `hasPeer()` now expose only ready/sendable peers instead of peers merely discovered by receive-side tracking
- (JA) `peerCount()`, `getPeer()`, `hasPeer()` は受信で見えただけのpeerではなく、送信可能なready peerのみを返すように変更
- (EN) Fixed peer registration when `useEncryption=false` so unicast peers are still added to the ESP-NOW peer list
- (JA) `useEncryption=false` 時でもユニキャストpeerをESP-NOW peer listへ追加するよう修正し、暗号化OFF時の送信失敗を改善
- (EN) Reduced `-Wunused-variable` warnings in non-debug builds by suppressing log-only variable warnings in `EspNowBus.cpp`
- (JA) 非デバッグビルドで `EspNowBus.cpp` のログ専用変数に起因する `-Wunused-variable` 警告を抑制
- (EN) Improved timeout debug logs to show `default` / `forever` / `Nms` instead of raw special timeout values
- (JA) timeout のデバッグログ表示を改善し、特別値を生の数値ではなく `default` / `forever` / `Nms` で表示するよう変更

## 1.0.1
- (EN) Release workflow now rebuilds the release branch and tags it so rewritten sketch.yaml files are part of the tagged release contents
- (JA) リリースワークフローでreleaseブランチを作り直し、書き換え済みsketch.yamlをタグの内容に含めるように変更

## 1.0.0
- (EN) Updated release scripts
- (JA) リリーススクリプト更新
- (EN) Added changelog
- (JA) チェンジログ追加
- (EN) Added `end(stopWiFi, sendLeave)` to allow explicit leave and WiFi stop [PR #2](https://github.com/tanakamasayuki/EspNowBus/pull/2) and [PR #3](https://github.com/tanakamasayuki/EspNowBus/pull/3)
- (JA) `end(stopWiFi, sendLeave)`を追加して、明示的離脱とWiFi停止を可能にしました [PR #2](https://github.com/tanakamasayuki/EspNowBus/pull/2) and [PR #3](https://github.com/tanakamasayuki/EspNowBus/pull/3)
