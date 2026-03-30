# 07_AutoPurge

JOIN と heartbeat 関連イベントを確認するサンプルです。

## このサンプルで確認できること

- `onJoinEvent(...)`
- heartbeat によるピア状態更新
- 既知ピア先頭への `ping` 送信

## 使い方

同じスケッチを 2 台に書き込み、シリアルモニタで join / leave / timeout 関連の表示を確認します。
