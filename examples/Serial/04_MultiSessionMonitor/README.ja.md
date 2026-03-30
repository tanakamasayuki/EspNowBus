# 04_MultiSessionMonitor

全 session を走査し、読めるデータを USB Serial に流す controller 側サンプルです。

## このサンプルで確認できること

- `sessionCapacity()`
- `sessionInUse()`
- `sessionConnected()`
- `sessionAvailable()`
- `bindSession(index)`

複数 session のトラフィックを手動切り替えせずに観察したいときに使います。

## 典型的な組み合わせ

相手側には `03_SilentDevice` を使う構成が適しています。`03_SilentDevice` は定期的に状態行を送信し、入力のエコーバックも行うため、monitor 側で動きが確認しやすくなります。
