# GOGUFW Messenger 1.2.0

GOGUFW is a messaging-focused firmware for the Quansheng UV-K1 / UV-K5 V3 hardware using the PY32F071 MCU and BK4829 RF IC.

The project is derived from F4HWN Fusion and currently incorporates selected changes through F4HWN 5.9.0. It is not a clean upstream tree: Messenger reliability and compatibility with existing GOGUFW radios take priority over broad feature merges.

> The `1.2.0` line is the current development line. The preserved pre-development baseline is tagged `v1.1.2-known-good`.

## Main features

- On-radio text Messenger with Inbox, Compose, Sent, Drafts, Reply and Resend
- Screen-independent FSK reception and ACK processing
- First-attempt wake preamble followed by compatibility-safe TEXT packets
- ACK queue, retry handling and delivery-source tracking
- HEARD station list with RSSI and packet type
- Range Check using PING/PONG, callsign, RSSI and voltage reporting
- Assignable Messenger, HEARD and CALLTX shortcuts
- FM broadcast station names and RSSI display
- Survival Mode and F4HWN Fusion feature set
- GOGUFW-aware CHIRP support

## Messenger architecture

Messenger runs as an FSK sidecar to the normal analog radio state machine. Normal voice RX, squelch and audio routing remain authoritative.

Important compatibility rules:

- FSK interrupts are armed only when Messenger RX is actually armed.
- There is no periodic 10 ms IRQ-mask rewrite or continuous FSK keepalive.
- Normal Messenger TX uses one invisible wake frame before the first TEXT attempt.
- Retries send only the original TEXT packet.
- ACK processing is independent of the currently displayed screen.
- VOX disables the Messenger FSK sidecar while voice operation owns the audio path.
- Range Check shares the RF transport but retains its own timing and restore behavior.

Packet framing, message IDs, callsign handling, ACK format, retry behavior and duplicate detection are compatibility-sensitive.

## Hardware

- Quansheng UV-K1
- Quansheng UV-K5 V3 variants using PY32F071 and BK4829

This firmware is not intended for unrelated BK4819-based hardware.

## Build on macOS

Requirements:

- CMake 3.22 or newer
- Ninja
- ARM GNU Embedded toolchain (`arm-none-eabi-gcc`)

Clean configure and build:

```bash
cmake --fresh --preset Fusion
cmake --build --preset Fusion --clean-first
```

Normal incremental build:

```bash
cmake --preset Fusion
cmake --build --preset Fusion
```

Outputs are generated under `build/Fusion/`:

- `gogufw.bin`
- `gogufw.hex`
- `gogufw.elf`
- `gogufw.map`

See [BUILD_VSCODE_MAC.md](BUILD_VSCODE_MAC.md) for the native VS Code workflow and [BUILD_WITH_VSCODE.md](BUILD_WITH_VSCODE.md) for the Docker workflow.

## Memory limits

The firmware operates close to the PY32F071 memory limits. Every meaningful change should be followed by a clean Fusion build and review of both FLASH and RAM usage. Avoid large static buffers and speculative RF state machines.

## Persistent storage

Messenger configuration uses the dedicated PY25Q16 external-flash sector at `0x012000`. FM station names use `0x013000`, exposed to CHIRP through the compatibility alias at `0x00D000`.

Do not change external-flash offsets or persisted structure layouts without deriving and checking the complete memory map.

## Development policy

The repository-specific reliability rules and regression checklist are documented in [AGENTS.md](AGENTS.md). RF, power-save, dual-watch, scanning, VOX, EEPROM and TX-to-RX changes require explicit Messenger regression review.

## Screens

![Messenger compose screen](uv-k5-screenshot17.png)

![HEARD station list](heard.png)

![Range Check](rangecheck.png)

## Attribution

GOGUFW is based on the F4HWN / UV-K5 custom firmware project and retains the original project attribution and license.

## Disclaimer

This firmware is provided as-is. Users are responsible for complying with the radio regulations, licensing requirements and permitted frequencies applicable in their jurisdiction.
