# Changelog / 変更履歴

## Unreleased
- (EN) `peerCount()`, `getPeer()`, and `hasPeer()` now expose only ready/sendable peers instead of peers merely discovered by receive-side tracking
- (JA) `peerCount()`, `getPeer()`, `hasPeer()` は受信で見えただけのpeerではなく、送信可能なready peerのみを返すように変更
- (EN) Fixed peer registration when `useEncryption=false` so unicast peers are still added to the ESP-NOW peer list
- (JA) `useEncryption=false` 時でもユニキャストpeerをESP-NOW peer listへ追加するよう修正し、暗号化OFF時の送信失敗を改善

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
