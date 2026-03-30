# 05_SendStatusDemo

Demonstrates `SendStatus` transitions for unicast sends.

## What It Shows

- `onSendResult(...)`
- `onAppAck(...)`
- The difference between physical send success and logical ACK success

## How to Use

1. Flash the same sketch to two boards.
2. Open the serial monitor.
3. Watch status lines such as `Queued`, `SentOk`, `AppAckReceived`, or `AppAckTimeout`.

This sketch is useful when tuning retry counts, timeouts, and app-level ACK behavior.
