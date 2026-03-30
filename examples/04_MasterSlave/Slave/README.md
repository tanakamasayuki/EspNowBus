# 04_MasterSlave / Slave

Slave-side sketch for the master/slave example pair.

## What It Does

- Uses the same fixed group name as `Master`
- Does not advertise on its own
- Periodically sends `value=<millis>` style payloads to all known peers

Flash this sketch to one or more sender-side boards and pair them with a board running `Master`.
