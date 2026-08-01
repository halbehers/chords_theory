> Copied verbatim from `~/.claude/plans/can-you-create-a-buzzing-sonnet.md` (the plan approved before
> implementation began) so it travels with the project instead of only living in the Claude Code
> install that wrote it. **This is the pre-implementation design, not an as-built description** —
> see `Handoff/HANDOFF.md` for what actually changed during implementation and why.

# chords_theory: MIDI chord-generator plugin

## Context

The user wants a new JUCE plugin, `chords_theory`, spun up as a sibling project to
`nierika_plugin_template` (at `/Users/sebastienhalbeher/Development/chords_theory`), built on a
pre-generated chord-theory JSON database (`/Users/sebastienhalbeher/Development/chords/chords.json`,
~5.27MB, 7,956 chord entries across 12 keys × 10 scales, produced by `generate.py` in the same
folder). It's a MIDI-effect plugin (Standalone/AU/AUv3/VST3): the user picks a Key + Scale, browses
the diatonic chords for each scale degree (most popular voicing shown by default, click to pick a
different voicing), and drags a chord card straight into a DAW MIDI track as a one-measure clip
anchored near middle C. A second section lets the user build a full chord progression on a
sequencer-style timeline — dragging chords from the browser above into slots (replacing whatever's
already there), or loading a preset — and drag the whole thing out as one MIDI clip. Presets the
user builds can be saved and persist across all plugin instances, like the app's settings.

Decisions confirmed with the user across this session:
- **Voicing**: closed voicing near middle C — chord tones packed within ~1 octave, the *lowest note
  in the chord's JSON array order* placed at or just below MIDI 60 (C4). Correctly covers
  inversions too: for `C/E`, the bass note E (first in the array) lands near middle C.
