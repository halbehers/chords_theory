#pragma once

#include "Theory/Degree.h"

namespace theory
{

// One slot in a chord progression. degree is always required; popularityOrder is an optional pin
// to a specific voicing - 0 means "resolve dynamically against whatever voicing the user
// currently has selected for this degree" (every built-in preset, and every slot while the
// sequencer is still being built live). User-saved presets pin popularityOrder at save time (see
// ProgressionEditor::getPopulatedSlots), so reloading one later reproduces the exact chords
// used when it was saved, independent of the browser's current per-degree voicings.
//
// Deliberately NOT pinned by Chord::symbol: a symbol is an absolute-pitch string (e.g. "Cmaj7"),
// unique to one key - C Major degree I's list is {"C","Cmaj7",...}, D Major degree I's is
// {"D","Dmaj7",...}, with no symbol ever shared between keys, so a symbol-based pin would always
// fail to resolve (and silently fall back to the live default) the moment the key changes.
// Chord::popularityOrder is the key-independent identifier instead: confirmed against the actual
// chords.json data that for every (scale, degree) pair, the sequence of chord types sorted by
// popularityOrder is identical across all 12 keys (e.g. popularityOrder 2 is always the
// "seventh"-type voicing) - it's what actually represents "which voicing slot" the user picked,
// transposably. Resolution - respecting the pin, falling back to the live per-degree voicing if
// the pinned popularityOrder doesn't exist under whatever key/scale is currently active - happens
// in ChordDegreeBrowser::resolveSlot.
struct ProgressionSlot
{
    Degree degree = Degree::I;
    int popularityOrder = 0; // 0 = unpinned

    bool operator==(const ProgressionSlot& other) const
    {
        return degree == other.degree && popularityOrder == other.popularityOrder;
    }
};

}
