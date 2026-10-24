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

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [05_ESP32SerialCtlDevice.ino](05_ESP32SerialCtlDevice.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<ESP32SerialCtl.h>`, `<EspNowSerial.h>`, `<WiFi.h>`

3. 書き込んだらシリアルモニタを115200 baudへ合わせます。起動ログとソースの操作条件を確認します。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `serialHub.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `controlSerial.attach(...)` — 準備済みの資源へ接続し、その寿命を保ちます。
3. `serialHub.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。
4. `remoteCli.service(...)` — 保留コマンドを定期的に処理します。呼び出し元を止めると次の入力も遅れます。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 操作と出力の見方

出力書式の一部は次のとおりです。printfの書式記号は実行時の値に変わります。表示順や数値は実行条件に応じて変わるので、その行へ進む条件と併せて確認します。

```text
ESP32SerialCtl device ready: %02X:%02X:%02X:%02X:%02X:%02X
Use examples/Serial/02_ControllerBridge on the controller side.
```

## 確認と問題の切り分け

シリアル Command DeviceをBus越しに使うことを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
