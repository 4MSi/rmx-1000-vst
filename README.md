# RMX-1000 Remix Station (VST3 / AU / Standalone)

A hardware-authentic **Pioneer DJ RMX-1000** live DSP remix station built in **C++20 + JUCE 8**, designed for **FL Studio, Ableton Live, Rekordbox, and Live Standalone audio processing**, with automated cloud builds via **GitHub Actions**.

## Features
- **ISOLATOR FX**: True 3-band Linkwitz-Riley 24 dB/oct crossover (`HI`, `MID`, `LOW`, full-kill to `+6 dB`) + **CUT/ADD**, **TRANS/ROLL**, and **GATE/DRIVE** modes.
- **SCENE FX (10 Iconic Effects)**:
  - **BUILD UP (Red)**: `BPF Echo`, `Echo`, `Noise`, `Spiral Up`, `Reverb Up`
  - **BREAK DOWN (Blue)**: `HPF Echo`, `LPF Echo`, `Crush Echo`, `Spiral Down`, `Reverb Down`
  - Giant glowing **Scene FX Knob** + **Sub Parameter 1 & 2**
- **X-PAD FX**:
  - 16-LED capacitive **Touchstrip** (`1/1` to `1/32` beat rolls)
  - 4 synthesized club drum voices (**KICK**, **SNARE**, **CLAP**, **HI-HAT**)
  - **PITCH** (`-12` to `+12` semitones), **OVERDUB** (16-step live looper), and **QUANTIZE**
- **RELEASE FX**:
  - Tactile spring-loaded mechanical lever for **VINYL BRAKE**, **ECHO**, and **BACK SPIN**
- **Live BPM & MIDI**:
  - Automatic DAW Host Sync (`AudioPlayHead`), Standalone **Auto-BPM**, and **Tap Tempo**
  - MIDI Mapping: `CC 1/19` (Scene Knob), `CC 16/17/18` (Iso Low/Mid/Hi), `CC 22` (X-Pad), Notes `36/38/39/42` (Kick/Snare/Clap/Hat), Note `48` (Release Lever)
  - Keyboard Shortcuts: `Space` (Hold Release Lever), `T` (Tap Tempo), `1-4` (Drum Pads)

## Installation (After GitHub Actions Build)
1. Open the **Actions** tab in your GitHub repository and download `RMX-1000-Windows-VST3-Standalone`.
2. Copy `RMX-1000.vst3` into `C:\Program Files\Common Files\VST3\`.
3. Rescan plugins in **FL Studio / Ableton Live / Rekordbox**, or run `RMX-1000.exe` directly for Live Standalone use.
