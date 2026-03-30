# 03_SendToAllPeers

Send the same message to every known peer by unicast.

## What It Shows

- `sendToAllPeers(...)`
- Per-peer delivery using unicast instead of plain broadcast
- Peer table inspection with `peerCount()` / `getPeer()`
- Receive logging for group-wide traffic

## How to Use

1. Flash the same sketch to two or more boards.
2. Open serial monitors.
3. Wait until peers appear.
4. Confirm that `hello all peers` is received by the other nodes.

Compared with plain broadcast, this path uses peer-based unicast and can take advantage of features such as app-level ACK.
