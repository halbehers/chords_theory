# Chords Theory

**Chords Theory** is a JUCE synth plugin that turns music theory into both sound and draggable MIDI:
pick a key and a scale, browse the diatonic chords for every scale degree (most popular voicing shown
by default — click a card to pick a different one), and preview or drag them straight into a DAW. A
full piano-roll MIDI editor builds a chord progression bar by bar — drop chords in, move/resize/
duplicate notes by hand, loop a region, and drag the whole progression (or any single chord within
it) out as a MIDI clip. A built-in dual-oscillator synth with its own filter/envelope/LFO/mixer
section lets you preview everything through actual sound rather than silent drag-and-drop. Undo/redo
covers every one of these surfaces at once.

Built on [Nierika Plugin Template](https://github.com/halbehers/nierika_plugin_template). Available
as Standalone, AU, AUv3, and VST3.

<img width="1144" height="746" alt="Screenshot 2026-08-09 at 16 31 08" src="https://github.com/user-attachments/assets/aea58952-d873-44c7-bf23-07883e6439bd" />


## Features

### Chords & progressions
- **Key/Scale browser**: 12 keys × 10 scales (Major, Minor, Harmonic/Melodic Minor, the modes, and
  Minor Blues), each scale degree shown with its most popular chord voicing by default.
- **Voicing picker**: click any chord card to swap to a different voicing for that degree (7th,
  9th, sus, inversions, and more, depending on what's diatonically available).
- **Drag to DAW**: drag any chord card onto a MIDI/instrument track to insert it as a one-measure
  clip, closed-voiced near middle C.
- **MIDI editor**: a scrollable/zoomable piano roll — drop chords onto it bar by bar, then move,
  resize, or delete individual notes directly. A read-only "chord lane" along the bottom continuously
  re-detects and re-labels whatever chord each group of notes currently spells, so hand-edited notes
  never go stale. Drag a selection while holding Shift and release with Shift still down to duplicate
  it in place; a resizable loop region drives playback from either tab.
- **Drag out individually or as a whole**: drag the whole progression out as one multi-chord MIDI
  clip via the "drag it out" handle, or drag any single chord segment — from the piano roll's chord
  lane, or the Synth tab's mini timeline — out on its own.
- **Presets**: load a built-in preset (pop, jazz, 12-bar blues, and more) or save your own; presets
  persist across every plugin instance and DAW project on your machine.

### Synth
A full built-in synthesizer for previewing chords/progressions as actual sound — sound design here is
optional and independent of the drag-to-DAW MIDI workflow:
- **Two oscillators + sub-oscillator**, each with Sine/Triangle/Saw/Square shapes, unison, detune,
  octave/transpose, warp/fold, and phase controls.
- **Five mixing algorithms** between the oscillators: Add, FM, Ring Mod, AM, and Serial Fold.
- **6-stage envelope** (delay/attack/hold/decay/sustain/release), a **filter** (low/high/band-pass,
  12 or 24 dB/octave, resonance, drive, key tracking), and an **LFO** (4 shapes, tempo-synced or free
  rate, trigger-per-note or free-running, smoothing).
- **Output section**: tuning reference (A4 Hz), pan, master compressor, and level trim.
- A live "what's playing right now" mini timeline sits in the Synth tab's header so the current
  chord — draggable out on its own — stays visible without switching back to the Chords tab.

### History
- **Undo/redo** (Cmd+Z / Cmd+Shift+Z on macOS, Ctrl+Z / Ctrl+Shift+Z elsewhere, or the
  Previous/Next header buttons) covers every edit surface at once — synth parameters, MIDI editor
  notes, and key/scale/voicing changes — coalesced into one step per edit gesture rather than one
  per intermediate value.

### Everything else
- **Session state**: key, scale, every degree's chosen voicing, and the full MIDI editor content
  round-trip through the plugin's own state, so closing and reopening a DAW project restores
  everything exactly as it was left.
- **Settings window**: audio input/output device selection (Standalone only), visual theme
  (light/dark), and language.
- **Internationalization** (English, French, Spanish, German, Italian, Portuguese), inherited from
  the template.

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

Runs the Catch2 unit test suite — chord database parsing, note-to-MIDI conversion, progression
presets, MIDI export, session-state serialization, the MIDI editor's gesture/state logic (drag/
resize/duplicate/undo-redo), the synth DSP (oscillators, sub, envelope, filter, LFO, master
compressor), and the inherited `AppSettings`/`PluginProcessor` coverage — plus an end-to-end
[pluginval](https://github.com/Tracktion/pluginval) validation pass against the built AU. See
`build/Tests/ChordsTheory_Tests --help` for running a subset of tests by name or tag.

Manual verification (not automatable): drag-and-drop into an actual DAW track, and the full
state-persistence round-trip across closing/reopening a DAW project — see `CLAUDE.md` for what
each piece is responsible for.

## Continuous integration

`.github/workflows/build_and_test.yml` (inherited unchanged from the template — it's identity-
agnostic) builds a macOS + Windows matrix on every push/PR, runs `ctest`, uploads installers as
workflow artifacts, and publishes a GitHub pre-release on any `v*` tag.

## Project layout

- `Code/Include/Theory`, `Code/Source/Theory` — chord/scale/progression data model, MIDI export,
  session-state serialization, and the undo/redo `HistoryManager`. No UI dependency.
- `Code/Include/Component`, `Code/Source/Component` — the chord browser, voicing picker, the
  progression editor/MIDI piano-roll editor, the Synth tab's sections (oscillator, sub, mixer,
  envelope, filter, LFO, output), and the settings window (input/output/visual/language).
- `Assets/Data/chords.json` — the chord database (12 keys × 10 scales), bundled as binary data.
- `Assets/Languages` — localization strings (`.lang` files).
- `Tests` — Catch2 unit tests.
- `CMake` — build configuration helpers (dependency fetching, compiler warnings, `pluginval`
  integration).
- `Libs` — CPM-fetched dependencies (JUCE; `nierika_dsp` only if `USE_LOCAL_NIERIKA_DSP` is `OFF`).

---

## Developers

Nierika (`halbehers`).
