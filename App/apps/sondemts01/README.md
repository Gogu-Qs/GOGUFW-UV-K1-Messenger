# Sonde MTS01 overlay

Receive-only Meteosis MTS01 decoder for the UV-K1 / UV-K5 v3 overlay loader.
It uses the radio's BK4829 discriminator audio on PA4 and samples it at 9.6
kHz. The streaming integer decoder implements the format used by the current
`projecthorus/radiosonde_auto_rx` `mts01mod.c` decoder:

- 1200 baud NRZ GFSK, either polarity;
- `0xAAAAB42B` 32-bit sync word (up to two hard-bit errors);
- `0x80`, 128-byte comma-separated ASCII payload and two-byte CRC-16;
- ID, frame number, position, altitude, speed, heading, battery and thermistor
  temperature parsing.

Version 1.1 continuously scans the complete 400.15–406.00 MHz meteorological
band in 10 kHz steps, starting at 401.90 MHz so the Isparta 402.00 MHz MTS01
channel is checked first. A local signal region at or above
-116 dBm is reduced to its strongest channel and probed for 2.5 seconds. A
valid MTS01 frame locks reception; scanning resumes after 20 seconds without a
valid frame. The user's original VFO is restored by the overlay loader on exit.
The SondeHub Isparta site record also lists 403.01 MHz for DFM17; that channel
is scanned but cannot be decoded by this MTS01-only application.

`1` toggles the speaker, `2` clears the accepted-frame counter, and `EXIT`
closes the app. The speaker setting is saved on exit. A valid frame flashes the
green LED. Signal detection is automatic, but decoding in this app is MTS01
only; unrelated signals and other radiosonde protocols are skipped.

Build from the repository root:

```sh
./compile-app.sh sondemts01
```

The result is `build/Apps/SondeMTS01.app`. Run the host decoder test with:

```sh
App/apps/sondemts01/test/run_tests.sh
```

The MTS01 decoder portion is adapted from `radiosonde_auto_rx`, GPL-3.0-or-
later. The application/hardware shell is adapted from Armel F4HWN's Apache-2.0
Sonde M20 v1.0 overlay. The combined MTS01 application is distributed under
GPL-3.0-or-later.
