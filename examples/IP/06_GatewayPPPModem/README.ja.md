# 06_GatewayPPPModem

AT コマンドに対応したセルラーモデムを使う PPP uplink gateway 側 `EspNowIP` サンプルです。

## このサンプルで確認できること

- Arduino ESP32 の `PPP.h` を使うモデム uplink 構成
- `PPP.netif()` を `EspNowIPGateway` の uplink に渡す構成
- APN / PIN / UART pin / flow control / modem model を使う AT モデム前提の PPP 例

## 想定用途

SIM7600、SIM7070、SIM7000、BG96、SIM800 など、`PPP.h` が想定する AT モデムを uplink に使いたい場合に使います。

## 補足

- このサンプルは `05_GatewayPPPSerial` と違って host PC raw PPP ではありません
- `PPP.h` は `esp_modem` 前提なので、AT コマンドと PPP をサポートするセルラーモデム用途向けです
- セルラーモデムには HTTP、MQTT、TCP などを個別の AT コマンドで扱えるものもありますが、このサンプルではそれらの個別 API ではなく PPP を使います
- PPP を使うことで uplink を `NetIF` として扱え、`EspNowIPGateway` の routing / NAT / DNS / socket 利用を Wi-Fi STA や Ethernet と同じ形に揃えられます
- 一方で PPP framing や制御手順のぶんだけオーバーヘッドが増えるため、AT コマンド直叩きより効率面で不利になる場合があります
- `PPP_MODEM_APN`、`PPP_MODEM_PIN`、UART pin、flow control、modem model は実機に合わせて調整してください
- `PPP.netif()` が得られれば、その先の NAT / routing は Wi-Fi STA や Ethernet uplink と同じです

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [06_GatewayPPPModem.ino](06_GatewayPPPModem.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<Arduino.h>`, `<EspNowIP.h>`, `<PPP.h>`

3. 書き込んだらシリアルモニタを115200 baudへ合わせます。起動ログとソースの操作条件を確認します。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `PPP.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `gateway.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 操作と出力の見方

出力書式の一部は次のとおりです。printfの書式記号は実行時の値に変わります。表示順や数値は実行条件に応じて変わるので、その行へ進む条件と併せて確認します。

```text
PPP Started
PPP Connected
PPP Got IP: %s gw=%s dns=%s
```

## 確認と問題の切り分け

PPP ModemでGatewayを接続することを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
