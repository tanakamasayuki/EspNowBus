# 03_GatewayWiFiSTA

uplink に Wi-Fi STA を使う gateway 側 `EspNowIP` サンプルです。

## このサンプルで確認できること

- `EspNowIPGateway` 初期化の基本形
- Wi-Fi STA uplink の準備
- `routing + IPv4 NAT` gateway の基本構成
- STA uplink が使っている channel の表示

## 制約

- このサンプルは試験しやすい一方で、実運用の本命 uplink としては扱いにくいです。
- `EspNowBus` は既定では `groupName` から channel を自動設定します。通常はその既定のまま使いますが、このサンプルでは実効 channel が STA uplink 側に引きずられるため、必要に応じて全端末で同じ channel を明示設定します。
- `ESP-NOW` と `Wi-Fi STA` を併用する場合、実効的な `ESP-NOW` channel は接続中の STA channel に制約されます。
- AP の channel が変わる、roaming する、複数 AP で channel が異なる、といった状況では device 側 channel が揃わず正常に link できなくなる可能性があります。

## 想定用途

Wi-Fi STA uplink 経路の検証や gateway 側の確認に使うサンプルです。

## テスト手順

1. gateway 側に Wi-Fi STA の SSID / パスワードを設定します。
2. gateway を起動し、シリアルログに表示される STA channel を確認します。
3. device 側 `EspNowBus` の channel を、その表示された channel と同じ値に設定します。
4. device 側サンプルを起動し、`Bus session`、続いて `IP session` が張れることを確認します。

設定例（通常はコメントアウトのままで使う想定）:

```cpp
// EspNowIPGateway::Config cfg;
// cfg.groupName = "espnow-ip-demo";
// cfg.channel = 6;                  // Optional
// cfg.phyRate = WIFI_PHY_RATE_1M_L; // Optional
```

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [03_GatewayWiFiSTA.ino](03_GatewayWiFiSTA.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<Arduino.h>`, `<WiFi.h>`, `<EspNowIP.h>`, `<esp_netif.h>`, `<time.h>`

3. 書き込んだらシリアルモニタを115200 baudへ合わせます。起動ログとソースの操作条件を確認します。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `gateway.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `gateway.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 操作と出力の見方

出力書式の一部は次のとおりです。printfの書式記号は実行時の値に変わります。表示順や数値は実行条件に応じて変わるので、その行へ進む条件と併せて確認します。

```text
Started NTP sync for uplink check.
Wi-Fi STA connected, channel=%ld, ip=%s
 gw=
```

## 確認と問題の切り分け

Wi-Fi STAでBus Gatewayを接続することを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
