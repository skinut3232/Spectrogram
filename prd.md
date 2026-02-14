# Spectrogram — Product Requirements Document

## Overview

Spectrogram is a real-time audio spectrum analyser plugin (VST3) and standalone application for music producers, sound designers, and audio engineers. It provides GPU-accelerated visualisation of audio frequency content with advanced features including bloom effects, stereo spatial analysis, and real-time spectrum overlays.

**Current Version**: 2.1
**Platform**: Windows 10+ (x64)
**Format**: VST3, Standalone
**Framework**: JUCE 8.0.6, OpenGL 3.3

---

## Target Users

- **Music Producers** — Visual feedback on mix frequency balance while producing in a DAW
- **Mixing/Mastering Engineers** — Precision frequency analysis with zoom, peak hold, and RTA
- **Sound Designers** — Stereo field visualisation via Nebula mode
- **Educators/Students** — Learning tool for understanding frequency content and stereo imaging

---

## Core Features

### 1. Real-Time Spectrogram Display

| Requirement | Detail |
|---|---|
| FFT Sizes | 1024, 2048, 4096, 8192 samples |
| Window Functions | Hann, Blackman-Harris |
| Overlap | 50%, 75% |
| Frequency Scale | Logarithmic or linear, toggle |
| Dynamic Range | Adjustable floor (-120 to -20 dB) and ceiling (-30 to +10 dB) |
| Rendering | GPU-accelerated via OpenGL fragment shader |
| Frame Rate | 60 Hz display update |
| Scrolling | Time-scrolling waterfall display, newest data at right edge |

### 2. Colour Maps (8 total)

| Map | Description |
|---|---|
| Heat | Blue → Cyan → Yellow → Red → White |
| Magma | Black → Purple → Magenta → Orange → Yellow |
| Inferno | Black → Purple → Red → Orange → Yellow |
| Grayscale | Black → White |
| Rainbow | Violet → Blue → Cyan → Green → Yellow → Red |
| Viridis | Dark purple → Teal → Yellow-green |
| Plasma | Dark blue → Purple → Magenta → Orange → Yellow |
| Turbo | Dark blue → Cyan → Green → Yellow → Red |

All maps implemented as 4-segment piecewise linear interpolation, both CPU-side (ColourMap.h) and GPU-side (GLSL).

### 3. Frequency Zoom

| Requirement | Detail |
|---|---|
| Range | 20 Hz to 20,000 Hz |
| Controls | Lo and Hi sliders with logarithmic skew (midpoint 1000 Hz) |
| Behaviour | Shader, axis labels, and hover readout all zoom-aware |
| Persistence | Zoom range saved/restored with DAW project |

### 4. Peak Hold Mode

| Requirement | Detail |
|---|---|
| Tracking | Per-bin maximum dB value |
| Decay | Configurable rate (5–60 dB/sec) |
| Display | Bright yellow line overlay on spectrogram |
| Toggle | "Peak" button, disabled in Nebula mode |

### 5. RTA Spectrum Overlay

| Requirement | Detail |
|---|---|
| Data Source | Current (most recent) FFT frame |
| Display | Semi-transparent cyan filled curve + stroke |
| Orientation | Y axis = frequency (zoom-aware), X position = magnitude |
| Toggle | "RTA" button, disabled in Nebula mode |

### 6. Bloom / Glow Effect

| Requirement | Detail |
|---|---|
| Pipeline | 4-pass FBO: scene → bright extract → ping-pong Gaussian blur (3 iterations) → composite |
| Resolution | Blur passes at half resolution for performance |
| Controls | Toggle button + intensity slider (0–2.0) |
| Default | Threshold 0.3, intensity 0.8 |
| Safety | Saves/restores JUCE's FBO binding; FBOs recreated on resize in GL thread only |

### 7. Nebula Stereo Spatial Mode

| Requirement | Detail |
|---|---|
| Analysis | Separate FFT on left and right channels |
| Pan Calculation | `pan = (magR - magL) / (magR + magL)` per bin |
| Display | 2D accumulation texture (256 x 512): X = stereo pan, Y = frequency |
| Colouring | Frequency-based rainbow (low = red, mid = green, high = blue) |
| Decay | Per-frame multiplicative decay (0.96) on accumulation buffer |
| Splat | Gaussian-ish energy spread (3x5 kernel) at (pan, freq) positions |
| Axis | L — C — R labels on X axis; frequency on Y axis |
| Constraints | Peak hold and RTA disabled in Nebula mode |

### 8. Hover Readout

| Requirement | Detail |
|---|---|
| Display | Crosshair lines + tooltip box at cursor |
| Content | Frequency (Hz or kHz) and magnitude (dB) |
| Behaviour | Zoom-aware frequency mapping; auto-repositions to stay in bounds |

### 9. Freeze / Pause

