# Handoff: Chords Theory

Written at the end of the session that built this plugin from scratch, for whoever (human or
another Claude Code session) picks this up next. Read this first; it points at everything else.

## TL;DR

A full MIDI-effect JUCE plugin (Standalone/AU/AUv3/VST3) was built end-to-end from
[`nierika_plugin_template`](https://github.com/halbehers/nierika_plugin_template) per the approved
plan (`Handoff/PLAN.md`). It builds clean (zero warnings, strict flags, Debug + Release), 26/26
Catch2 tests pass (12,495 assertions), and `pluginval` passes against the built AU. The UI was
visually confirmed twice via screenshots the user took (chord browser; full sequencer with a
correctly-resolved Pachelbel progression). **Nothing is committed to git yet** — `git init` was
run but the working tree is untouched, 120 files untracked, ready for review before a first commit.

## Post-handoff fix: crash on key/scale change (nierika_dsp bug)

Found and fixed in the very next session, from live user testing: selecting a new key or scale
would often show the wrong number of degree cards (not always 7) and could crash outright. Root
cause was in **`nierika_dsp`**, not this plugin: `GridLayout::reset()`
(`~/Development/nierika_dsp/source/gui/layout/GridLayout.cpp`) cleared row/column sizing but never
cleared its internal `_itemsById`/`_componentsById` maps. `ChordDegreeBrowser::setKeyAndScale`
destroys the previous key/scale's `ChordCard`s and rebuilds from scratch on every call (`reset()` +
re-populate) — with that bug, the maps kept referencing already-destroyed components (dangling
`juce::Component&`), and since the same identifiers (`"chord-card-I"`.."chord-card-VII"`) are
reused every time, `unordered_map::emplace()` silently no-op'd instead of registering the new
cards. The very next `resized()` (called at the end of `setKeyAndScale` itself) then touched those
dangling references via `GridLayout::replaceAll()`, which iterates every entry unconditionally —
producing both the wrong counts and the crash. Confirmed reproducible with a segfault via a
deliberate before/after test (stashed the fix, reran the test, got a real `SIGSEGV` at a
Key::A/Locrian transition; restored the fix, passed clean).

**Fixed** in `nierika_dsp` (a separate repo, `git@github.com:halbehers/nierika_dsp.git`, currently
**uncommitted** on `main` — review and commit there too): `GridLayout::reset()` now also clears
`_itemsById`/`_componentsById`. Regression test added in this repo:
`Tests/ChordDegreeBrowserTests.cpp` — constructs a `ChordDegreeBrowser` headlessly and drives it
through several key/scale transitions including 7↔3-degree-count swaps (Minor Blues), which is
exactly the scenario that crashed.

**If you're reviewing this on a machine/checkout where `nierika_dsp` isn't already patched**, this
plugin will crash on key/scale changes again — the fix lives in the library, not here.

## Where to look

- **This file** — status, gaps, how to resume.
- **`CLAUDE.md`** (repo root) — the as-built architecture map: what each file/class owns, how the
  drag-and-drop mechanism actually works, how session state round-trips. Read this to understand
  the code, not `PLAN.md`.
- **`Handoff/PLAN.md`** — the pre-implementation design that was approved before writing any code.
  Useful for *why* things are shaped the way they are, and for the full list of decisions the user
  made during planning (voicing algorithm, naming conventions, drag mechanism rationale). **Not
  fully accurate to what shipped** — see "Deviations from PLAN.md" below.
- **`README.md`** — user-facing feature list + build/test commands.

## Deviations from PLAN.md during implementation

The plan was solid but a few things changed once real code met real APIs:

- `MidiExporter`'s methods take `bpm` only (default `120.0`), not `bpm, timeSigNumerator` as
  drafted — time signature is hardcoded 4/4 (`kBeatsPerMeasure = 4`) since nothing in this plugin
  ever varies it.
- `ProgressionPresetFactory::createFromSlots` gained a third parameter, `Scale builtOnScale`, not
  in the original signature — needed so a preset built while on Minor Blues (which only has I/IV/V)
  gets correctly scoped to `applicableScales = {MinorBlues}` on save, rather than silently becoming
  a "works on any scale" preset that would fail to resolve II/III/VI/VII slots later. See
  `Theory/ProgressionPresetFactory.cpp`.
- Session-state persistence turned out simpler than planned: rather than hand-rolling XML
  serialization inside `PluginProcessor.cpp`, `ndsp::ParameterManager::getStateInformation` already
  serializes its *entire* `AudioProcessorValueTreeState::state` tree — so `AppLayout` just splices
  an extra `"ChordsTheoryState"` child node into `_parameterManager.getState().state` directly.
  `PluginProcessor.cpp`'s `getStateInformation`/`setStateInformation` needed **zero changes**. The
  actual (de)serialization logic lives in `Theory/SessionState.h` + `Theory/SessionStateSerializer`
  — pure data, no UI dependency, directly unit-tested (`Tests/SessionStateSerializerTests.cpp`,
  called `ChordsTheoryStateTests.cpp` in the plan).
- `ProgressionPresetLibrary`'s constructor was made public and file-parameterized (mirroring
  `AppSettings`'s pattern) rather than purely private-behind-`getInstance()`, so tests can point it
  at a throwaway temp file instead of the real per-app presets file.
- UI component `ProgressionSlot` was named `ProgressionSlotView` from the start (the plan already
  flagged the naming collision with `theory::ProgressionSlot` and predicted this rename).

## What's NOT done

Be upfront about these with whoever reviews this — none are hidden, but none are finished either:

1. **Icon** — `Packaging/icon.png` is still the template's placeholder icon. No image-generation
   tooling was available this session. Needs real artwork, then
   `Scripts/generate_iconset.sh Packaging/icon.png Packaging`.
2. **Delete-preset UI** — `ProgressionPresetLibrary::deletePreset(id)` exists and is tested
   (`Tests/ProgressionPresetTests.cpp`), but **no UI control calls it**. The plan called for "a
   small delete affordance" next to each user preset in `ProgressionPresetPicker`; only the backend
   half got built. `ProgressionPresetPicker.cpp`/`.h` is where this would go.
3. **EULA files** (`Packaging/{macos,windows}/resources/EULA`) — left as the generic template
   placeholder text intentionally (not something to fabricate) — needs real legal text before any
   real distribution.
4. **Git** — repo initialized, nothing committed. Review the diff (there's no "diff" yet, just
   untracked files — `git status`/`git diff --no-index` against the upstream template if you want
   to see the delta) before the first commit.
5. **Live interactive verification** — everything below was *implemented and unit-tested* but never
   driven interactively, because this session had no GUI-automation/screen-recording access:
   - Actually dragging a chord card onto a real DAW track and confirming the resulting MIDI clip.
   - Dragging a chord from the browser onto an *already-occupied* progression slot to confirm the
     replace-on-drop behavior (unit tests cover the resolution logic, not the live drop gesture).
   - The full state round-trip via an actual DAW project close/reopen, or a Standalone app
     quit/relaunch (JUCE's `StandaloneFilterWindow` auto-saves/restores processor state on
     quit/launch — confirmed by reading its source — but this wasn't watched happen).
   - Multi-DAW compatibility of the external `.mid`-file-drag interpretation (per the plan, this
     isn't perfectly standardized across hosts).
   - Visual polish beyond the two screenshots that were reviewed (chord browser sizing; full
     sequencer with a preset loaded). The save-preset prompt, voicing picker popup, and
     drag-hover states on `ProgressionSlotView` were never seen rendered.

## Known risk areas — worth extra scrutiny on review

- **The ValueTree-splicing persistence trick** (`AppLayout::syncStateToValueTree`/
  `restoreStateFromValueTree`, `CLAUDE.md`'s "Session-state persistence" section): reusing APVTS's
  own state tree to piggyback unrelated data is a real, idiomatic JUCE technique, but it's
  non-obvious and only unit-tested at the pure-data (`SessionStateSerializer`) layer — the
  UI-gathering/UI-applying glue in `AppLayout` itself has no automated test, only compiles-and-
  type-checks confidence.
- **The unified drag-and-drop mechanism** (`CLAUDE.md`'s "Drag-and-drop" section): using the same
  native OS file drag for both DAW-export and in-app slot-drop is clever but was never watched
  working live. If it doesn't behave as expected, `AppLayout::onChordDragStarted`/
  `onSlotFileDropped` and `ProgressionSlotView`'s `FileDragAndDropTarget` implementation are where
  to look.
- **CMake identity values chosen unilaterally** (not blocking, just flagging): `PLUGIN_CODE = "Chrd"`
  was picked as an arbitrary-but-valid 4-char code; `MANUFACTURER_CODE` and `COMPANY_NAME` were kept
  identical to the template's (`Nrka` / `Nierika`) on the reasoning that they identify the
  manufacturer, not the product. Worth a quick confirm that these are actually what should ship.

## How to resume

```sh
cd /Users/sebastienhalbeher/Development/chords_theory

# Build (Debug)
cmake --workflow --preset default

# Full test suite (Catch2 + pluginval against the built AU)
ctest --test-dir build --output-on-failure

# Run the Standalone app
open "build/ChordsTheory_artefacts/Debug/Standalone/Chords Theory.app"

# Release build (also packages an installer)
cmake --workflow --preset release
```

`USE_LOCAL_NIERIKA_DSP` is `ON` in `CMakeLists.txt` — this builds against a local
`~/Development/nierika_dsp` checkout (`v0.6.5`), not a pinned remote release. If that checkout ever
moves or gets deleted, configure will fall back to fetching the pinned remote version automatically
(see the `if (USE_LOCAL_NIERIKA_DSP AND NOT EXISTS ...)` guard near the top of `CMakeLists.txt`).

## Suggested review order

1. `CLAUDE.md` — architecture map, read this before any code.
2. `Code/Include/Theory/` — the data model, in dependency order: `Key.h`/`Scale.h`/`Degree.h`/
   `ChordType.h` (enums) → `NoteName.h`/`Chord.h`/`ScaleDegreeData.h`/`KeyScaleData.h` (structs) →
   `ChordDatabase.h`/`.cpp` (the loader) → `NoteConvertor.h`/`.cpp` (the trickiest logic — the
   octave-placement algorithm) → `MidiExporter.h`/`.cpp` → `ProgressionSlot.h`/`ProgressionPreset.h`/
   `ProgressionPresetFactory.h`/`.cpp`/`BuiltInProgressionPresets.h`/`.cpp`/
   `ProgressionPresetLibrary.h`/`.cpp` → `SessionState.h`/`SessionStateSerializer.h`/`.cpp`.
3. `Tests/` — `ChordDatabaseTests.cpp`, `NoteConvertorTests.cpp`, `MidiExporterTests.cpp`,
   `ProgressionPresetTests.cpp`, `SessionStateSerializerTests.cpp` — read alongside the
   corresponding `Theory/` files above; every one of these was run and passed this session.
4. `Code/Include/Component/` — `KeyScaleSelector` → `ChordCard`/`ChordDegreeBrowser`/
   `VoicingPicker` → `ProgressionSlotView`/`ProgressionDragHandle`/`ProgressionPresetPicker`/
   `SavePresetPrompt`/`ProgressionSequencer`.
5. `Code/Include/AppLayout.h` / `Code/Source/AppLayout.cpp` — the top-level wiring: owns every
   component above, drives `MidiExporter`, owns the in-flight drag map, owns
   `syncStateToValueTree`/`restoreStateFromValueTree`. This is where most of the "does it actually
   fit together" risk lives.
6. `CMakeLists.txt` diff vs. the template (identity vars, MIDI-effect flags, `USE_LOCAL_NIERIKA_DSP`,
   the `chords.json` binary-data entry) and `Code/Include/PluginEditor.h` (added
   `juce::DragAndDropContainer` base).
