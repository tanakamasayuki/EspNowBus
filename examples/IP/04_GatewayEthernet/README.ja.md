# 04_GatewayEthernet

有線 Ethernet を uplink に使う gateway 側 `EspNowIP` サンプルです。

現時点の標準機候補は `LilyGo T-Internet-COM` です。`ETH.h` と `LAN8720 RMII` 構成を前提にしています。

## このサンプルで確認できること

- `ETH.h` で取得した Ethernet `esp_netif` を gateway uplink に渡す構成
- 有線 uplink でも同じ `routing + IPv4 NAT` モデルを使う構成
- `Wi-Fi STA` よりチャネル制約を避けやすい gateway 例

## 想定用途

有線 Ethernet uplink を使える環境で、より実運用向きの gateway サンプルとして使います。

## 補足

- 現行の標準機候補は `T-Internet-COM` です
- `T-Internet-COM` では `M5_Ethernet.h` ではなく Arduino ESP32 標準の `ETH.h` を使い、`ETH.netif()` を `EspNowIPGateway` に渡す方針です
- `02_DeviceConnectivityCheck` と組み合わせることで、Ethernet uplink 経路の疎通確認ができます

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [04_GatewayEthernet.ino](04_GatewayEthernet.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<Arduino.h>`, `<EspNowIP.h>`, `<ETH.h>`

3. 書き込んだらシリアルモニタを115200 baudへ合わせます。起動ログとソースの操作条件を確認します。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `ETH.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `gateway.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。

変更する前に確認する定数: `kEthAddr`, `kEthPowerPin`, `kEthMdcPin`, `kEthMdioPin`, `kEthType`, `kEthClockMode`。GPIOやチャンネル、容量などは用途に合わせます。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 操作と出力の見方

出力書式の一部は次のとおりです。printfの書式記号は実行時の値に変わります。表示順や数値は実行条件に応じて変わるので、その行へ進む条件と併せて確認します。

```text
ETH Started
ETH Connected
ETH MAC: %s, IPv4: %s, %s, %uMbps
```

## 確認と問題の切り分け

EthernetでBus Gatewayを接続することを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
