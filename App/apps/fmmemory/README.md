# FM Memory

This is the memory-mode half of GOGUFW Broadcast FM. It must be installed
together with `FMVFO.app`; use `F+3` to move between VFO and Memory mode.

The implementation is shared with `../fm/fm_app.c` and built with
`FM_MEMORY_ONLY` so both apps keep identical controls and visuals without
duplicating two independent source implementations.
