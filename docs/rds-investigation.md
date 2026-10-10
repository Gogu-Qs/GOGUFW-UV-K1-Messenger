# UV-K1 / GOGUFW FM Broadcast RDS Investigation

Date: 2026-10-02

Scope: determine whether the stock UV-K1 signal path can preserve and decode the 57 kHz FM-broadcast RDS subcarrier, with no permanent hardware modification as the first priority. Statements below are tagged **Confirmed**, **Inference**, **Experimental**, or **Speculation**.

## 1. Executive conclusion

**Confirmed:** BK1080 does not expose RDS in its public register map. Its public analog outputs are `LOUT` and `ROUT`, downstream of the internal FM demodulator, stereo decoder and de-emphasis path. Registers `0x10` through `0x1F` are explicitly described only as internal test registers whose values/procedure must come from Beken.

**Confirmed:** BK4829 is a narrowband two-way-radio transceiver. Beken specifies 6.25, 12.5, 20 and 25 kHz channel modes. The known register interpretation used by this firmware gives a maximum documented receive-filter setting of 5.5 kHz × 2 = 11 kHz. RDS is a 57 kHz subcarrier in the post-demodulated FM multiplex, so a channel filter this narrow precedes the point where RDS would appear.

**Inference, high confidence:** neither stock receive route preserves a usable 57 kHz multiplex signal at a documented pin or MCU input. A firmware-only RDS decoder is therefore not currently viable.

**Experimental:** the added `RDSProbe` build deliberately tests the remaining uncertainty: a reserved BK4829 bandwidth code, AF mux selections 4/9/13/14 and the existing undocumented `REG_3D=0x2AAB` experiment. A negative result is expected, but hardware measurement is the proper way to close the question.

## 2. What the current firmware actually does

**Confirmed:** GOGUFW builds `App/driver/bk4829.c`, not the older `bk4819.c`. The BK4829 initialization writes a large table of partly undocumented values, then runtime receive configuration programs frequency, RX mode, AGC, `REG_43` bandwidth and `REG_47` AF routing.

**Confirmed:** the existing BYP and RAW modes do not expose I/Q or a pre-channel-filter discriminator. Both normally select the FM AF output. Their main documented action is setting `REG_2B` bits 10, 9 and 8, which bypass the 300 Hz receive HPF, 3 kHz LPF and de-emphasis. Those are post-demodulation audio filters; bypassing them cannot undo the earlier RF/IF channel filter.

**Confirmed:** RAW additionally disables AFC. BYP uses the undocumented `REG_3D=0x2AAB`. The AF selector names such as `BASEBAND1`, selector 9, 13 and 14 are inherited empirical labels, not BK4829 documentation.