- **nierika_dsp dependency**: local checkout at `~/Development/nierika_dsp` (`v0.6.5`, matching the
  template's `NIERIKA_DSP_VERSION`) via `USE_LOCAL_NIERIKA_DSP=ON`.
- **Sequencer drag behavior**: dragging a chord card from the browser onto any progression slot —
  empty or already occupied — replaces that slot's chord with the dragged one.
- **Naming/structure**: the note-name→MIDI logic is a class `NoteConvertor`; every data structure
  gets its own header file; progression presets are built via a factory class, with an easy way to
  register new ones and a way for the user to save their own presets, persisted across all plugin
  instances (like `AppSettings`), with a UX-friendly save/manage flow.
- **State**: everything the user has set in a session — Key, Scale, the chosen voicing for every
  degree, and the full progression sequencer contents — must round-trip through the plugin's own
  state (a `juce::ValueTree`) so a DAW project reload restores the editor exactly as it was left.

Research (3 parallel explorations) plus direct verification of the critical files (chords.json
structure, `PluginProcessor.cpp`, `ComboBox.h`, `PopupPanel.h`, `GridLayout.h`, `AppSettings.h`,
`AppLayout.h`) confirms:
- `nierika_dsp` has **zero** music-theory/MIDI content — all chord/scale/MIDI logic is new code.
- The template's MIDI-effect plumbing in `PluginProcessor.cpp` is **already conditionally correct**
  (`#if JucePlugin_IsMidiEffect` branches in the constructor's bus setup and
  `isBusesLayoutSupported`) — only CMake flags need to change, no C++ edits there.
- `nelement::ComboBox`, `nelement::PopupPanel` (static `show(...)`/`showNonModal(...)`), and
  `nlayout::GridLayout<T>` (drag-to-reorder via `setMovable`/`Listener::onItemSwaped`) are real,
  usable primitives — confirmed by reading the headers directly.
- Nothing in `nierika_dsp` supports dragging a file out of the plugin into a host, or writing MIDI
  files — both are built directly against JUCE (`juce::DragAndDropContainer`/
  `juce::FileDragAndDropTarget`, `juce::MidiFile`).
- `AppSettings` (`Code/Include/AppSettings.h`) is the existing precedent for cross-instance,
  cross-session persistence: a `juce::PropertiesFile` + `juce::InterProcessLock`, stored under
  `AppSettings::getAppSupportDirectory()` (a public static helper) — the user-saved-presets store
  reuses this exact pattern with its own file and lock id.

## Milestones

1. **Foundation** — project scaffold, chord database model + loader, `NoteConvertor`, tests for both.
2. **Milestone A (first usable build)** — Key/Scale selection, chord-by-degree browser with default
   (most-popular) voicing, voicing picker, single-chord drag-to-DAW.
3. **Milestone B** — progression sequencer (drag-from-browser-to-slot, reordering), presets
   (built-in + user-saved, save/manage UX), whole-progression drag-to-DAW.
4. **Polish** — i18n keys, full session-state persistence, packaging identity, temp-file cleanup.

---

## 1. Project scaffolding

Copy the template excluding regenerable/VCS content, then re-init git as an independent project:

```
rsync -a --exclude=build --exclude=release-build --exclude=vs-build --exclude=xcode-build \
      --exclude=.git --exclude=Libs/juce --exclude=Libs/nierika_dsp --exclude=Libs/cpm \
      /Users/sebastienhalbeher/Development/nierika_plugin_template/ \
      /Users/sebastienhalbeher/Development/chords_theory/
cd /Users/sebastienhalbeher/Development/chords_theory && git init
```

**Renaming checklist** (per the template's own CLAUDE.md), concrete values — items marked
**(pick your own)** are arbitrary and trivially changed, not load-bearing decisions:

| Location | New value |
|---|---|
| `CMakeLists.txt` `PROJECT_NAME` | `ChordsTheory` |
| `CMakeLists.txt` `PRODUCT_NAME` | `Chords Theory` |
| `CMakeLists.txt` `COMPANY_NAME` | `Nierika` (unchanged — same manufacturer) |
| `CMakeLists.txt` `BUNDLE_ID` | `com.nierika.chordstheory` |
| `CMakeLists.txt` `MANUFACTURER_CODE` | `Nrka` (unchanged — identifies the manufacturer, not the product) |
| `CMakeLists.txt` `PLUGIN_CODE` | `Chrd` **(pick your own 4-char code if you prefer)** |
| `CMakeLists.txt` `FORMATS` | unchanged (`Standalone AU AUv3 VST3` already matches) |
| `CMakeLists.txt` `juce_add_plugin(...)` | `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`, add `IS_MIDI_EFFECT TRUE` (currently absent); `IS_SYNTH` stays `FALSE` |
| `CMakeLists.txt` `USE_LOCAL_NIERIKA_DSP` | `ON` (path already defaults correctly to `~/Development/nierika_dsp`) |
| `Code/Include/AppSettings.h:30` | `InterProcessLock` id → `"chords-theory-settings"` |
| `Packaging/icon.png` + iconset/icns | new icon, regenerate via `Scripts/generate_iconset.sh Packaging/icon.png Packaging` |
| `Packaging/macos/resources/README` | rewrite (hand-authored, currently names "Nierika Plugin Template") |
| `Packaging/{macos,windows}/resources/EULA` | rewrite or leave placeholder |
| `README.md`, `CLAUDE.md` | rewrite for Chords Theory |
| `.github/workflows/build_and_test.yml`, `Packaging/macos/distribution.xml.template`, `Packaging/windows/installer.iss` | **no changes needed** — confirmed identity-agnostic |

## 2. Data pipeline & C++ schema

New subdirectory `Code/Include/Theory/` + `Code/Source/Theory/` (parallel to the existing
`Component/` subfolder), holding all new music-theory domain logic. **One structure per header**,
per your feedback:

- `Theory/Key.h` — `enum class Key { C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B };` (12) + free
  functions `getKeyJsonKey(Key)` / `getKeyLabel(Key)`.
- `Theory/Scale.h` — `enum class Scale { Major, Minor, HarmonicMinor, MelodicMinor, Dorian,
  Phrygian, Lydian, Mixolydian, Locrian, MinorBlues };` (10) + `getScaleJsonKey(Scale)` (maps
  `HarmonicMinor` → `"Harmonic Minor"` etc., since JSON keys contain spaces).
- `Theory/Degree.h` — `enum class Degree { I, II, III, IV, V, VI, VII };` + `getDegreeLabel(Degree)`.
- `Theory/ChordType.h` — `enum class ChordType { Triad, Seventh, Ninth, Sus4, Sus2, Add9, Sixth,
  Sus4Seventh, Sus4Ninth, Power, Eleventh, Thirteenth, Inversion1, Inversion2 };` (14, matches the
  JSON's `type` string values 1:1 for a typed name — actual popularity ranking always comes from
  the JSON's own `popularityOrder` int, never re-derived from this enum's order).
- `Theory/NoteName.h`:
  ```cpp
  struct NoteName
  {
      std::string rawNote;       // JSON "note" — may contain double accidentals, e.g. "Ebb"
      std::string readableNote;  // JSON "readableNote" — display spelling
      int positionInChord;       // chord-tone role: 1,3,5,6,7,9,11,13, or 2/4 for sus — NOT octave/array order
      [[nodiscard]] int getPitchClass() const; // via NoteConvertor::parsePitchClass(rawNote)
  };
  ```
- `Theory/Chord.h`:
  ```cpp
  struct Chord
  {
      std::string symbol;         // JSON dict key, e.g. "Em7b9"
      std::string readableName;   // display string, e.g. "C7"
      ChordType type;
      int popularityOrder;        // 1-based, 1 = most popular = default voicing
      std::vector<NoteName> notes; // JSON array order (bass-first for inversions)
  };
  ```
- `Theory/ScaleDegreeData.h`:
  ```cpp
  struct ScaleDegreeData
  {
      Degree degree;
      std::vector<Chord> chords; // sorted ascending by popularityOrder; chords[0] is the default
  };
  ```
- `Theory/KeyScaleData.h`:
  ```cpp
  struct KeyScaleData
  {
      Key key;
      Scale scale;
      std::array<std::string, 5> moods;
      std::vector<NoteName> scaleNotes;     // 7 notes, or 6 for MinorBlues
      std::vector<ScaleDegreeData> degrees; // 7 entries, or 3 (I, IV, V only) for MinorBlues
      [[nodiscard]] bool isMinorBlues() const { return scale == Scale::MinorBlues; }
      [[nodiscard]] const ScaleDegreeData* findDegree(Degree d) const; // nullptr if absent
  };
  ```
- `Theory/ChordDatabase.h` / `Theory/ChordDatabase.cpp` — the loader/index class:
  ```cpp
  class ChordDatabase
  {
  public:
      static const ChordDatabase& getInstance();               // lazy-parsed singleton
      [[nodiscard]] const KeyScaleData& get(Key key, Scale scale) const; // O(1)
  private:
      ChordDatabase();  // parses BinaryData::chords_json via juce::JSON::parse, builds _index
      std::array<KeyScaleData, 12 * 10> _index; // flat, indexed by key*10 + scale
  };
  ```

**Bundling**: add `chords.json` to `juce_add_binary_data` in `CMakeLists.txt`, exactly like the
existing `.lang` files:
```cmake
juce_add_binary_data("${PROJECT_NAME}_Assets" SOURCES
        ...existing .lang files...
        ${CMAKE_CURRENT_LIST_DIR}/Assets/Data/chords.json)
```
Copy the source file to `Assets/Data/chords.json`. Parsed once, cached in a process-wide singleton
(mirrors `AppSettings::getInstance()`) so multiple plugin instances in one DAW process share one
parse.

Default-voicing resolution: `ScaleDegreeData::chords[0]` (sorted by `popularityOrder` at load time)
— confirmed against the real data (C Major degree I: `C` pop=1, `Cmaj7` pop=2, ... `C/G` pop=12,
exactly matching `generate.py`'s static type-priority list). Minor Blues confirmed structurally
distinct: only `I`/`IV`/`V` keys present in the JSON, no II/III/VI/VII.

## 3. Note-name → MIDI conversion

`Theory/NoteConvertor.h` / `Theory/NoteConvertor.cpp` — a stateless utility class (static methods,
same shape as the template's existing `Parameters` struct):

```cpp
class NoteConvertor
{
public:
    static constexpr int kMiddleC = 60;

    // "C","Bb","F#","Ebb","C##" -> 0-11. Letter base (C=0,D=2,E=4,F=5,G=7,A=9,B=11) + accidental
    // run (#/b, possibly doubled), wrapped mod 12.
    static int parsePitchClass(const std::string& noteName);

    // Closed voicing anchored near middle C: chordTonePitchClasses in JSON array order (bass-first
    // for inversions). First tone -> highest MIDI value <= kMiddleC matching its pitch class. Each
    // subsequent tone -> lowest MIDI value strictly above the previous tone's MIDI value matching
    // its own pitch class (strictly ascending output, no crossed voices, each tone within an
    // octave of its neighbor).
    static std::vector<int> voiceChordCloseToMiddleC(const std::vector<int>& chordTonePitchClasses);
};
```
Worked example (verified by hand): C7#9 → pitch classes `[0,4,7,10,3]` → MIDI `[60,64,67,70,75]`.

## 4. Chord-progression presets

One structure per header again, plus the requested factory + persistence:

- `Theory/ProgressionSlot.h`:
  ```cpp
  struct ProgressionSlot { Degree degree; };
  ```
- `Theory/ProgressionPreset.h`:
  ```cpp
  struct ProgressionPreset
  {
      std::string id;                      // stable key, e.g. "twelve_bar_blues" or "user_<uuid>"
      std::string nameKey;                  // .lang translation key — built-in presets only
      std::string displayName;              // literal name — user-saved presets only (nameKey empty)
      std::vector<Scale> applicableScales;  // empty = any scale; MinorBlues-only presets list {MinorBlues}
      std::vector<ProgressionSlot> slots;
      [[nodiscard]] bool isUserPreset() const { return nameKey.empty(); }
  };
  ```
  (`nameKey`/`displayName` split is needed once user presets exist: built-in names are translated
  live via `juce::translate(nameKey)`, same as every other UI string in this codebase; a
  user-typed preset name has no translation key, it's just text.)
- `Theory/ProgressionPresetFactory.h` / `.cpp` — makes adding new built-in presets a one-liner:
  ```cpp
  class ProgressionPresetFactory
  {
  public:
      // Every slot is a plain degree reference, sorted in playback order — this covers all
      // presets in this plan's starter list, including 12-bar blues as 12 individual entries.
      static ProgressionPreset createBuiltIn(const std::string& id, const std::string& nameKey,
                                              const std::vector<Scale>& applicableScales,
                                              const std::vector<Degree>& degreesInOrder);

      // Builds a preset from the sequencer's current live contents (used by the "Save as preset" UX).
      static ProgressionPreset createFromSlots(const std::string& displayName,
                                                const std::vector<ProgressionSlot>& slots);
  };
  ```
- `Theory/BuiltInProgressionPresets.h` / `.cpp` — the starter list, each entry one
  `ProgressionPresetFactory::createBuiltIn(...)` call (easy to extend — adding a preset is adding
  one line here):

  | id | Degrees | Scale constraint |
  |---|---|---|
  | `pop_i_v_vi_iv` | I – V – VI – IV | any 7-degree scale |
  | `fifty_progression` | I – VI – IV – V | any 7-degree scale |
  | `pachelbel` | I – V – VI – III – IV – I – IV – V | any 7-degree scale |
  | `jazz_ii_v_i` | II – V – I | any 7-degree scale |
  | `andalusian_cadence` | I – VII – VI – V | Minor / Harmonic Minor |
  | `minor_i_iv_v` | I – IV – V | Minor |
  | `twelve_bar_blues` | I-I-I-I-IV-IV-I-I-V-IV-I-I (12 slots) | **Minor Blues only** |
  | `eight_bar_blues` | I-V-IV-IV-I-V-I-V | Minor Blues only |

  (This list is exactly what's editable/reviewable per-line in `BuiltInProgressionPresets.cpp` —
  add, remove, or reorder entries freely later.)

- `Theory/ProgressionPresetLibrary.h` / `.cpp` — the combined registry + persistence, following
  `AppSettings`'s exact pattern:
  ```cpp
  class ProgressionPresetLibrary
  {
  public:
      static ProgressionPresetLibrary& getInstance(); // process-wide singleton

      [[nodiscard]] std::vector<ProgressionPreset> getAllPresets() const;    // built-in + user
      [[nodiscard]] std::vector<ProgressionPreset> getUserPresets() const;

      void savePreset(const ProgressionPreset& preset); // add or overwrite by id, persists to disk immediately
      void deletePreset(const std::string& id);          // no-op for built-in ids

  private:
      ProgressionPresetLibrary();
      void loadUserPresets();
      void persistUserPresets();

      juce::InterProcessLock _processLock { "chords-theory-user-presets" }; // distinct from AppSettings' lock
      juce::PropertiesFile _userPresetsFile; // AppSettings::getAppSupportDirectory() + "user-presets.xml"
  };
  ```
  Reuses `AppSettings::getAppSupportDirectory()` (already a public static helper) so user presets
  live in the same per-app writable location as settings, in their own file, shared/visible across
  every plugin instance and every DAW project on the machine — exactly like `AppSettings`.

  **Save/manage UX**: a small "Save as preset" affordance next to `ProgressionPresetPicker` (an
  `nelement::SVGButton` or `nelement::TextButton`, matching existing icon-button patterns) opens a
  short `nelement::TextInput` prompt (reusing the `SettingsWindow`-style overlay or a
  `PopupPanel`) for a name; confirming calls `ProgressionPresetFactory::createFromSlots(name,
  currentSlots)` then `ProgressionPresetLibrary::getInstance().savePreset(...)`.
  `ProgressionPresetPicker`'s combobox lists built-ins first, then a visual separator, then user
  presets (label resolved as `preset.isUserPreset() ? preset.displayName :
  juce::translate(preset.nameKey)`); each user-preset entry gets a small delete affordance so the
  list doesn't grow unbounded.

## 5. UI components

New files under `Code/Include/Component/` + `Code/Source/Component/`, following the exact
`LanguageSettings`/`VisualSettings` construction convention (constructor takes an id, owns
`nlayout::GridLayout<nui::Component> _layout { *this }`, `paint()`/`resized()` forward to `_layout`,
theme/locale reactivity via `changeListenerCallback`):

1. **`KeyScaleSelector`** — two `nelement::ComboBox` (`_keyPicker`, `_scalePicker`), same pattern as
   `LanguageSettings`'s `_languagePicker`. Owns current `Key`/`Scale`, notifies a listener interface
   up to `AppLayout`.
2. **`ChordCard`** — custom `nui::Component` (not `nelement::TextButton`, which can't lay out a
   two-line "degree label + chord name"). Click opens `VoicingPicker`; drag triggers MIDI export
   (§6) — the *same* drag mechanism serves both "drop onto a progression slot" and "drop onto a DAW
   track" (see §6, this is the part your feedback changed the design of).
3. **`ChordDegreeBrowser`** — a `GridLayout` of one `ChordCard` per available degree (7 normally, 3
   for Minor Blues), repopulated on key/scale change.
4. **`VoicingPicker`** — built on `nelement::PopupPanel::show(...)` (confirmed signature:
   `static juce::CallOutBox& show(std::unique_ptr<juce::Component>, juce::Rectangle<int>,
   juce::Component*)`); content is the degree's chords sorted by `popularityOrder`; selecting one
   swaps the card's chord.
5. **`ProgressionSlot`** (UI component — distinct from the `theory::ProgressionSlot` data struct;
   name collision noted, will likely rename the UI component `ProgressionSlotView` to avoid
   confusion in code) — one sequencer timeline cell, empty or holding a resolved chord. Implements
   `juce::FileDragAndDropTarget` to accept a dropped `ChordCard` (§6) and **replace** whatever chord
   currently occupies it, per your requirement. `GridLayout`'s built-in `setMovable`/`Listener::
   onItemSwaped` (confirmed present in `GridLayout.h`) additionally handles reordering slots that
   are already populated, dragging within the sequencer itself.
6. **`ProgressionSequencer`** — owns the row of slot views, the "insert whole progression"
   drag-source, the "save as preset" affordance, and hosts `ProgressionPresetPicker` above it.
7. **`ProgressionPresetPicker`** — one more `nelement::ComboBox`, populated from
   `ProgressionPresetLibrary::getInstance().getAllPresets()`, filtered/grayed per §4.

**Wiring into `AppLayout`**: currently a 1×1-weighted grid holding just the settings gear button
(`Code/Include/AppLayout.h`/`.cpp`). Extend to 3 rows: row 0 fixed-height for `KeyScaleSelector`
(existing settings button unchanged), row 1 for `ChordDegreeBrowser`, row 2 for
`ProgressionSequencer` — using the same `getLayout().setFixedRowHeight(...)`/`addComponent(...)`
calls already in use. `AppLayout` owns the shared `Key`/`Scale` state and the per-degree voicing
selection map, since both the browser and sequencer read it.

**i18n**: new keys in all six `Assets/Languages/*.lang` files (never delete existing keys) — labels
for the pickers, one key per scale name, one key per built-in preset name.

## 6. Drag-and-drop: unified mechanism for both destinations

This is the part your feedback (drag-from-browser-into-a-sequencer-slot) required working out
concretely, because JUCE can't start one drag type and switch to another mid-gesture —
`DragAndDropContainer::startDragging` (in-app only) and `performExternalDragDropOfFiles` (native OS
file drag) are two separate blocking calls; you must commit to one when the gesture begins, before
you know whether the mouse will end up over a `ProgressionSlot` or over the DAW's track view
outside the plugin window entirely.

**Resolution**: always use the native OS file drag (`performExternalDragDropOfFiles`) for every
`ChordCard` drag, no matter where it ends up. This works for *both* destinations because
`juce::FileDragAndDropTarget` — the receiving side of that same native drag — can be implemented by
**any** JUCE component, including ones inside the same plugin window that started the drag. So:

- `ChordCard::mouseDrag` (past a minimum-distance threshold, so plain clicks still open the
  `VoicingPicker`): calls `Theory::MidiExporter::writeSingleChordMidiFile(...)` (new class,
  `Theory/MidiExporter.h`/`.cpp`, same static-method shape as `NoteConvertor`) to get a temp `.mid`
  file, records `tempFilePath → Chord` in a small in-flight map owned by `AppLayout` (so a drop
  target can look up which chord a given dropped path represents without re-parsing MIDI), then
  calls `performExternalDragDropOfFiles({tempFilePath}, false)`.
- If the user releases over a `ProgressionSlot` (a `FileDragAndDropTarget`) inside the plugin's own
  window: `filesDropped(files, x, y)` looks up the chord for `files[0]` in that in-flight map and
  replaces the slot's chord — satisfying "replace whatever's already there."
- If the user releases outside the plugin window entirely, over the DAW's own track view: the OS
  hands the temp file to the DAW as normal, which imports it as a MIDI clip — no plugin-side code
  runs at all for that path, it's just how the OS drag already works.
- **Whole-progression drag** (the item at the bottom of the sequencer) uses the same mechanism:
  `MidiExporter::writeProgressionMidiFile(...)` generates one multi-chord `.mid`, then
  `performExternalDragDropOfFiles`. Nothing inside the plugin needs to accept *this* drag, so no
  in-flight-map entry is needed for it.

`Theory/MidiExporter.h`:
```cpp
class MidiExporter
{
public:
    static juce::File writeSingleChordMidiFile(const Chord& chord, double bpm, int timeSigNumerator);
    static juce::File writeProgressionMidiFile(const std::vector<ResolvedProgressionSlot>& slots, double bpm, int timeSigNumerator);
};
```
(`ResolvedProgressionSlot` — a small struct pairing a `theory::ProgressionSlot` with the actual
`Chord` it resolves to given the current per-degree voicing map — gets its own header,
`Theory/ResolvedProgressionSlot.h`, per the one-structure-per-file rule.)

- **Tempo/measure assumption**: 120 BPM, 4/4 fallback (no reliable host tempo is available from a
  UI-only `mouseDrag`; read `AudioPlayHead::getPosition()` if valid, else fall back). One measure =
  4 beats, `ticksPerQuarterNote = 960`, fixed velocity (e.g. 100).
- **Temp file lifecycle**: `<tempDir>/ChordsTheory/<uuid>.mid`, one fresh file per drag gesture.
  Clean up stale files lazily (delete files older than a few hours) at next plugin load — not
  synchronously after the drag, since `performExternalDragDropOfFiles` returns once the OS-level
  drag ends, not once the target has finished reading the file.
- **`DragAndDropContainer` placement**: `juce::AudioProcessorEditor` does not inherit this — add it
  explicitly: `class PluginAudioProcessorEditor : public juce::AudioProcessorEditor, public
  juce::DragAndDropContainer` (confirmed current `PluginEditor.h` has no conflicting base).

## 7. Session state — everything round-trips via ValueTree

Per your requirement, this now covers the full session, not just Key/Scale: **Key, Scale, the
chosen chord (symbol) for every scale degree, and the full contents of the progression
sequencer (chord per slot)** all round-trip through `PluginProcessor::getStateInformation`/
`setStateInformation`, so a DAW closing/reopening or reloading a saved project restores the editor
exactly as it was left. This is per-instance/per-project state (saved inside the DAW project file)
— a different, separate concern from the cross-instance `ProgressionPresetLibrary` in §4 (which is
more like a personal preset shelf, shared everywhere, independent of any specific project).

Not modeled as `AudioProcessorValueTreeState` parameters: these are discrete, editor-set-once
config values, not continuously host-automated ones, and the progression (a variable-length list)
doesn't map onto any single-value `RangedAudioParameter` type. Instead, a `juce::ValueTree`
appended alongside the existing APVTS parameter state:

```
ChordsTheoryState
  key="C" scale="Major"
  DegreeVoicings
    Degree id="I" chordSymbol="Cmaj7"
    Degree id="II" chordSymbol="Dm7"
    ...
  ProgressionSlots
    Slot index="0" degree="I" chordSymbol="Cmaj7"
    Slot index="1" degree="V" chordSymbol="G7"
    ...
```

`PluginProcessor.cpp`'s `getStateInformation`/`setStateInformation` currently just forward to
`ndsp::ParameterManager`'s (which only covers the APVTS parameter tree) — extend both to also
serialize/deserialize this `ValueTree` alongside it. `Parameters::PLUGIN_ENABLED_ID` stays as-is —
it already drives `PluginEditor::setBypass`/`AppLayout`'s inherited bypass-disable behavior, which
still makes sense as a "disable the UI" toggle.

## 8. Testing (Catch2, mirrors existing `Tests/*.cpp` convention)

New files, added to `Tests/CMakeLists.txt`'s source list:
- **`ChordDatabaseTests.cpp`**: parses without throwing; C Major degree I matches the verified
  12-chord/popularity-order example above; every (Key,Scale) has 7 degrees except Minor Blues (3:
  I/IV/V only); all 120 index cells populated; every degree's chords sorted ascending by
  `popularityOrder` starting at 1 with no gaps.
- **`NoteConvertorTests.cpp`**: `parsePitchClass` table incl. naturals/sharps/flats/double-accidentals
  (confirmed present in the real data, e.g. `Ebb`, `C##`); `voiceChordCloseToMiddleC` strictly
  ascending, `front()` in `[49,60]`, matches the C7#9 worked example.
- **`ProgressionPresetTests.cpp`**: every built-in preset resolves against every scale in its
  `applicableScales`; ids unique/non-empty (mirrors `PluginProcessorTests.cpp`'s existing
  ID-uniqueness pattern); 12-bar blues has exactly 12 one-bar slots in the documented order;
  `ProgressionPresetLibrary` save/load round-trips a user preset through a temp properties file.
- **`MidiExporterTests.cpp`**: round-trip through `juce::MidiFile::readFrom`; note count matches
  chord size (single) / sum across slots (progression); tick timing matches the 120bpm/4-4
  assumption. Explicitly **not** testing actual OS drag-and-drop (not feasible/valuable in Catch2)
  — only the file-generation logic, called directly.
- **`ChordsTheoryStateTests.cpp`**: `getStateInformation`/`setStateInformation` round-trips Key,
  Scale, all per-degree voicing choices, and a populated progression sequencer, mirroring
  `PluginProcessorTests.cpp`'s existing APVTS round-trip test pattern.

## Verification

- **Build**: `cmake --workflow --preset default` (Debug) or `--preset release` from
  `/Users/sebastienhalbeher/Development/chords_theory`. First configure fetches JUCE via CPM and
  `juce_add_module()`s the local `~/Development/nierika_dsp` checkout.
- **Unit tests**: `ctest --test-dir build` — Catch2 suite above + `pluginval` end-to-end validation
  of the built AU (load/save-state/repeated open-close as a MIDI effect).
- **Standalone smoke test**: run `build/ChordsTheory_artefacts/Debug/Standalone/Chords Theory.app`
  to interactively exercise Key/Scale selection, degree browsing, voicing picker, and progression
  building (including drag-from-browser-into-a-slot, testable within Standalone since both ends are
  inside the same app) without a DAW — covers everything except the actual drop onto a DAW track.
- **Manual DAW verification** (not automatable — call out explicitly to the user):
  1. Build/install VST3 and/or AU; load as a MIDI-effect track insert.
  2. Drag a `ChordCard` onto an empty MIDI/instrument track; confirm a one-measure clip appears
     with the expected closed-voiced notes anchored near middle C (check note numbers in the DAW's
     piano roll against §3's algorithm).
  3. Repeat after picking a non-default voicing via `VoicingPicker`; confirm the dropped clip
     reflects the swap.
  4. Drag a chord from the browser onto an already-occupied progression slot; confirm it replaces
     the existing chord (not just this plan's design intent — the actual UX request).
  5. Build a progression via a preset (one 7-degree preset and the 12-bar-blues preset
     specifically), save it as a user preset, confirm it appears in the picker; drag the
     whole-progression item and confirm correct measure count/order in the DAW.
  6. Close and reopen the DAW project; confirm Key/Scale/per-degree voicings/progression contents
     round-trip exactly (§7), and confirm the saved user preset from step 5 is still available in a
     *different* DAW project entirely (§4's cross-instance persistence).
  7. Test in at least two DAWs if available — external-MIDI-file-drop handling isn't perfectly
     standardized across hosts.
