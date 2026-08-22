# GOGUFW 1.0.3 + PR405 + PR423 selective fix

Applied selectively from the provided 1.1.0 source zip only:

- Minimal FSK sidecar behavior:
  - FSK IRQ mask is written only while arming the FSK RX path.
  - Removed periodic 10 ms IRQ-mask refresh.
  - Removed startup follow-up keepalive/re-prime.
  - Kept delayed re-arm only through the TX/post-TX controlled re-prime path.
  - No idle periodic keepalive is enabled; an idle-only low-frequency refresh can be added later if needed.
- VOX restore behavior:
  - VOX ON leaves Messenger FSK/data sidecar and restores stock voice/VOX path.
  - Messenger FSK sidecar is not armed while VOX is ON.
  - VOX menu/action changes notify Messenger RF restore logic.
- FM radio RSSI bar:
  - Added the final HEARD-pixel-style 5-bar FM RSSI meter.
  - Kept the latest accepted geometry/alignment with the bottom baseline aligned to the lower FM label area.

Intentionally not ported:

- Unrelated 1.1.0 cleanup/version/readme/archive changes.
- Unrelated Messenger store/UI/debug removals.
- Other non-requested source differences from the 1.1.0 zip.

Build note:

- Build was not run in this environment because arm-none-eabi-gcc is unavailable.
