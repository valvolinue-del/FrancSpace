# FrancSpace

FrancSpace is a sidechain spectral carve (VST3 and Standalone) built with JUCE 8 and C++20.

Put it on the track you want to make room in — a music bus, a guitar, a pad. Feed the priority signal, usually a vocal, into the sidechain. FrancSpace watches that sidechain in 32 log-spaced bands from 20 Hz to 20 kHz and turns the same bands down on the main input, only while the sidechain is occupying them. It is the same idea as a multiband sidechain compressor, with a band for every third of an octave instead of four or five broad ones.

With no sidechain routed, and the detector left on External, the plug-in is transparent apart from its fixed spectral latency. The display reads **No sidechain**. Standalone has no sidechain input; switch the detector to **Internal** to carve the track against itself while you audition.

## How the carve works

Detection and processing share one 2048-point STFT, a periodic Hann window, and a hop of 256 samples (8x overlap). The overlap-add is scaled so a flat gain of 1 reconstructs the input. Latency is 2047 samples at every sample rate (about 43 ms at 48 kHz, 21 ms at 96 kHz) and it does not change when you bypass, so a host delay compensation stays valid.

Each FFT bin is grouped into the nearest of 32 bands. The band's level is the loudest bin in it, compared with a full-scale sine. A one-pole envelope (attack and release) follows that level. Depth reaches the Amount control once the band is about 6 dB below full scale, and Amount at 100% is 24 dB of reduction. Sidechain energy below Low Cut or above High Cut is ignored. The resulting gains are applied to the main signal in the same frame, so the carve lines up with the sidechain that caused it.

Three stereo modes share the same timing:

| Mode | What it does |
| --- | --- |
| Linked | The louder sidechain channel sets one curve, applied to both main channels. |
| Left/Right | Each main channel follows its own sidechain channel. A mono sidechain is copied to both. |
| Mid/Side | The sidechain is split into mid and side, and those curves are applied to the main mid and side. |

Amount, the cuts, the trims, mix, and bypass are smoothed. Band gains move with the attack and release envelopes. The audio thread does not allocate and does not take a lock. The display reads atomics written at the end of each block and repaints at 60 Hz.

## Routing the sidechain

The plug-in exposes a stereo **Sidechain** aux input. It is off until the host enables it. A mono send is accepted too.

### Ableton Live

