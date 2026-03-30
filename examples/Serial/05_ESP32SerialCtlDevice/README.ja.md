# 05_ESP32SerialCtlDevice

このサンプルは、外部ライブラリ `ESP32SerialCtl` の CLI を Serial over EspNow 経由で公開します。

組み合わせて使うスケッチは以下です。

- controller 側: `examples/Serial/02_ControllerBridge`
- device 側: `examples/Serial/05_ESP32SerialCtlDevice`

## 依存ライブラリ

このサンプルは外部ライブラリ `ESP32SerialCtl` が必要です。

- Repository: `https://github.com/tanakamasayuki/ESP32SerialCtl`
- WebSerial コンソール: `https://tanakamasayuki.github.io/ESP32SerialCtl/`

同梱の `sketch.yaml` では、以下のライブラリ設定を前提にしています。

```yaml
libraries:
  - ESP32SerialCtl (1.0.2)
```

このスケッチをコンパイルする前に `ESP32SerialCtl` をインストールしてください。

## 書き込み先と役割

- PC に USB 接続する ESP32 には `02_ControllerBridge` を書き込みます。
- リモート側の ESP32 / ESP32-S3 には `05_ESP32SerialCtlDevice` を書き込みます。
- PC からは controller 側デバイスの USB シリアルに接続します。

controller 側スケッチは、USB Serial と選択中の EspNowSerial session を透過的に bridge します。

## 基本的な流れ

1. 両方のデバイスを起動し、ESP-NOW でペアリングされるのを待ちます。
2. PC は remote device ではなく controller device に接続します。
3. 必要なら controller のシリアルに `///list` を送って session 一覧を確認します。
4. 必要なら `///use 0` などで使用する session を選択します。

`02_ControllerBridge` は session `0` が利用可能になった時点で自動選択するため、2 台構成では通常 `///use 0` を明示的に送る必要はありません。

session が選択されると、通常のシリアル入出力はそのまま転送されます。

- PC から controller の USB Serial
- controller から remote device 上の `ESP32SerialCtl`
- `ESP32SerialCtl` の応答が EspNowSerial 経由で controller に戻る
- controller から PC に返る

## WebSerial の使い方

`ESP32SerialCtl` プロジェクトが提供している WebSerial コンソールを利用できます。

- `https://tanakamasayuki.github.io/ESP32SerialCtl/`

ブラウザから `02_ControllerBridge` が動いている controller 側デバイスの USB シリアルに接続してください。

接続後は、WebSerial UI から送ったコマンドが bridge を通って remote device 上の `ESP32SerialCtl` に届きます。`help`、`sys info` などの `ESP32SerialCtl` コマンドをブラウザからそのまま利用できます。

## 備考

- controller 側のローカルコマンドは `///` プレフィックス付きの行だけです。
- 例:
  - `///list`
  - `///use 0`
  - `///help`
- `///` で始まらない行は、選択中の remote session にそのまま転送されます。
