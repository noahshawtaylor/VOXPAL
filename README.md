# VOXPAL — signature vocal chain plugin (v0.3)

A mono JUCE plugin that runs one fixed vocal chain with seven knobs. 32-bit float processing, the two compressors 2x oversampled, tested at 44.1, 48, 96 and 192 kHz. Reports 4 samples of latency (the oversampler) so the DAW compensates. Defaults are the
house settings; "reset" lands on the chain exactly as you mix with it.

## Signal path (fixed order)

```
High-pass 100 Hz, 18 dB/oct   (always on, no control)
  → TOM            FET (1176 circuit): JFET shunt divider driven by a full-wave feedback detector; 4:1, attack "3" (~0.5 ms), release "7" (50 ms)
  → SPIKE          Opto (LA-2A circuit): EL panel + CdS photoresistor model of the T4 cell; Compress mode, stock R37 emphasis
  → Control        two dynamic bands: Boom (200–350 Hz) and Harsh (2.5–5.5 kHz, centred 3.7k)
  → Passive EQ     Pultec EQP-1A network: first-order boost/atten shelves with the measured hardware knees at "100", 10 kHz LC bell; boost 3 / atten 2 / high 3
  → Air            16 kHz boost, Q dial tracks the boost value (4 dB → Q "4"), proportional-Q on top
  → Gain Match     RMS-based loudness match to the post-HPF input (1.5 s window, 300 ms ramp)
  → Output trim
```

Compressor 1's input drive is compensated at its output, so Compressor 2 always
sees a consistent level no matter how hard you push the first stage.

## Controls

| Knob | Range | Default | What it does |
|---|---|---|---|
| Compressor 1 | −12 … +12 dB | 0 | Drive into the FET stage (everything else fixed) |
| Compressor 2 | 0 … 100 | 50 | Peak Reduction on the opto stage |
| Control Boom | 0 … 10 | 5 | Threshold for the 200–350 Hz dynamic cut (0 = off) |
| Control Harsh | 0 … 10 | 5 | Threshold for the 2.5–5.5 kHz dynamic cut (0 = off) |
| EQ Low | 0 … 10 | 3 | 100 Hz boost knob; atten follows at ⅔ (3 → boost 3 / atten 2) |
| EQ High | 0 … 10 | 3 | 10 kHz boost knob |
| Air | 0 … 8 dB | 4 | 16 kHz boost; bandwidth tracks the boost value |

Header: **Gain Match**, **Bypass**, **Output**. Each section has an on/off.
With Gain Match on, Bypass is a level-matched A/B by construction (the processed
path already sits at input loudness).

### Sweet-spot lights

Each compressor and each Control band has a GR meter with a target zone and a
three-state LED driven by a 400 ms rolling average of gain reduction:

| Stage | Green zone |
|---|---|
| Compressor 1 | 2 – 5 dB |
| Compressor 2 | 3 – 5 dB |
| Control Harsh | 3 – 4 dB |
| Control Boom | 2 – 4 dB |

Dim = under, green = in the zone, amber = over.

## Installing on a Mac (Ableton, Logic, Pro Tools)

You need Xcode (free, from the App Store) and CMake (`brew install cmake`).
Then, in Terminal:

```
./scripts/build-mac.sh
```

That builds a universal (Apple Silicon + Intel) VST3 and AU, installs both into
`~/Library/Audio/Plug-Ins`, ad-hoc signs them so macOS and Logic accept a local
build, and clears the AU cache. Rescan plugins in Ableton; Logic finds the AU on
its next launch. To verify the AU the way Logic does: `auval -v aufx Vxpl Nsta`.

**The installer (no Xcode needed).** Push this folder to a GitHub repository.
The included workflow (`.github/workflows/build-mac.yml`) builds the plugin on
GitHub's Mac runners, validates the AU, and attaches `VOXPAL <version>.pkg`
to the run (Actions tab → the latest run → Artifacts). Download, double-click,
and it installs the VST3 and AU system-wide. Until the pkg is Developer-ID
signed, macOS shows a warning on first open: go to System Settings → Privacy &
Security and click **Open Anyway**, then run it again.

To ship it to other people without that warning you need an Apple Developer
account ($99/yr). Add these repository secrets and the same workflow signs and
notarizes automatically: `DEVELOPER_ID_APPLICATION`, `DEVELOPER_ID_INSTALLER`
(the certificate names), `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`
(an app-specific password). You also need the certificates imported on the
runner, which is a one-time keychain step; ask when you get there.

Building locally? `./scripts/build-mac.sh` then `./scripts/make-pkg.sh` makes
the same pkg in `dist/`.

### Pro Tools / AAX

Pro Tools only loads AAX, and AAX needs two things from Avid:

1. **The AAX SDK.** Register as a developer at developer.avid.com (free) and
   download it. Then build with the SDK path:
   `./scripts/build-mac.sh /path/to/aax-sdk-2-x`. The project adds the AAX
   target automatically and leaves `VOXPAL.aaxplugin` in
   `build/VocalChain_artefacts/Release/AAX/`.
