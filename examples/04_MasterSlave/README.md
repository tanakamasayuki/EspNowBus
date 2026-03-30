# 04_MasterSlave

Two-sketch example that separates roles into a master receiver and a slave sender.

## Included Sketches

- `Master/Master.ino`
- `Slave/Slave.ino`

## Role Split

- Master: waits for registrations and mainly receives data
- Slave: does not advertise on its own and periodically sends payloads to all known peers

Flash `Master` to one board and `Slave` to one or more other boards.
