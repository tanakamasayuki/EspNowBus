# 04_MasterSlave / Slave

`04_MasterSlave` の slave 側スケッチです。

## このスケッチの役割

- `Master` と同じ固定 group 名を使う
- 自分からは募集しない
- `value=<millis>` 形式のデータを既知ピアへ定期送信する

送信側にしたいボードにこのスケッチを書き込み、`Master` と組み合わせて使います。
