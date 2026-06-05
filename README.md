# Guitar Channelizer MIDI FX

Guitar Channelizer MIDI FX is a JUCE/CMake Audio Unit MIDI processor for Logic Pro. It maps incoming guitar-style MIDI notes to string-based MIDI channels, shows a fretboard view, and includes tools for guitaristic voicing, chord display, MIDI file processing, and generated melody output.

The built plugin product name is `Guitarizer`.

## Features

- Logic Pro Audio Unit MIDI FX target
- Live MIDI processing
- Per-string MIDI channel routing for six-string guitar layouts
- Optional inverted channel mapping
- Input-channel-aware processing mode
- Fretboard visualization with selectable themes, shapes, and color modes
- Guitaristic fingering and upper-fret preferences
- Chord latch and live chord naming
- MIDI file load/export workflow
- Generated melody controls with root, scale, density, complexity, range, and legato settings

## Requirements

- macOS
- Logic Pro, or another host that can load Audio Unit MIDI FX processors
- CMake 3.22 or newer
- Xcode command line tools
- JUCE checkout or extracted JUCE folder

## Build

Set `JUCE_DIR` to your JUCE folder when configuring:

```sh
cmake -B build -S . -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
```

If you set `JUCE_DIR` in your CMake cache or environment, this shorter command should work:

```sh
cmake -B build -S .
cmake --build build --config Release
```

## Install for Logic Pro

After a successful build, find the built Audio Unit component under the build artifacts folder, then copy `Guitarizer.component` to:

```sh
~/Library/Audio/Plug-Ins/Components/
```

Restart Logic Pro or rescan Audio Units if Logic does not show it immediately.

In Logic Pro, insert it as a MIDI FX plugin on a software instrument channel strip.

## Usage Notes

- Use live processing to channelize incoming MIDI while playing.
- Use the fretboard controls to bias voicings toward a hand position or upper frets.
- Enable inverted channels if your receiving instrument expects the high string on channel 1 instead of the low string.
- Load a `.mid` or `.midi` file to process existing MIDI, then export the channelized result.
- Generated melody output can be enabled from the plugin UI and follows the selected scale/root/range controls.

## Source Layout

- `CMakeLists.txt` - JUCE Audio Unit MIDI FX build definition
- `Source/PluginProcessor.*` - MIDI processing, parameters, MIDI file export, and generation logic
- `Source/PluginEditor.*` - plugin UI, fretboard view, and controls
- `Source/GuitarFingering.*` - guitar string/fret selection and scoring logic
