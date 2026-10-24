# 01_DeviceBasic

device 側 `EspNowIP` の最小構成サンプルです。

## このサンプルで確認できること

- `EspNowIP` device 初期化の基本形
- 仮想 IP インターフェイスの起動
- link / lease 状態の確認
- 後続の device 側サンプルの土台構成

## 想定用途

最初に device 側の `Bus session -> IP session` 確立と lease 適用を確認するためのベースに使います。

## 補足

- `EspNowBus` は既定では `groupName` から channel を自動設定します。通常はこの既定のまま使い、環境によって必要な場合のみ全端末で同じ channel を明示設定します。
- `03_GatewayWiFiSTA` と組み合わせて使う場合は、gateway 側が表示する STA channel に device 側 `EspNowBus` channel を合わせる必要があります。
- このサンプルは device 側からは募集せず、`cfg.autoJoinIntervalMs = 0` にして gateway 側の募集へ乗る前提です。
- 設定例（通常はコメントアウトのままで使う想定）:

```cpp
// EspNowIP::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.autoJoinIntervalMs = 0;      // Device stays silent; gateway advertises
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [01_DeviceBasic.ino](01_DeviceBasic.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<Arduino.h>`, `<EspNowIP.h>`, `<esp_netif.h>`, `<esp_wifi.h>`

3. 書き込んだらシリアルモニタを115200 baudへ合わせます。起動ログとソースの操作条件を確認します。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `ip.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `ip.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 操作と出力の見方

出力書式の一部は次のとおりです。printfの書式記号は実行時の値に変わります。表示順や数値は実行条件に応じて変わるので、その行へ進む条件と併せて確認します。

```text
selfMac=%02X:%02X:%02X:%02X:%02X:%02X
channel=%u second=%d
channel=?
```

## 確認と問題の切り分け

Bus上のIP Endpointを開始することを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
