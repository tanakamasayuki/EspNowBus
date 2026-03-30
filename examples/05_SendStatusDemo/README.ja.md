# 05_SendStatusDemo

ユニキャスト送信時の `SendStatus` を確認するサンプルです。

## このサンプルで確認できること

- `onSendResult(...)`
- `onAppAck(...)`
- 物理送信成功と論理 ACK 成功の違い

## 使い方

1. 同じスケッチを 2 台のボードに書き込みます。
2. シリアルモニタを開きます。
3. `Queued`、`SentOk`、`AppAckReceived`、`AppAckTimeout` などの状態を確認します。

再送回数やタイムアウト調整をするときの確認用として便利です。