2. **PACE signing.** Release Pro Tools refuses unsigned AAX. Avid's developer
   program gives you access to PACE's signing tools (`wraptool`); once you have
   them, sign with
   `wraptool sign --verbose --account <acct> --wcguid <guid> --in "VOXPAL.aaxplugin" --out "VOXPAL.aaxplugin"`
   and copy the bundle to `/Library/Application Support/Avid/Audio/Plug-Ins/`.
   Until it's signed, the unsigned AAX loads only in the Pro Tools Developer
   build that comes with the developer account.

Everything else about the plugin is identical across the three formats.

## Building on other platforms

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8.0.4 is fetched automatically; pass `-DJUCE_DIR=/path/to/JUCE` to use a
local copy. Linux VST3 build verified end to end (clean compile, 4.7 MB bundle, host smoke test passing).

### Tests

Two layers. The DSP suite needs no JUCE; the host smoke test builds a headless
host around the real processor, runs audio through it at 48 and 192 kHz, and
checks meters, bypass and state save/restore (`cmake --build build --target
host_smoke && ./build/host_smoke_artefacts/Release/host_smoke`). Both pass.

### DSP tests (no JUCE needed)

```
g++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test && ./dsp_test        # 48 kHz
./dsp_test 192000                                                       # any rate
```

Checks the HPF slope, the Pultec low-end shape, the 10k and 16k boosts, the
Air Q tracking, FET attack/release, the opto two-stage release, that each
Control band only reacts to its own range, gain-match convergence, and the
sweet-spot state machine.

## Where the models stand

Every stage follows the hardware circuit, not a generic compressor or EQ shape:

- **TOM (1176).** The gain cell is a JFET shunt divider in its ohmic region,
  fed by a full-wave rectified feedback detector with the hardware attack and
  release RC. The 4:1 ratio and the soft knee come out of the loop (measured:
  3.3:1 just above the knee rising to 4.5:1 at the top, which is how a real
  1176 at 4:1 behaves). Calibrated so a -18 dBFS peak at 0 dB drive gives
  ~3 dB of GR. Distortion is the JFET's Rds(Vds) term after the LN
  half-signal cancellation.
- **SPIKE (LA-2A).** Square-law EL panel into a CdS photoresistor with fast and
  slow carrier populations. The ~3:1 slope (measured 3.0:1), the very soft
  knee and the two-stage memory release are all consequences of the cell
  physics; nothing is set by a ratio or knee constant. Attack 10 ms, fast
  release 60 ms to 50 %, slow release 0.5-5 s with exposure.
- **Passive EQ (EQP-1A).** Boost and atten are first-order RC shelves. At the
  "100" setting the boost's upper turnover is ~700 Hz (13 dB max) and the
  atten's is ~1.5 kHz (17 dB max), from the published GroupDIY turnover
  measurements of the hardware. At boost 3 / atten 2 that gives roughly
  +0.9 dB below 100 Hz and a -0.8 dB dip around 1 kHz.
- **10k and Air bells, and the Control bands** use analog-matched biquads
  (Vicanek matched-Z design), so a 16 kHz bell has the same shape at 48 kHz as
  at 192 kHz instead of being squashed against Nyquist.

What is still approximate: the Pultec pot tapers (knob position to dB), the
exact high-boost Q vs bandwidth law, and the point at which the specific UAD
1176AE and LA-2A Gray units sit on their curves. The constants to tune live at
the top of each `Source/dsp/*.h` file.

To close the last gap to your specific UAD instances: capture each plugin at
your settings in Plugin Doctor (frequency response for the EQs; transfer curve,
attack/release envelope and harmonic spectrum for the compressors) and the
constants get fitted to those curves.

Sources used for the circuit models: the UA 1176LN manual and schematic, the
Teletronix LA-2A manual and schematic, T4B cell data sheets, the GroupDIY
EQP-1A turnover analysis, and Simionato & Fasciani (2024) on optical
compressor time behaviour.

## Engineering notes

- Mono in / mono out only, by design (a vocal chain). Logic runs mono plugins
  on stereo tracks as dual-mono automatically; in Live and Pro Tools use it on a
  mono track.
- TOM and SPIKE run at 2x the host rate through a half-band polyphase IIR
  oversampler, so their harmonics don't alias at 48 kHz.
- All seven knobs and Output are smoothed over 20 ms and applied in 32-sample
  sub-blocks; filter coefficients are recomputed only when a value moved.
- Gain Match holds its last makeup value during gaps between phrases instead of
  drifting, so the first word after a pause is at the right level.
- The Control bands update their bell coefficients every 16 samples, not per
  sample, to keep CPU flat during attacks.

## Not in this build yet

- The real UI. `PluginEditor.cpp` is deliberately plain and talks to the
  processor only through parameter attachments, so a skinned editor (filmstrip
  knobs, VU-style meters, textured panel) drops in without touching the DSP.

## Naming

The sections are labelled FET / Opto / Passive EQ / Air. Don't put "1176",
"LA-2A", "Pultec" or "Ozone" on the product; those are other people's marks.
