# GOGUFW 0.5.12 Range Check refinement

Base: GOGUFW 0.5.11 range_check_source

Changes:
- Version defaults updated to 0.5.12.
- Range Check WAIT/listen window increased from 3s to 10s.
- Range PONG response jitter widened to 500-9500ms; busy backoff increased to 1000ms.
- Range Check top separator moved upward to sit closer below title.
- Initial "PRESS MENU / TO CHECK" prompt re-centered between separators.
- Found list now shows up to 4 rows and supports scrolling through up to 10 entries.
- Added temporary Range Check UI test shortcuts:
  - KEY 1 fills 10 fake found nodes with RSSI values for scroll/UI testing.
  - KEY 2 clears the test list and returns to the initial prompt.

Notes:
- Docker/firmware build was not run in this environment because docker is unavailable.
- No new flash storage area was added; Range Check found list remains RAM-only.
