# 03_SilentDevice

controller 側の募集にだけ接続する device 側サンプルです。

## このサンプルの役割

- 自分からは募集しない
- 最初に使える controller 側 session に bind する
- 定期的に短い状態行を送る
- 受信した byte を controller 側へエコーバックする

## 典型的な組み合わせ

controller 側に `02_ControllerBridge`、device 側にこの `03_SilentDevice` を使います。
