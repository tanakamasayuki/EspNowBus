# Changelog / 変更履歴

## Unreleased

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
