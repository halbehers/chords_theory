# Chords Theory

**Chords Theory** is a MIDI-effect JUCE plugin that turns music theory into draggable MIDI: pick a
key and a scale, browse the diatonic chords for every scale degree (most popular voicing shown by
default — click a card to pick a different one), and drag a chord straight into a DAW track as a
one-measure clip anchored near middle C. Build a full chord progression on the sequencer below —
drag chords from the browser into slots, or load a preset (pop, jazz, 12-bar blues, and more) —
and drag the whole thing out as one MIDI clip. Save your own progressions as presets; they persist
across every plugin instance and DAW project on your machine.

Built on [Nierika Plugin Template](https://github.com/halbehers/nierika_plugin_template). Available
as Standalone, AU, AUv3, and VST3.

## Features

- **Key/Scale browser**: 12 keys × 10 scales (Major, Minor, Harmonic/Melodic Minor, the modes, and
  Minor Blues), each scale degree shown with its most popular chord voicing by default.
- **Voicing picker**: click any chord card to swap to a different voicing for that degree (7th,
  9th, sus, inversions, and more, depending on what's diatonically available).
- **Drag to DAW**: drag any chord card onto a MIDI/instrument track to insert it as a one-measure
  clip, closed-voiced near middle C.
- **Progression sequencer**: drag chords from the browser into a 12-slot timeline (dropping onto an
  occupied slot replaces it), load a built-in preset, or save your own — then drag the whole
  progression out as one multi-chord MIDI clip.
- **Session state**: key, scale, every degree's chosen voicing, and the full progression sequence
  round-trip through the plugin's own state, so closing and reopening a DAW project restores
  everything exactly as it was left.
- **Internationalization** (English, French, Spanish, German, Italian, Portuguese) and a
  light/dark theme, both inherited from the template.

## Requirements

- macOS ≥ 14.5 or Windows ≥ 10 (2020)
- CMake ≥ 3.22, [Ninja](https://ninja-build.org/)
- A C++20 compiler (Xcode command line tools)

## Building

Dependencies — [JUCE](https://github.com/juce-framework/JUCE) 8.0.14,
[Catch2](https://github.com/catchorg/Catch2), and Nierika's `nierika_dsp` module — are fetched
automatically via [CPM](https://github.com/cpm-cmake/CPM.cmake) on first configure.
`USE_LOCAL_NIERIKA_DSP` is `ON` in `CMakeLists.txt`, building against a local
`~/Development/nierika_dsp` checkout — flip it `OFF` to use the pinned remote release instead.

```sh
cmake --workflow --preset default  # configure (first run/whenever CMakeLists.txt changes) + build
```

`--workflow` always does the right thing whether `build/` already exists or not (a fresh clone, or
after deleting it) - if you'd rather configure and build as separate steps (e.g. to build
repeatedly without reconfiguring), that still works too, as long as `build/` already exists:

```sh
cmake --preset default          # configure (Debug, Ninja) - only needed once, or after CMakeLists.txt changes
cmake --build --preset default  # build
```

Built plugin bundles land in `build/ChordsTheory_artefacts/Debug/{Standalone,AU,VST3}`. For a
Release build (also produces an installer - see below):

```sh
cmake --workflow --preset release
```

Xcode and Visual Studio project generation is available via the `Xcode`/`vs` presets.

### Installers

A Release build also packages an installer automatically:
`release-build/Packaging/Chords Theory-<version>-macOS.pkg` (AU + VST3, ad-hoc signed unless real
Developer ID credentials are configured in the environment) on macOS, or
`release-build\Packaging\Chords Theory-<version>-Windows.exe` (VST3, requires
[Inno Setup](https://jrsoftware.org/isdl.php)'s `iscc` on `PATH`) on Windows.

## Testing

```sh
ctest --test-dir build
```

Runs the Catch2 unit test suite (chord database parsing, note-to-MIDI conversion, progression
presets, MIDI export, session-state serialization, plus the inherited `AppSettings`/
`PluginProcessor` coverage) plus an end-to-end [pluginval](https://github.com/Tracktion/pluginval)
validation pass against the built AU. See `build/Tests/ChordsTheory_Tests --help` for running a
subset of tests by name or tag.

Manual verification (not automatable): drag-and-drop into an actual DAW track, and the full
state-persistence round-trip across closing/reopening a DAW project — see `CLAUDE.md` for what
each piece is responsible for.

## Continuous integration

`.github/workflows/build_and_test.yml` (inherited unchanged from the template — it's identity-
agnostic) builds a macOS + Windows matrix on every push/PR, runs `ctest`, uploads installers as
workflow artifacts, and publishes a GitHub pre-release on any `v*` tag.

## Project layout

- `Code/Include/Theory`, `Code/Source/Theory` — chord/scale/progression data model, MIDI export,
  session-state serialization. No UI dependency.
- `Code/Include/Component`, `Code/Source/Component` — the chord browser, voicing picker,
  progression sequencer, and the template's inherited settings components.
- `Assets/Data/chords.json` — the chord database (12 keys × 10 scales), bundled as binary data.
- `Assets/Languages` — localization strings (`.lang` files).
- `Tests` — Catch2 unit tests.
- `CMake` — build configuration helpers (dependency fetching, compiler warnings, `pluginval`
  integration).
- `Libs` — CPM-fetched dependencies (JUCE; `nierika_dsp` only if `USE_LOCAL_NIERIKA_DSP` is `OFF`).

---

## Developers

Nierika (`halbehers`).
