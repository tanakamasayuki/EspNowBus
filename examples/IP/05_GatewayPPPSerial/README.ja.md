# 05_GatewayPPPSerial

物理 UART PPP を uplink に使う gateway 側 `EspNowIP` サンプルです。

## このサンプルで確認できること

- `lwIP PPPoS` と `esp_netif PPP` を組み合わせる前提の構成
- 物理 UART 上で raw PPP peer を扱う gateway 側の骨組み
- 他の uplink 種別と同じ NAT モデルを流用する gateway 例

## 想定用途

Wi-Fi uplink を使わない、または使いにくい環境での代替 gateway サンプルとして使います。

## 補足

- 目標トポロジーは `ESP32 UART <-> host PC PPP peer`
- `PPP.h` は AT モデム前提のため、この用途では使いません
- `ESP_NETIF_DEFAULT_PPP()` で `esp_netif PPP` を作り、`pppapi_pppos_create()` で `ppp_pcb` を張る方針です
- UART 受信データを `pppos_input_tcpip()` に流し、送信は PPPoS の output callback から UART に書き戻します
- その結果得られた `esp_netif_t*` を `cfg.uplink` に渡せば、その先の NAT / routing は Wi-Fi STA や Ethernet uplink と同じです
- このサンプル配下のローカル `PPPoSNetif` ヘルパーが、任意の初期化済み `Stream` を受けます
- シリアル初期化はユーザー側責務です
- ログは `Config.logger = nullptr` で無効、`Print*` を渡せば有効です
- `pppIo` と `debugOut` に同じ `Stream` を指定した場合は、PPP データと混線しないようにログを自動で無効化します

## 準備と実行

1. [サンプル入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)で導入条件と必要な機器を確認します。対応するボード・外部ライブラリはルートREADMEを参照します。
2. [05_GatewayPPPSerial.ino](05_GatewayPPPSerial.ino)を開き、実際のボードとポートを選びます。次のincludeで使う依存機能も準備してください。

   `<Arduino.h>`, `<EspNowIP.h>`

同じフォルダの必要ファイル: [PPPoSNetif.h](PPPoSNetif.h)

3. 書き込み後は画面／入力など、この例の出力先で確認します。シリアルを使う場合はソースの初期化条件に合わせます。

## 動作の流れ

以下はソース内で最初に現れるAPIの順です。コールバック登録の位置は、実際の呼び出し順序とは異なります。

1. `pppos.begin(...)` — 前提となる機器を準備してから初期化し、結果を返すAPIでは成否を確認します。
2. `pppos.poll(...)` — 保留入力を定期的に処理し、結果を確認してから新しい値を使います。

変更する前に確認する定数: `kEnablePppLogs`。GPIOやチャンネル、容量などは用途に合わせます。

## APIの使い方と注意点

互換な相手とグループ設定・Wi-Fi チャンネルを合わせます。基本例のgroupNameには__FILE__が入るため、違うスケッチ同士では共通の名前を明示しないと別グループになる場合があります。まず対応する組を再現し、アドレス・チャンネル・Securityを意図して変更します。

キュー受理、物理的なSentOk、アプリのAppAckReceivedは別の段階で、App ACKでも下流装置の動作完了までは示しません。BroadcastにUnicastと同じ相手ごとのACKはありません。受信データには長さがあり、例の文字列は終端付きでも任意のバイナリがそうとは限りません。

必要な値はコールバックから戻る前にコピーし、loopと共有する状態は同期します。シリアル／IP用アダプタには独自の区切り・バッファ・機器／ネットワーク設定があり、無線PacketだけでInternetへつながるわけではありません。

## 確認と問題の切り分け

シリアル PPPのGateway Endpointを使うことを出力から確認します。まず例のままの入力で試し、その後に設定やデータ源を1つずつ変えます。

| 症状 | 最初に確認すること |
|---|---|
| 出力がない | ボード／依存ライブラリ、初期化結果、表示先とシリアル速度 |
| 開始するが途中で止まる | 待機条件、バッファの空き、相手や入力の準備、継続呼び出し |
| 値が不自然・不安定 | 長さと単位、形式、寿命、共有状態、部分的な処理結果 |

共通の考え方は[入門ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)、別の例は[サンプル一覧](../../README.ja.md)へ進んでください。
