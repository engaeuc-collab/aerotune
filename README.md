# AeroTune

A real-time, stereo vocal auto-tune **VST3** plugin (plus a standalone app) with a **Frutiger Aero** interface:
glossy glass panels, aqua orbs, sky, sun, rainbow, glass skyline, grass and floating bubbles - all drawn procedurally
in code, so there are no image assets in the repo.

Workflow is in the spirit of Waves Tune Real-Time: pick a key and scale, click individual notes on or off,
and dial in how fast the voice snaps to the nearest allowed note.

> AeroTune is an independent open-source project. It is not affiliated with or endorsed by Waves Audio.

## Controls

| Control | What it does |
|---|---|
| **Key / Scale** | Lights the notes of the chosen scale (Chromatic, Major, Natural/Harmonic Minor, Major/Minor Pentatonic, Blues, Dorian, Mixolydian). |
| **Note keys (C ... B)** | Click to allow / mute any pitch class as a correction target. Edit the scale by hand. |
| **Range** | Restricts pitch tracking to a vocal range to avoid octave errors. |
| **Retune Speed** | 0 ms = instant "hard tune" effect; ~20 ms = natural; 100-400 ms = gentle drift that preserves vibrato. |
| **Amount** | Strength of the correction (100% = fully on the note). |
| **Detune** | Fixed offset in cents added to the target. |
| **A4 Reference** | 430-450 Hz tuning reference. |
| **Mix / Output** | Dry/wet and output trim. |

The display shows the detected note, the output note, a cents meter, and a scrolling pitch roll
(light line = input, green line = corrected output).

## How it works

All DSP lives in `Source/DSP/` and has **no JUCE dependency**:

* `PitchDetector.h` - YIN pitch tracker on a ~11 kHz decimated copy of the mono sum, analysed every ~6 ms. Allocation-free.
* `DelayLine.h` - circular buffer with 4-point Hermite interpolation.
* `AutoTuneEngine.h` - note targeting with hysteresis, retune-speed smoothing, and a pitch-synchronous two-tap granular
  shifter (window = 2k pitch periods, so the grains stay phase-coherent). Both channels share **one** pitch track and **one**
  shift ratio, so the stereo image never wobbles. Unvoiced sounds (breaths, "s", "t") fade to a delay-matched dry signal.

Reported latency is about 10 ms (the actual path delay is roughly 7-25 ms depending on the pitch of the voice).

## Build

Requirements: CMake 3.22+, a C++17 compiler. JUCE 8 is downloaded automatically by CMake.

```bash
git clone <your repo> && cd AeroTune
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target AeroTune_VST3 --parallel
```

The plugin appears at `build/AeroTune_artefacts/Release/VST3/AeroTune.vst3`. Copy it to:

* **Windows:** `C:\Program Files\Common Files\VST3\`
* **macOS:** `~/Library/Audio/Plug-Ins/VST3/`  (unsigned builds may need `xattr -cr AeroTune.vst3`)
* **Linux:** `~/.vst3/`

Add `-DAEROTUNE_COPY_AFTER_BUILD=ON` to have CMake copy it for you. Build the `AeroTune_Standalone` target for a standalone app
(use headphones - it is a live input).

Linux needs the usual JUCE packages (ALSA, X11, freetype, fontconfig, ...) - see `.github/workflows/build.yml`.

### CI

`.github/workflows/build.yml` builds Windows, macOS (universal) and Linux on every push and uploads the VST3s as artifacts.
Pushing a tag like `v1.0.0` also publishes them to a GitHub Release.

### DSP self-test

```bash
g++ -std=c++17 -O2 -ISource/DSP tests/engine_test.cpp -o engine_test && ./engine_test
```

Synthesises voices, runs them through the engine at 44.1/48/96 kHz and prints the measured output pitch.

## Using it well

* Set **Key/Scale** to the song, **Range** to your voice, and start with **Retune Speed** around 10-30 ms.
* Use **0 ms** and a chromatic or pentatonic scale for the hard-tuned effect.
* Track a **single voice** (monophonic). Chords or backing-vocal stacks on one track will confuse any pitch tracker.
* Use a pop filter / de-esser upstream; breath and plosive noise are treated as unvoiced and passed through.

## Known limitations / roadmap

* No formant preservation (large shifts get "chipmunk" colour; typical tuning shifts of < 1 semitone are fine).
* Monophonic only.
* Scale/Key selection writes the note keys when you change it in the UI; automating Key/Scale from the host does not
  re-light the notes (automate the individual note buttons instead).
* Ideas: formant-preserving shifter, MIDI-note target input, vibrato/humanize, resizable UI, presets.

## License

JUCE 8 is dual-licensed (AGPLv3 / commercial). If you distribute AeroTune built against JUCE under the AGPL, this project
must also be released under **AGPL-3.0** (add a `LICENSE` file - on GitHub: *Add file > Create new file > name it LICENSE >
Choose a license template*). If you want to ship closed-source, you need a JUCE commercial license.
