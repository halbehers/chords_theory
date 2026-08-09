# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Chords Theory** is a JUCE MIDI-effect plugin (Standalone / AU / AUv3 / VST3) that helps a user
browse the diatonic chords of a key/scale and drag them straight into a DAW as MIDI. Built on
[Nierika Plugin Template](https://github.com/halbehers/nierika_plugin_template) — the CI/build/
test/packaging infrastructure, i18n mechanism, and themed `AppLayout`/`SettingsWindow` shell are
inherited from there unchanged; everything under `Code/Include/Theory` and the chord/progression
components under `Code/Include/Component` are this plugin's own domain logic on top of it.

## Build / Tests

Same CMake presets, CPM dependency fetching, Catch2 + `pluginval` CTest wiring as the template —
see `README.md` for exact commands (`cmake --workflow --preset default`, `ctest --test-dir build`).
`USE_LOCAL_NIERIKA_DSP` is `ON` (see `CMakeLists.txt`), building against the local
`~/Development/nierika_dsp` checkout rather than a pinned remote release.

## Architecture

**Data layer** (`Code/Include/Theory`, `Code/Source/Theory`) — one structure per header:

- `Key`, `Scale`, `Degree`, `ChordType` — enums mirroring `Assets/Data/chords.json`'s vocabulary,
  each with `get*JsonKey`/`get*Label` (forward) and `parse*` (reverse) string conversions.
- `NoteName`, `Chord`, `ScaleDegreeData`, `KeyScaleData` — the parsed chord database's shape.
  `Chord::notes` preserves chords.json's array order (bass-first for inversions);
  `ScaleDegreeData::chords` is sorted ascending by `popularityOrder` at load time, so `chords[0]`
  is always the default (most popular) voicing. `Scale::MinorBlues` is structurally different —
  6 scale notes and only degrees I/IV/V populated (no II/III/VI/VII); code touching `degrees` size
  or degree lookup must account for this (see `KeyScaleData::findDegree`, which returns `nullptr`
  for absent degrees rather than asserting).
- `ChordDatabase` — parses the bundled `chords.json` (via `juce_add_binary_data`, same mechanism
  as the `.lang` files) once into a process-wide singleton (`getInstance()`), indexed flat by
  `key*10 + scale` for O(1) lookup.
- `NoteConvertor` — stateless: `parsePitchClass` (note-name string → 0-11) and
  `voiceChordCloseToMiddleC` (pitch classes in chord-array order → ascending MIDI notes, closed
  voicing, lowest note at/below middle C — the first array entry becomes the bass, which correctly
  handles inversions since bass-first order is preserved from the source data).
- `ChordIdentifier` — the reverse of the above: stateless `identify(midiNotes, key, scale)` matches a
  set of sounding MIDI notes against `ChordDatabase::get(key, scale)`'s full chord list, keyed on
  `(bass pitch class, full pitch-class set)` — confirmed unique within any single Key/Scale's chord
  list, so at most one match is possible; deliberately does not fall back to searching other
  keys/scales. Backs `MidiEditor`'s auto-detected chord lane (see below) — there is no forward→
  backward ambiguity to resolve beyond picking the right Key/Scale context.
- `MidiExporter` — writes temporary Standard MIDI Files (120bpm/4-4 fallback, 960 ticks/quarter).
  `writeSingleChordMidiFile`/`writeProgressionMidiFile` place chords on a rigid one-measure grid
  (the latter still backs single-chord `ChordCard` drags and remains directly unit-tested, but has
  no live caller left in the app); `writeMidiEditorContentFile` writes each note at its own exact
  `startBeat`/`lengthBeats` position (beats → ticks via `kTicksPerQuarterNote`, no quantization) -
  this is what "Drag it out" actually uses, so the exported MIDI matches the piano roll exactly.
- `Audio::ChordSynthEngine`/`Audio::ProgressionPlayer` — the actual synth. `ChordSynthEngine` bridges
  UI-thread "preview this chord" clicks into the audio-thread `juce::Synthesiser` via
  `juce::MidiKeyboardState`. `ProgressionPlayer` (owned by it) is the sample-accurate loop-playback
  scheduler behind `MidiEditor`'s play/pause button - injects real note-on/off `juce::MidiMessage`s
  directly into the same block `ChordSynthEngine::renderNextBlock` renders, at the exact sample
  offset each note falls on (host tempo when available, `kFallbackBpm` otherwise). Message thread
  (`setNotes`/`setLoopBounds`/`play`/`stop`) and audio thread (`renderNextBlock`) hand off the note
  list through a fixed-capacity, wait-free double-buffer - no allocation or locking on either side,
  see its own class doc comment for the full cross-thread contract.
- `ProgressionSlot`/`ProgressionPreset`/`ProgressionPresetFactory`/`BuiltInProgressionPresets` —
  presets reference degrees generically (not pinned chord types) plus an optional `popularityOrder`
  pin. `ProgressionEditor::loadPreset` resolves each slot against the live per-degree voicing (or
  the pin) and places the result at one bar per slot in `MidiEditor`; `ProgressionEditor::
  getPopulatedSlots` reads back out by sorting `MidiEditor`'s chord blocks by position and
  reporting each one's `ProgressionSlot` - always freshly detected from whatever notes are
  currently there (see `ChordIdentifier` above), not authored/frozen at drop time. A saved preset
  therefore reflects whatever's diatonically detectable at save time; a bar whose notes have been
  edited into something non-diatonic to the current Key/Scale simply has no chord block and is
  skipped. `ProgressionPresetLibrary` combines built-ins with user-saved presets,
  persisted via `juce::PropertiesFile` in `AppSettings::getAppSupportDirectory()` (its own
  file/lock, separate from `AppSettings`) — shared across every plugin instance and DAW project,
  same pattern as `AppSettings` itself (including a public file-parameterized constructor for test
  isolation).
- `MidiEditorState`/`SessionState`/`SessionStateSerializer` — pure data + `juce::ValueTree`
  (de)serialization for everything a session needs to survive a DAW project close/reopen: Key,
  Scale, the chosen chord per degree, and `MidiEditorState` (a pure-data mirror of `MidiEditor`'s
  own notes - notes only, no chord-block data: chord blocks are 100% derived from notes, see below,
  so persisting them would be redundant and risk drifting out of sync). Deliberately has zero UI/
  component dependency so it's unit-testable without constructing any `nui::Component` -
  `MidiEditor::getState()`/`restoreState()` are the bridge on the component side.

**UI layer** (`Code/Include/Component`) — `KeyScaleSelector` (two comboboxes) →
`ChordDegreeBrowser` (one `ChordCard` per available degree) + `VoicingPicker` (popup, click a
card) → `ProgressionEditor` (header row: `ProgressionPresetPicker`, `SavePresetPrompt`,
`ProgressionDragHandle`; below it, `MidiEditor` - a scrollable/zoomable piano roll with a
pitch-labeled gutter, beat/bar ruler, and a read-only "chord lane" strip). `MidiEditor` is the sole
data model for the progression: dropping a `ChordCard` splits the chord into movable/resizable note
blocks (`addChordAtBeat`); the chord lane itself is never authored - it's entirely derived from
whatever notes exist, via `ChordIdentifier`, and re-labels live on every note edit (see
`recomputeChordBlocksFromNotes`, called from `notifyContentChanged`). `ProgressionEditor` never
keeps its own parallel copy of that data, it only reads/writes `MidiEditor` directly
(`addChordAtBeat`, `loadPreset`, `getPopulatedSlots`, `setKeyAndScale`,
`getMidiEditorState`/`restoreMidiEditorState`). All components follow the
template's existing construction convention: `nui::Component` base, own a
`nlayout::GridLayout<nui::Component> _layout { *this }`, `paint()`/`resized()` forward to it, react
to `nui::Theme::getChangeBroadcaster()` and `AppLocalisation::getChangeBroadcaster()` via
`changeListenerCallback`. Components bubble events up through small `Listener` interfaces (never
reach sideways) — `AppLayout` is the top-level owner that wires them together, drives
`Theory::MidiExporter`, and owns the drag-and-drop mechanism.

**Drag-and-drop** (see `AppLayout::onChordDragStarted`/`onChordFileDropped`): every chord drag is
the *same* native OS file drag (`juce::DragAndDropContainer::performExternalDragDropOfFiles`)
regardless of where it lands — a DAW track (imports the temp `.mid` as a clip, no plugin code
involved) or `MidiEditor` inside this same window (a `juce::FileDragAndDropTarget`, which can
receive a drag that originated from its own process just as well as an external one). `AppLayout`
keeps a `tempFilePath → Degree` map populated right before starting each drag, so an internal drop
can be resolved back to a chord without re-parsing the MIDI file. The "Drag it out" whole-
progression handle (`ProgressionDragHandle`) is a separate gesture-only signal (no file of its
own) - `AppLayout::onProgressionDragStarted` builds the file itself from `MidiEditor`'s exact
content via `MidiExporter::writeMidiEditorContentFile`. `PluginAudioProcessorEditor` inherits
`juce::DragAndDropContainer` explicitly (`juce::AudioProcessorEditor` doesn't).

**Session-state persistence**: `AppLayout::syncStateToValueTree()` (called after every UI change
that should persist, including `ProgressionEditor::Listener::onContentChanged` - fired on every
`MidiEditor` mutation) rebuilds a `SessionState` from the live UI and splices it as an extra child
node into `_parameterManager.getState().state` — the same `juce::AudioProcessorValueTreeState`
tree `PluginAudioProcessor::getStateInformation`/`setStateInformation` already serializes whole,
so no changes were needed there. This is *separate* from `ProgressionPresetLibrary`'s persistence:
session state is per-project (travels with the DAW project file), the preset library is global
(shared across every project, like `AppSettings`).

## Turning this into a different plugin

Follow the template's own renaming checklist (see the upstream template's `CLAUDE.md`/`README.md`)
for the CMake identity variables, `AppSettings.h`'s `InterProcessLock` id,
`ProgressionPresetLibrary`'s lock id, packaging assets, and this file. The `Theory`/`Component`
layers described above are this plugin's actual product — they'd need to be replaced, not renamed,
for an unrelated plugin.
