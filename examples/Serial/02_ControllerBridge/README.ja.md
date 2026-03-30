# 02_ControllerBridge

`EspNowSerial` の controller 側透過 USB bridge サンプルです。

## このサンプルの役割

- USB Serial と選択中の `EspNowSerial` session を bridge する
- session `0` が利用可能になると自動選択する
- `///` で始まるローカルコマンドだけ controller 側で処理する

## ローカルコマンド

- `///list`
- `///use <index>`
- `///help`

`///` で始まらない行は、選択中の remote session にそのまま転送されます。
