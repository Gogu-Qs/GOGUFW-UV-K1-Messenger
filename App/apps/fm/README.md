# GOGUFW Broadcast FM overlays

Broadcast FM is split into two paired overlay apps because the complete GOGUFW
VFO, memory, channel-name editor and matching UI cannot fit safely in one 4 KiB
overlay. Install both `FMVFO.app` and `FMMemory.app`.

- `FM VFO`: frequency entry, tuning, manual/automatic scan and saving to any of
  48 FM memories.
- `FM Memory`: 48-memory navigation, channel names, deletion and scanning.
- `F+3` switches between the two apps through a deferred overlay hand-off. The
  last mode and frequency/channel are retained.
- `MENU` opens the GOGUFW menu, including Live RSSI. `F+1` changes band,
  `STAR` scans, `F+STAR` starts auto scan after confirmation, and `EXIT` closes.
- The normal main-screen `F+0` shortcut opens FM. Inside FM, `F+0` or `EXIT`
  returns to the screen and VFO/MR state that launched it.
- Selected-channel RX temporarily suspends FM and shows the normal radio screen.
  Five seconds after RX ends, the same FM mode, band and frequency resume.
- A held shortcut key is released before a chained FM mode accepts input, so
  `F+3` cannot enter digit 3 or jump to channel 3 in the destination mode.
- Manual VFO scan remains in scan mode when a station is found: UP/DOWN resumes
  scanning in either direction and EXIT returns to normal VFO operation.
- At a found station, MENU opens the FM menu and UP/DOWN changes its selection;
  scan navigation resumes only after the menu is closed.
- Inside Ch Save, UP/DOWN and two-digit entry select the destination memory even
  when the frequency came from manual scan; EXIT cancels Ch Save first.
- Memory band selection skips bands without saved stations instead of silently
  switching to VFO mode.

Only the BK1080 driver and the persistence bridge remain resident. FM settings,
the 48-channel table and the existing station-name sector keep their original
addresses and formats.
