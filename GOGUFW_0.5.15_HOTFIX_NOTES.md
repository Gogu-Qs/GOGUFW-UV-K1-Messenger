# GOGUFW 0.5.15 Hotfix Notes

Base: GOGUFW 0.5.14 range_jitter_chirp_beep_source

Changes:
- Range Check cold-boot RX readiness: after boot FSK sidecar prime, schedule a one-time safe controlled re-prime; after Range PING TX, request immediate safe RX re-prime before the 10 s PONG listen window.
- Range Check result row UI: draw callsign, RSSI, battery voltage, and signal bars at fixed X coordinates so RSSI and voltage no longer appear crowded.
- Version bumped to 0.5.15.

Safety intent:
- Keep Messenger RF/ACK/retry behavior intact.
- Do not touch REG_47/audio path; use the existing controlled re-prime mechanism only while idle.
