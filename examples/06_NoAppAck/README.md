# 06_NoAppAck

Example with app-level ACK disabled.

## What It Shows

- `enableAppAck = false`
- Physical send result handling without logical ACK
- That `onAppAck(...)` is not expected to fire in this mode

## How to Use

Flash the same sketch to two boards and compare the send-result logs with `05_SendStatusDemo`.
