# 04_GatewayPPPSerial

物理 UART PPP を uplink に使う gateway 側 `EspNowIP` サンプルです。

## このサンプルで確認できること

- `lwIP PPPoS` と `esp_netif PPP` を組み合わせる前提の構成
- 物理 UART 上で raw PPP peer を扱う gateway 側の骨組み
- 他の uplink 種別と同じ NAT モデルを流用する gateway 例

## 想定用途

Wi-Fi uplink を使わない、または使いにくい環境での代替 gateway サンプルとして使います。

## 現在の方針

- 目標トポロジーは `ESP32 UART <-> host PC PPP peer`
- `PPP.h` は AT モデム前提のため、この用途では使いません
- `ESP_NETIF_DEFAULT_PPP()` で `esp_netif PPP` を作り、`pppapi_pppos_create()` で `ppp_pcb` を張る方針です
- UART 受信データを `pppos_input_tcpip()` に流し、送信は PPPoS の output callback から UART に書き戻します
- その結果得られた `esp_netif_t*` を `cfg.uplink` に渡せば、その先の NAT / routing は他の gateway と共通です
- 任意の初期化済み `Stream` を受ける `EspNowPPPoS` ヘルパーを使います
- シリアル初期化はユーザー側責務です
- ログは `Config.logger = nullptr` で無効、`Print*` を渡せば有効です
- `pppIo` と `debugOut` に同じ `Stream` を指定した場合は、PPP データと混線しないようにログを自動で無効化します
