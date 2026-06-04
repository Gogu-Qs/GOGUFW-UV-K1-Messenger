# F+9 PMR Call Tone Test

Base: GOGUFW v0.5.11 (`GOGUFW_0.5.11_msgcsg_alignment_fix_source.zip`)

Change:
- Short F+9 now transmits a PMR-style RF call tone instead of the old F+9 action.
- The tone is an alternating 1180 Hz / 1560 Hz "dili-dili" pattern.
- Total tone duration is approximately 3.5 seconds.
- Long-press 9 keeps the original 1-CALL channel shortcut behavior.
- Before tone TX, Messenger RF voice-path restore is called when Messenger is enabled.
- Normal TX permission checks are still handled through `RADIO_PrepareTX()`.

Files changed:
- `App/app/main.c`

Build note:
- Docker is not available in this ChatGPT container, so a full firmware build could not be run here.
- I did run a host-side syntax check of `App/app/main.c` with the project include paths and `PY32F071xB`; no `main.c` syntax errors were reported.
