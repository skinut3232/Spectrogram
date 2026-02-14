# Spectrogram

A real-time spectrogram audio plugin (VST3) and standalone application built with JUCE and OpenGL.

## Features

- **Real-time FFT analysis** with configurable sizes: 1024, 2048, 4096, 8192
- **GPU-accelerated rendering** via OpenGL with GLSL fragment shaders
- **5 colour maps**: Heat, Magma, Inferno, Grayscale, Rainbow
- **Logarithmic and linear** frequency scaling
- **Interactive hover readout** showing frequency (Hz) and magnitude (dB) at cursor
- **Freeze/pause** to hold the display for inspection
- **Adjustable dynamic range** with dB floor and ceiling sliders
- **Hann and Blackman-Harris** window functions
- **50% and 75% overlap** options
- **Full state persistence** — all settings save/restore with your DAW project
- **Resizable window** with saved dimensions

## System Requirements

- **OS**: Windows 10 or later (x64)
- **GPU**: OpenGL 3.3 compatible graphics card
- **DAW**: Any VST3-compatible host (tested with Standalone)

## Installation

### Quick Install (VST3)

Run `installer\install-vst3.bat` as Administrator. This copies the VST3 bundle to:

```
C:\Program Files\Common Files\VST3\Spectrogram.vst3
```

### Inno Setup Installer

If you have [Inno Setup](https://jrsoftware.org/isinfo.php) installed, compile `installer\Spectrogram.iss` to produce a full installer with VST3 and Standalone components.

### Manual Install

Copy the VST3 bundle:

```
build\SpectrogramPlugin_artefacts\Release\VST3\Spectrogram.vst3
```

to your VST3 plugin folder (typically `C:\Program Files\Common Files\VST3\`).

The standalone executable is at:

```
build\SpectrogramPlugin_artefacts\Release\Standalone\Spectrogram.exe
```

## Controls

| Control | Description |
|---------|-------------|
| **FFT** | FFT size: 1024, 2048, 4096, or 8192 samples |
| **Overlap** | Frame overlap: 50% or 75% |
| **Window** | Window function: Hann or Blackman-Harris |
| **Colour** | Colour map: Heat, Magma, Inferno, Grayscale, Rainbow |
| **Log / Linear** | Toggle between logarithmic and linear frequency scale |
| **Freeze / Resume** | Pause or resume the scrolling display |
| **Floor** | Minimum dB level (controls colour map range) |
| **Ceil** | Maximum dB level (controls colour map range) |

Hover the mouse over the spectrogram to see a crosshair with frequency and dB readout.

## Building from Source

### Prerequisites

- [CMake](https://cmake.org/) 3.22+
- [Visual Studio 2022](https://visualstudio.microsoft.com/) (Build Tools or full IDE, with C++ workload)
- Git

### Build Steps

```bash
git clone --recursive <repo-url>
cd Spectrogram

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target SpectrogramPlugin_VST3
cmake --build build --config Release --target SpectrogramPlugin_Standalone
```

Build outputs:
- VST3: `build/SpectrogramPlugin_artefacts/Release/VST3/Spectrogram.vst3`
- Standalone: `build/SpectrogramPlugin_artefacts/Release/Standalone/Spectrogram.exe`

## Architecture

```
Audio Thread ──► Lock-free FIFO ──► Message Thread Timer ──► FFT Analyser
                                                                  │
                                                          Circular Frame Buffer
                                                                  │
                                              GUI Thread ◄────────┘
                                           (OpenGL Renderer)
```

- **Audio thread**: Mixes input to mono, pushes to lock-free FIFO. Zero allocations, zero blocking.
- **Message thread timer** (60 Hz): Drains FIFO, feeds FFT analyser which produces spectral frames.
- **OpenGL renderer**: Uploads magnitude data as a GL_R32F texture, renders via fragment shader with GPU-side colour mapping and frequency scaling.

## License

All rights reserved.