| Requirement | Detail |
|---|---|
| Behaviour | Stops consuming FFT frames; display holds at last state |
| Toggle | "Freeze" / "Resume" button |

---

## User Interface

### Layout

```
┌─────────────────────────────────────────────────────────────────┐
│ Row 1: FFT | Overlap | Window | Colour | Log | Freeze | Fl/Ceil│
│─────────────────────────────────────────────────────────────────│
│ Row 2: Mode | Bloom [slider] | Peak [slider] | RTA | Lo/Hi    │
├────┬────────────────────────────────────────────────────────┬───┤
│    │                                                        │   │
│ Hz │              Spectrogram / Nebula                      │dB │
│    │              (OpenGL rendered)                          │   │
│    │                                                        │   │
├────┴────────────────────────────────────────────────────────┴───┤
│                        Time axis / L C R                        │
└─────────────────────────────────────────────────────────────────┘
```

### Theme

| Element | Value |
|---|---|
| Background (dark) | #12121a |
| Background (medium) | #1e1e2e |
| Background (controls) | #2a2a3e |
| Accent | #6366f1 (indigo) |
| Text primary | #e2e8f0 |
| Text secondary | #94a3b8 |
| Border | #3f3f5a |
| Corner radius | 4px on all controls |

### Window

| Property | Value |
|---|---|
| Default size | 900 x 520 |
| Minimum size | 700 x 400 |
| Maximum size | 1920 x 1080 |
| Resizable | Yes, with saved dimensions |

---

## Architecture

```
Audio Thread (DAW processBlock)
    │
    ├── Mono mix ──► AudioFifo (lock-free) ──► SpectralAnalyser
    │
    └── L/R push ──► StereoFifo L/R ──► StereoSpectralAnalyser
         (only when Nebula active)

Message Thread Timer (60 Hz)
    │
    ├── Drains FIFOs → pushes samples to analysers
    │
    └── Editor timerCallback()
         ├── Pulls FFT frames → writes texture columns
         ├── Updates peak hold data (decay)
         ├── Updates nebula accumulation texture
         └── Triggers GL repaint

OpenGL Thread (renderOpenGL)
    │
    ├── Uploads texture data (double-buffered, atomic flag)
    ├── Standard path: spectrogram shader → screen
    ├── Bloom path: scene FBO → bright extract → blur → composite
    └── Nebula path: nebula shader → screen
```

### Thread Safety

| Mechanism | Purpose |
|---|---|
| `AudioFifo` (lock-free FIFO) | Audio thread → message thread data transfer |
| `std::atomic<bool> textureNeedsUpload` | Message thread → GL thread upload signal |
| Double-buffered `textureDataFront` / `textureDataBack` | Concurrent read/write without locks |
| `std::atomic<bool> nebulaActive` | Editor → processor nebula mode flag |
| Atomic frame buffer read/write positions | Analyser → editor frame passing |

---

## State Persistence

All settings serialised to XML via `getStateInformation` / `setStateInformation`:

- FFT size, overlap, window type
- Colour map, log scale
- dB floor / ceiling
- Zoom min / max frequency
- Peak hold enabled + decay rate
- RTA enabled
- Bloom enabled + intensity + threshold
- Nebula mode
- Editor window dimensions

---

## File Structure

```
src/
├── PluginProcessor.h/.cpp         Audio processor, FIFOs, state management
├── PluginEditor.h/.cpp            UI, OpenGL rendering, all controls
├── SpectralAnalyser.h/.cpp        Mono FFT engine
├── StereoSpectralAnalyser.h/.cpp  Stereo FFT + pan analysis (Nebula)
├── AudioFifo.h                    Lock-free circular audio buffer
├── ColourMap.h                    8 colour map implementations
└── CustomLookAndFeel.h/.cpp       Dark theme UI styling
```

---

## Build & Install

### Prerequisites

- CMake 3.22+
- Visual Studio 2022 (C++ workload)
- Git

### Build

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target SpectrogramPlugin_VST3
cmake --build build --config Release --target SpectrogramPlugin_Standalone
```

### Install

Run `install.bat` as Administrator (copies VST3 to `C:\Program Files\Common Files\VST3\`).

---

## Version History

| Version | Description |
|---|---|
| v1.0 | Core spectrogram: FFT engine, OpenGL rendering, 5 colour maps, log scale, hover readout, freeze, settings persistence |
| v2.0 | Bloom/glow, Nebula stereo mode, peak hold, RTA overlay, frequency zoom, 3 new colour maps, custom dark UI theme, two-row control bar |
| v2.1 | Added VST3 install script |

---

## Future Considerations

- macOS / Linux builds (AU, VST3)
- Resizable font / DPI-aware scaling
- Configurable bloom threshold slider in UI
- Nebula mode with bloom support
- MIDI note overlay on frequency axis
- Screenshot / export to image
- Sidechain input for comparison analysis
- Smoothed RTA with adjustable averaging
