#include "Theory/MidiExporter.h"

#include "Theory/NoteConvertor.h"

namespace theory
{

namespace
{
    constexpr double kMeasureTicks = static_cast<double>(MidiExporter::kBeatsPerMeasure * MidiExporter::kTicksPerQuarterNote);

    juce::File getTempDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ChordsTheory");
        dir.createDirectory();
        return dir;
    }

    juce::File createFreshTempMidiFile()
    {
        return getTempDirectory().getChildFile(juce::Uuid().toString() + ".mid");
    }

    void addChordNotesToSequence(juce::MidiMessageSequence& sequence, const Chord& chord, double startTick, double durationTicks)
    {
        const auto midiNotes = NoteConvertor::voiceChordCloseToMiddleC(chord);

        for (const int midiNote : midiNotes)
        {
            auto noteOn = juce::MidiMessage::noteOn(1, midiNote, MidiExporter::kVelocity);
            noteOn.setTimeStamp(startTick);
            sequence.addEvent(noteOn);

            auto noteOff = juce::MidiMessage::noteOff(1, midiNote);
            noteOff.setTimeStamp(startTick + durationTicks);
            sequence.addEvent(noteOff);
        }
    }

    void addTempoAndTimeSignature(juce::MidiMessageSequence& sequence, double bpm)
    {
        auto tempoEvent = juce::MidiMessage::tempoMetaEvent(static_cast<int>(60'000'000.0 / bpm));
        tempoEvent.setTimeStamp(0);
        sequence.addEvent(tempoEvent);

        auto timeSigEvent = juce::MidiMessage::timeSignatureMetaEvent(MidiExporter::kBeatsPerMeasure, 4);
        timeSigEvent.setTimeStamp(0);
        sequence.addEvent(timeSigEvent);
    }

    juce::File writeSequenceToFreshTempFile(juce::MidiMessageSequence& sequence)
    {
        sequence.updateMatchedPairs();

        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote(MidiExporter::kTicksPerQuarterNote);
        midiFile.addTrack(sequence);

        const auto file = createFreshTempMidiFile();

        if (auto outputStream = file.createOutputStream())
            midiFile.writeTo(*outputStream);

        return file;
    }
}

juce::File MidiExporter::writeSingleChordMidiFile(const Chord& chord, double bpm)
{
    juce::MidiMessageSequence sequence;

    addTempoAndTimeSignature(sequence, bpm);
    addChordNotesToSequence(sequence, chord, 0.0, kMeasureTicks);

    return writeSequenceToFreshTempFile(sequence);
}

juce::File MidiExporter::writeProgressionMidiFile(const std::vector<ResolvedProgressionSlot>& slots, double bpm)
{
    juce::MidiMessageSequence sequence;

    addTempoAndTimeSignature(sequence, bpm);

    double startTick = 0.0;
    for (const auto& resolvedSlot : slots)
    {
        addChordNotesToSequence(sequence, resolvedSlot.chord, startTick, kMeasureTicks);
        startTick += kMeasureTicks;
    }

    return writeSequenceToFreshTempFile(sequence);
}

}
