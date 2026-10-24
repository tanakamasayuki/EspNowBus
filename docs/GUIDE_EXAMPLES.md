# EspNowBus — Example workflow

> 日本語版: [GUIDE_EXAMPLES.ja.md](GUIDE_EXAMPLES.ja.md)

Exchange grouped ESP-NOW messages and inspect delivery.

Check installation and platform requirements in the [library README](../README.md), then choose a sketch from the [example index](../examples/README.md). Version/support tables and internal capacity defaults stay in their original reference rather than being duplicated here.

## From setup to a verified result

1. Choose the smallest relevant sketch and check its board, hardware, dependencies and bundled files.
2. Match pins, peers and input files to your actual setup. Each README identifies important placeholders.
3. Open the .ino, select board/port and upload. Match Serial baud where used.
4. Locate the last completed stage: initialization, input, processing or output. Compilation does not test hardware I/O.
5. Reproduce the supplied example before changing one setting at a time.

## API and data handling

Use compatible peers with the same group settings and Wi-Fi channel. The basic sketches put __FILE__ in groupName, so different sketches can derive different groups unless you set an explicit common value on both. Start by reproducing a matching pair, then change addresses, channel and security settings deliberately.

Queue acceptance, physical SentOk and application AppAckReceived are different stages; even an app ACK does not prove a downstream actuator finished. Broadcast does not have per-peer unicast acknowledgement semantics. Receive payloads have an explicit length: the sample text includes a terminator, but arbitrary binary input does not.

Copy needed bytes before returning from callbacks and synchronize data shared with loop. Serial/IP adapters add their own framing, buffering and hardware/network setup; they do not turn every radio packet into an Internet connection automatically.

## When adapting it

- Distinguish bool status, processed counts and handles; do not use stale output after failure.
- Registered callbacks run later from their documented context. Decide synchronization and retained-data lifetime.
- Keep bytes, samples, frames and pixels distinct; capacity is different from valid length.
- Retain views/pointers/buffers for the required period. Copying a pointer does not copy its payload.
- Add application-specific failure handling, retry and cleanup that a minimal example may omit.

## Read the individual README

Use Prepare and run for hardware and steps, Follow the sketch for the actual APIs and precautions for feature-specific constraints. If output is missing, check initialization first, continued service calls next and input/peer readiness after that.
