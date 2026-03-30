# 09_PhyRateOverride

ESP-NOW の PHY レートを上書きするサンプルです。

## このサンプルで確認できること

- `cfg.phyRate`
- `WIFI_PHY_RATE_1M_L` のような低レート設定
- 明示した PHY でのブロードキャスト動作

## 使い方

同じスケッチを複数ボードに書き込み、シリアルログで指定 PHY レートを確認します。
