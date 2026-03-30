# 02_JoinAndUnicast

Peer discovery and unicast example with app-level ACK enabled.

## What It Shows

- Automatic peer discovery through JOIN traffic
- `peerCount()` and `getPeer()`
- Random unicast send with `bus.sendTo(...)`
- Inbound payload logging on the receiver side

## How to Use

1. Flash the same sketch to two or more boards.
2. Open the serial monitor.
3. Wait until peers are discovered.
4. Confirm that `hello peer` is sent to one known peer at a time.

This example is useful when you want the simplest peer-to-peer unicast flow.