**Confirmed:** the public BK4829 datasheet shows low-IF I/Q signals going through filter/VGA/ADC and then DSP, but its QFN32 pin list exposes only the processed `EARO` analog output—no I/Q, digital sample bus, discriminator or multiplex output. The datasheet also specifies only narrowband channel modes. See the [BK4829 datasheet](https://manuals.plus/m/b03fb6dba016158ab9f4b5b3d1aeb1d9ea178efc44cc11c2c08398621553cdc9.pdf).

## 3. Can BK4829 preserve the 57 kHz RDS component?

**Confirmed:** `REG_43` is used in the firmware as two selectable receive-filter fields, with an ×2 mode. The largest described field value is 5.5 kHz, producing 11 kHz at the maximum documented setting. The existing WIDE value is `0x3028`; the experiment's maximum documented value is `0x7E28`. Mode bits `11` are not documented and are tested as `0x7E38`.

**Inference, high confidence:** centered on an FM broadcast carrier, ±11 kHz of pre-demodulation bandwidth cannot preserve a subcarrier at ±57 kHz. FM broadcast itself normally needs roughly an order of magnitude more channel width than this chip's intended 12.5/25 kHz voice-radio modes.

**Experimental:** a reserved bandwidth mode could behave differently from the inherited BK4819 interpretation, and an undocumented AF mux might tap a different internal node. Those are the only plausible firmware-only openings found. There is no documentation supporting either possibility.

**Speculation:** measuring RSSI while tuning near carrier ±57 kHz might reveal station-dependent spectral energy, but it would not by itself recover RDS bits and would be easily confused by stereo/audio modulation. It is not a practical decoder route.

## 4. MCU ADC and audio-routing audit

**Confirmed:** current firmware configures PB0/ADC channel 8 for battery measurement. PB1/channel 9 is available at MCU level, but no code or schematic evidence found connects BK4829 `EARO` or BK1080 audio to either ADC input.

**Confirmed:** the PY32F071 ADC is fast enough in principle for a sample rate above 114 ksample/s when appropriately clocked and configured. That is sufficient for a 57 kHz presence experiment only at the Nyquist edge; a practical software RDS decoder would prefer approximately 171, 192 or 228 ksample/s and an analog path with adequate bandwidth. See the [PY32F071 datasheet](https://download.py32.org/Datasheet/en/PY32F071_Datasheet_Rev0.7_EN.pdf).

**Inference:** CPU performance is not the first blocker. The missing wideband physical signal route is. Sampling the normal speaker chain is also unlikely to help because the tuner/transceiver and audio stages are intended to remove ultrasonic content.

## 5. If a usable sample stream becomes available

**Confirmed:** RDS uses a 57 kHz suppressed carrier and 1187.5 symbols/s with differential coding and 26-bit blocks (16 data + 10 check/offset bits).

**Feasible design:** a small fixed-point decoder can use:

1. 171/192/228 ksample/s ADC or external sound interface;
2. a 57 kHz band-pass or complex mixer followed by low-pass/decimation;
3. carrier/timing recovery, or initially a coherent-energy detector for presence only;
4. differential biphase decode;
5. syndrome/offset-word block synchronization;
6. group 0A/0B parsing for PI and PS as the first user-visible result.

**Implemented experiment:** the radio's 48 MHz Cortex-M0+ now runs a compact
fixed-point streaming decoder in the `RDSProbe` preset. It samples a temporary
PB1 input at 192 ksample/s, measures coherent 57 kHz energy, and attempts full
RDS block synchronization and PI/PS extraction. The signal source and decoder
still require validation on real hardware; decoded CRC-valid blocks, not the
amplitude counters alone, are the acceptance criterion.

## 6. BK1080 hidden/test-mode result

**Confirmed:** BK1080 performs low-IF digital demodulation, stereo multiplex decode and de-emphasis internally. Its SOP16 public outputs are left/right audio after this chain; it has no public MPX/discriminator pin. The [BK1080 datasheet](https://file2.dzsc.com/icpdf/17/12/18/1125865_165859249.pdf) explicitly calls `0x10–0x1F` internal test registers not visible to the user.

**Confirmed:** GOGUFW writes the same undocumented initialization values commonly seen in other UV-K firmware and toggles register `0x19`, but no trustworthy public bit definition was found that maps MPX, RDS or discriminator data to `LOUT`, `ROUT` or a GPIO.

**Inference:** blind fuzzing these registers is a low-probability path with real risks: loss of reception, loud output transients and writes whose analog effects are unknown. It should not be the first experiment.

## 7. BK1086/BK1088 replacement comparison

**Confirmed:** BK1086/1088 include a hardware RDS/RBDS decoder. Correct blocks are placed in `RDSA–RDSD`; status register `0x0A` exposes RDS-ready and a GPIO interrupt can be enabled. Their published package is QFN24 4×4 mm; the UV-K1 board inspected in the teardown uses BK1080SB SOP16. See the [BK1086/1088 datasheet](https://xn--p8jqu4215bemxd.com/wp-content/uploads/2019/08/BK1086-88-Datasheet-v1-0.pdf), Beken's [BK1088 product page](https://www.bekencorp.com/en/goods/detail/cid/36.html), and the [UV-K1 teardown](https://github.com/armel/k1-teardown).

**Confirmed:** control concepts are compatible—serial clock/data, reference clock, supplies and L/R outputs—but the pin count, footprint, pin positions, RF input arrangement, power rails and register map differ.

**Conclusion:** this is not a drop-in substitution. It needs an interposer or PCB redesign plus a new driver and RDS UI. BK1088 also adds SW/LW support; that does not make the mechanical/electrical conversion easier.

## 8. Added experimental firmware mode

**Experimental:** build with:

```sh
cmake --preset RDSProbe --fresh
cmake --build --preset RDSProbe -j 4
```

The preset disables Messenger to provide safe flash headroom. Normal `Fusion` keeps RDS probing disabled.

On the radio:

1. Open FM Radio and stay in VFO mode on a strong known-RDS station.
2. Press `F`, then `2` repeatedly to cycle P1 through P8; the ninth press returns to normal BK1080 reception.
3. The normal VFO ruler is replaced by two live lines. The first shows profile,
   BK4829 voice amplitude and measured 57 kHz quality. The second advances
   through `NO ADC`, `NO 57K`, `CARRIER`, `SYNC`, then `PI + PS`.
4. UP/DOWN and direct frequency entry retune BK4829 while preserving the selected profile.
5. MENU, scan-related shortcuts and PTT are blocked while the probe is active. EXIT restores the saved BK4829 registers before leaving FM Radio.

Profiles:

| Profile | Change under test | Evidence class |
|---|---|---|
| P1 STOCK | Existing wide receive setting, normal AF | Control |
| P2 MAX11K | Documented maximum filter; HPF/LPF/de-emphasis bypass | Confirmed fields |
| P3 BW-RES | Reserved bandwidth encoding `11` | Experimental |
| P4 AF-RAW | AF selector 4 plus AFC off | Experimental |
| P5 AF-9 | AF selector 9 | Experimental |
| P6 AF-13 | AF selector 13; highest observed `V`, test first | Leading candidate |
| P7 AF-14 | AF selector 14 | Experimental |
| P8 REG3D | Existing `0x2AAB` experiment | Experimental |

Each profile is rebuilt from the first-entry register snapshot; profile order therefore does not contaminate later measurements. Leaving the probe restores `REG_2B`, `30`, `38`, `39`, `3D`, `43`, `47` and `73`.

### Temporary ADC connection required for decoding

The decoder uses MCU `PB1 / ADC_IN9` at 64 ksample/s. PB1 is unused by the
normal firmware, but it is not connected to BK4829 `EARO` on the stock board.
Use this temporary AC-coupled connection:

```text
BK4829 EARO --- 1 uF non-polar capacitor --- 1 kOhm ---+--- PB1 / ADC_IN9
                                                       |
                                                   10 kOhm
                                                       |
                                                      3.3 V
                                                       |
                                                   10 kOhm
                                                       |
                                                      GND
```

The two 10 kOhm resistors bias PB1 near 1.65 V; the capacitor prevents the
EARO DC level from disturbing that bias. Confirm 0–3.3 V at PB1 with an
oscilloscope before connecting it to the MCU. Never connect EARO directly to
PB1 without AC coupling and bias. Keep leads short and share radio ground.
The diagnostic line reports `A` as the 12-bit ADC DC level and `S` as the
short-term peak-to-peak span. A working 1.65 V bias should put `A` near 2048;
`A` near 0 or 4095 indicates a missing/incorrect bias or connection. `S:0`
means no AC signal is reaching PB1, regardless of the separate BK4829 `V`
counter.

The on-device decoder deliberately undersamples at 64 ksample/s, where the 57
kHz carrier aliases to 7 kHz. A complex mixer and low-pass produce a 16
ksample/s baseband for eight biphase timing hypotheses, differential decoding,
RDS CRC/offset-word block synchronization, and group 0A/0B PI/PS extraction.
DMA completion flags are polled without enabling the DMA interrupt; all signal
processing runs in the normal application loop so it cannot monopolize
interrupt time. It resets independently for every P1–P8 profile. `57:n` alone
is not proof; `SYNC` and repeatable PI/PS are the proof.

Physical observation recorded during initial testing: P6 (`AF selector 13`)
produced the highest BK4829 `V` value. This makes P6 the leading candidate but
does not establish 57 kHz bandwidth until CRC-valid RDS blocks are received.

## 9. Recommended physical test and decision gate

Start on P6 with a very strong station whose RDS is confirmed by another
receiver, then compare P1–P8 at the exact same frequency and antenna position.
The live counters are only health indicators; they cannot prove that 57 kHz
survives.

For a conclusive result, use a high-impedance oscilloscope or ≥192 ksample/s external ADC to inspect BK4829 `EARO` directly. This requires opening/probing the radio but no permanent circuit change. Compare spectra with RDS on/off stations and look specifically near 19, 38 and 57 kHz. Keep the speaker volume low when changing undocumented AF profiles.

Decision gate:

- If no profile shows reproducible 57 kHz energy at `EARO`, stop firmware-only RDS work; the expected channel-filter limitation is confirmed.
- If a profile shows `CARRIER` but never `SYNC`, capture several seconds of raw
  PB1 samples for offline analysis and decoder tuning.
- If `SYNC` and a stable PI/PS appear repeatedly, repeat on other known-RDS
  stations to rule out false synchronization.
- If RDS is a product requirement rather than an experiment, a receiver with documented RDS support (BK1086/1088-class or a separate tuner/module) is the dependable route.

Community caution: a recently circulated “BK4829 register list” was identified by maintainers as apparently a retitled BK4819 document, so it was not treated as authoritative. See the [register-list discussion](https://github.com/armel/uv-k1-k5v3-firmware-custom/discussions/36).