1. Put FrancSpace on the music track.
2. In the device title bar, open the sidechain section and enable **Sidechain**.
3. Set Audio From to the vocal track, and tap to Post Mixer (or Pre FX if you want it before the vocal's own processing).

### Logic Pro

1. Put FrancSpace on the music track.
2. In the plug-in header, open the sidechain menu and choose the vocal track.

### FL Studio

1. Put FrancSpace on the music mixer track.
2. On the vocal track, right-click the send arrow to the music track and choose **Sidechain to this track**.
3. In the plug-in wrapper, enable the sidechain input if FL has not already.

### Reaper

1. Put FrancSpace on the music track.
2. Open the plug-in's pin connector and add the extra input channels (3/4).
3. Send the vocal track to the music track on channels 3/4.

Turn **Amount** up until the vocal has the space it needs. Pull **Low Cut** up if the vocal's proximity effect is digging a hole in the bass, and pull **High Cut** down to leave the air band alone. **SC Gain** is a detector trim for a quiet sidechain. **Mix** blends the dry (latency-aligned) signal back in.

## Parameters

| ID | Name | Range | Default |
| --- | --- | --- | --- |
| `amount` | Amount | 0–100% | 50% |
| `lowCut` | Low Cut | 20 Hz–20 kHz, logarithmic | 20 Hz |
| `highCut` | High Cut | 20 Hz–20 kHz, logarithmic | 20 kHz |
| `attack` | Attack | 0.1–200 ms | 5 ms |
| `release` | Release | 5–2000 ms | 80 ms |
| `sidechainGain` | Sidechain Gain | -24 to +24 dB | 0 dB |
| `outputGain` | Output | -24 to +24 dB | 0 dB |
| `mix` | Mix | 0–100% | 100% |
| `mode` | Mode | Linked, Left/Right, Mid/Side | Linked |
| `detector` | Detector | External, Internal | External |
| `bypass` | Bypass | off / on | off |

State is saved and restored through the parameter tree. Knobs reset to these defaults on double-click. Shift or Cmd while dragging is a finer motion, the scroll wheel nudges the value, and a popup shows the value during a drag. The low-cut and high-cut regions on the display are handles: drag the shaded edge or the gold diamond, scroll while hovering it, double-click to reset.

## Build

Requirements: CMake 3.22 or newer, a C++20 compiler, and a network connection the first time you configure (CMake fetches JUCE 8.0.15). To build against a JUCE checkout you already have, pass `-DFRANCSPACE_JUCE_PATH=/path/to/JUCE` instead.

Web browser and curl support in JUCE are turned off, which avoids the Linux webkit dependency.

The Standalone icon is `Resources/Icon.png` (1024×1024), wired with `ICON_BIG` and `ICON_SMALL` so the macOS and Windows app bundles pick it up.

### Linux

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libasound2-dev libfreetype6-dev libfontconfig1-dev libx11-dev libxext-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxrender-dev libxcomposite-dev \
  libgl1-mesa-dev libglu1-mesa-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`libfreetype6-dev` supplies the `libfreetype-dev` dependency JUCE links against, and `libfontconfig1-dev` is required alongside it. If `c++` on your machine is not GCC and cannot link `libstdc++`, configure with `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`.

### macOS

Install Xcode (or the Command Line Tools) and CMake, then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release
```

The Standalone app can be launched directly. Copy `FrancSpace.vst3` to `~/Library/Audio/Plug-Ins/VST3` when you want it in a host.

### Windows

From a Visual Studio x64 developer prompt, with the Desktop development with C++ workload installed:

```bat
cmake -B build -A x64
cmake --build build --config Release
```

Copy the `FrancSpace.vst3` bundle to `C:\Program Files\Common Files\VST3`. The Standalone target is `FrancSpace.exe`.

### Artefacts

After a Release build:

- VST3: `build/FrancSpace_artefacts/Release/VST3/FrancSpace.vst3`
- Standalone: `build/FrancSpace_artefacts/Release/Standalone/FrancSpace` (`.exe` on Windows, `.app` on macOS)

The plug-in is not copied into the system plug-in folders automatically.

### Sanity test

`FrancSpaceSanity` processes audio offline: no sidechain must match a delayed copy of the input, a 1 kHz sidechain sine must attenuate that band on tones and on white noise and leave 100 Hz and 10 kHz nearly alone, Amount 0 and bypass and mix 0 must stay transparent, the low and high cuts must be respected, output must stay finite across sample rates and block sizes, and a saved state must restore.

```bash
cmake --build build --config Release --target FrancSpaceSanity
./build/FrancSpaceSanity_artefacts/Release/FrancSpaceSanity
./build/FrancSpaceSanity_artefacts/Release/FrancSpaceSanity \
  --snapshot /tmp/francspace-wide.png \
  --snapshot-compact /tmp/francspace-compact.png
```

Pass `-DFRANCSPACE_BUILD_TESTS=OFF` to skip that target. The snapshot flags need a display (or Xvfb) because they open the editor offscreen. The wide shot is 1280×800 with the internal detector running; the compact shot is 980×640 with no sidechain.

## Continuous integration

`.github/workflows/ci.yml` builds a Windows MSVC x64 package and a macOS universal package (arm64 + x86_64, deployment target 11.0) and uploads `FrancSpace-windows-x64` and `FrancSpace-macos-universal`. A tag named `v*` also publishes those zip files as a GitHub Release.

Developer ID signing and notarization run only when all six secrets are set:

- `APPLE_CERTIFICATE_P12_BASE64`
- `APPLE_CERTIFICATE_PASSWORD`
- `APPLE_SIGNING_IDENTITY`
- `APPLE_ID`
- `APPLE_APP_SPECIFIC_PASSWORD`
- `APPLE_TEAM_ID`

If none of them are set, the macOS artifact is unsigned. If only some are set, the job fails rather than shipping a half-signed build.

## Layout

```
CMakeLists.txt
Resources/Icon.png
Source/PluginProcessor.h
Source/PluginProcessor.cpp
Source/PluginEditor.h
Source/PluginEditor.cpp
Source/SpaceDisplay.h
Source/SpaceDisplay.cpp
Source/SpectralEngine.h
Source/SpectralEngine.cpp
Source/Palette.h
tests/Sanity.cpp
```

## Limitations

The STFT latency is fixed at 2047 samples. That is the cost of a 2048-point window; it is reported to the host and the dry path is delayed by the same amount. Bands below about 80 Hz share FFT bins, so the bottom of the display is coarser than the mids. Attack times shorter than one hop (256 samples, about 5 ms at 48 kHz) cannot get any faster than that hop. The carve is minimum-phase only in the sense that each frame keeps the original bin phase; it is not a linear-phase EQ, and a strong Amount on a dense sidechain can still sound like a moving comb if the sidechain is noisy. Internal mode feeds the main input to the detector, so it ducks the track against itself rather than against a separate source. Linked mode uses the louder sidechain channel in each band, not a summed mid.

JUCE itself is covered by the [JUCE licence](https://juce.com/legal/juce-8-licence/) (or AGPLv3). This plug-in's manufacturer code is `Frnc` and its plug-in code is `FrSp`.
