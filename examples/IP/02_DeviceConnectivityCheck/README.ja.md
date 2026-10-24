# 02_DeviceConnectivityCheck

`EspNowIP` の疎通診断をまとめて行う device 側サンプルです。

## このサンプルで確認できること

- `gateway` への ICMP ping
- DNS 名前解決
- NTP への UDP 通信
- HTTP GET による TCP 通信

## 想定用途

`01_DeviceBasic` で lease が取れたあとに、実際の IP 通信がどこまで通るかを段階的に確認します。

## 補足

- 宛先はサンプル内の固定値です。
- 実験時はこのサンプルを `examples/IP/temp/` へコピーしてから編集する運用を想定しています。
- `gateway ping` は `10.201.0.1` を使います。
- 外向き確認は `example.com`、`pool.ntp.org`、`http://example.com/` を使います。
- device 側は `cfg.autoJoinIntervalMs = 0` で自分からは募集せず、gateway 側の募集を受けて lease 取得する前提です。

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [02_DeviceConnectivityCheck.ino](02_DeviceConnectivityCheck.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<Arduino.h>`, `<EspNowIP.h>`, `<HTTPClient.h>`, `<Network.h>`, `<NetworkClient.h>`, `<NetworkUdp.h>`, `<esp_netif.h>`, `<esp_wifi.h>`, `<lwip/ip_addr.h>`, `<ping/ping_sock.h>`

3. 書き込んだらシリアルモニタを115200 baudへ合わせます。起動ログとソースの操作条件を確認します。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `udp.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `ip.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。

変更する前に確認する定数: `kMtu`, `kRunIntervalMs`, `kPingTaskStackSize`, `kNtpRetries`, `kNtpPort`。GPIOやチャンネル、容量などは用途に合わせます。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 操作と出力の見方

出力書式の一部は次のとおりです。printfの書式記号は実行時の値に変わります。表示順や数値は実行条件に応じて変わるので、その行へ進む条件と併せて確認します。

```text
selfMac=%02X:%02X:%02X:%02X:%02X:%02X
channel=%u second=%d
ip=
```

## 確認と問題の切り分け

Device側のIP疎通を確認することを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
