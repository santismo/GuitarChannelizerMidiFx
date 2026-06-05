#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace
{
    struct SourceMidiEvent
    {
        juce::MidiMessage message;
        double tick = 0.0;
        int order = 0;
    };

    bool isMidiFile (const juce::File& file)
    {
        return file.hasFileExtension ("mid;midi");
    }

    struct ScaleDefinition
    {
        const char* name = "";
        std::vector<int> intervals;
    };

    const std::array<ScaleDefinition, 15>& scaleDefinitions()
    {
        static const std::array<ScaleDefinition, 15> scales
        {{
            { "Ionian",              { 0, 2, 4, 5, 7, 9, 11 } },
            { "Dorian",              { 0, 2, 3, 5, 7, 9, 10 } },
            { "Phrygian",            { 0, 1, 3, 5, 7, 8, 10 } },
            { "Lydian",              { 0, 2, 4, 6, 7, 9, 11 } },
            { "Mixolydian",          { 0, 2, 4, 5, 7, 9, 10 } },
            { "Aeolian",             { 0, 2, 3, 5, 7, 8, 10 } },
            { "Locrian",             { 0, 1, 3, 5, 6, 8, 10 } },
            { "Major Pentatonic",    { 0, 2, 4, 7, 9 } },
            { "Minor Pentatonic",    { 0, 3, 5, 7, 10 } },
            { "Blues",               { 0, 3, 5, 6, 7, 10 } },
            { "Harmonic Minor",      { 0, 2, 3, 5, 7, 8, 11 } },
            { "Melodic Minor",       { 0, 2, 3, 5, 7, 9, 11 } },
            { "Diminished Whole",    { 0, 2, 3, 5, 6, 8, 9, 11 } },
            { "Diminished Half",     { 0, 1, 3, 4, 6, 7, 9, 10 } },
            { "Whole Tone",          { 0, 2, 4, 6, 8, 10 } }
        }};

        return scales;
    }

    bool pitchInScale (int note, int root, const ScaleDefinition& scale)
    {
        const auto pitchClass = (note - root + 1200) % 12;
        return std::find (scale.intervals.begin(), scale.intervals.end(), pitchClass) != scale.intervals.end();
    }

    int clampScaleMask (int mask)
    {
        const auto maxMask = (1 << static_cast<int> (scaleDefinitions().size())) - 1;
        return juce::jlimit (1, maxMask, mask);
    }

    int clampRootMask (int mask)
    {
        return juce::jlimit (1, (1 << 12) - 1, mask);
    }

    int nearestPitchIndex (const std::vector<int>& pitches, int targetPitch)
    {
        auto bestIndex = 0;
        auto bestDistance = std::numeric_limits<int>::max();

        for (int i = 0; i < static_cast<int> (pitches.size()); ++i)
        {
            const auto distance = std::abs (pitches[static_cast<size_t> (i)] - targetPitch);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        return bestIndex;
    }

    int positiveMod12 (int value) noexcept
    {
        return (value % 12 + 12) % 12;
    }

    juce::String pitchClassName (int pitchClass)
    {
        static const std::array<const char*, 12> names
        {
            "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
        };

        return names[static_cast<size_t> (positiveMod12 (pitchClass))];
    }

    struct ChordPattern
    {
        const char* suffix = "";
        std::vector<int> required;
        int baseScore = 0;
    };

    struct ChordInfo
    {
        juce::String name = "--";
        int rootPitchClass = -1;
    };

    const std::vector<ChordPattern>& chordPatterns()
    {
        static const std::vector<ChordPattern> patterns
        {
            { "dim7",  { 0, 3, 6, 9 },  42 },
            { "m7b5",  { 0, 3, 6, 10 }, 41 },
            { "mMaj7", { 0, 3, 7, 11 }, 40 },
            { "maj7",  { 0, 4, 7, 11 }, 39 },
            { "m7",    { 0, 3, 7, 10 }, 38 },
            { "7sus4", { 0, 5, 7, 10 }, 37 },
            { "7",     { 0, 4, 7, 10 }, 36 },
            { "m6",    { 0, 3, 7, 9 },  35 },
            { "6",     { 0, 4, 7, 9 },  34 },
            { "m7",    { 0, 3, 10 },    33 },
            { "maj7",  { 0, 4, 11 },    33 },
            { "mMaj7", { 0, 3, 11 },    32 },
            { "7",     { 0, 4, 10 },    32 },
            { "m6",    { 0, 3, 9 },     31 },
            { "6",     { 0, 4, 9 },     30 },
            { "dim",   { 0, 3, 6 },     30 },
            { "aug",   { 0, 4, 8 },     29 },
            { "m",     { 0, 3, 7 },     28 },
            { "",      { 0, 4, 7 },     27 },
            { "9(no3)", { 0, 2, 10 },   26 },
            { "sus2",  { 0, 2, 7 },     24 },
            { "sus4",  { 0, 5, 7 },     24 }
        };

        return patterns;
    }

    bool hasInterval (const std::array<bool, 12>& intervals, int interval) noexcept
    {
        return intervals[static_cast<size_t> (positiveMod12 (interval))];
    }

    bool patternContains (const ChordPattern& pattern, int interval)
    {
        return std::find (pattern.required.begin(), pattern.required.end(), positiveMod12 (interval)) != pattern.required.end();
    }

    juce::String promotedExtensionSuffix (juce::String suffix, const std::array<bool, 12>& intervals, const ChordPattern& pattern)
    {
        const auto hasFlat7 = hasInterval (intervals, 10);
        const auto hasMajor7 = hasInterval (intervals, 11);
        const auto hasAny7 = hasFlat7 || hasMajor7;
        const auto has9 = hasInterval (intervals, 2) && ! patternContains (pattern, 2);
        const auto has11 = hasInterval (intervals, 5) && ! patternContains (pattern, 5);
        const auto has13 = hasInterval (intervals, 9) && ! patternContains (pattern, 9);

        if (hasAny7)
        {
            if (has13)
            {
                if (suffix == "7") return "13";
                if (suffix == "maj7") return "maj13";
                if (suffix == "m7") return "m13";
                if (suffix == "mMaj7") return "mMaj13";
                return suffix + "13";
            }

            if (has11)
            {
                if (suffix == "7") return "11";
                if (suffix == "maj7") return "maj11";
                if (suffix == "m7") return "m11";
                if (suffix == "mMaj7") return "mMaj11";
                return suffix + "11";
            }

            if (has9)
            {
                if (suffix == "7") return "9";
                if (suffix == "maj7") return "maj9";
                if (suffix == "m7") return "m9";
                if (suffix == "mMaj7") return "mMaj9";
                return suffix + "9";
            }
        }

        if (has9)
            suffix += "add9";
        if (has11 && suffix != "sus4")
            suffix += "add11";
        if (has13 && suffix != "6" && suffix != "m6")
            suffix += "6";

        return suffix;
    }

    juce::String alterationSuffix (const std::array<bool, 12>& intervals, const ChordPattern& pattern)
    {
        juce::String suffix;

        if (hasInterval (intervals, 1))
            suffix += "b9";

        if (hasInterval (intervals, 3) && hasInterval (intervals, 4) && ! patternContains (pattern, 3))
            suffix += "#9";

        if (hasInterval (intervals, 6) && ! patternContains (pattern, 6))
            suffix += "#11";

        if (hasInterval (intervals, 8) && ! patternContains (pattern, 8))
            suffix += "#5";

        return suffix;
    }

    ChordInfo identifyChord (const std::vector<int>& activeNotes)
    {
        if (activeNotes.size() < 3)
            return {};

        const auto bassPitchClass = positiveMod12 (activeNotes.front());
        std::array<bool, 12> pitchClasses {};
        std::vector<int> roots;

        for (const auto note : activeNotes)
        {
            const auto pitchClass = positiveMod12 (note);
            if (! pitchClasses[static_cast<size_t> (pitchClass)])
                roots.push_back (pitchClass);
            pitchClasses[static_cast<size_t> (pitchClass)] = true;
        }

        ChordInfo best;
        auto bestScore = std::numeric_limits<int>::min();

        for (const auto root : roots)
        {
            std::array<bool, 12> intervals {};
            auto intervalCount = 0;

            for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
            {
                if (! pitchClasses[static_cast<size_t> (pitchClass)])
                    continue;

                intervals[static_cast<size_t> (positiveMod12 (pitchClass - root))] = true;
                ++intervalCount;
            }

            for (const auto& pattern : chordPatterns())
            {
                const auto matches = std::all_of (pattern.required.begin(), pattern.required.end(), [&] (int interval)
                {
                    return hasInterval (intervals, interval);
                });

                if (! matches)
                    continue;

                auto recognized = static_cast<int> (pattern.required.size());
                if (hasInterval (intervals, 2)) ++recognized;
                if (hasInterval (intervals, 5)) ++recognized;
                if (hasInterval (intervals, 9)) ++recognized;
                if (hasInterval (intervals, 1)) ++recognized;
                if (hasInterval (intervals, 6)) ++recognized;
                if (hasInterval (intervals, 8)) ++recognized;

                auto score = pattern.baseScore + recognized * 2 - juce::jmax (0, intervalCount - recognized) * 3;
                if (root == bassPitchClass)
                    score += 8;

                auto suffix = promotedExtensionSuffix (pattern.suffix, intervals, pattern) + alterationSuffix (intervals, pattern);
                auto name = pitchClassName (root) + suffix;

                if (root != bassPitchClass)
                    name += "/" + pitchClassName (bassPitchClass);

                if (score > bestScore)
                {
                    bestScore = score;
                    best.name = name;
                    best.rootPitchClass = root;
                }
            }
        }

        if (best.rootPitchClass >= 0)
            return best;

        juce::StringArray names;
        for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
            if (pitchClasses[static_cast<size_t> (pitchClass)])
                names.add (pitchClassName (pitchClass));

        best.rootPitchClass = bassPitchClass;
        best.name = pitchClassName (bassPitchClass) + " voicing (" + names.joinIntoString (" ") + ")";
        return best;
    }

    double melodySpeedMultiplierForIndex (int index) noexcept
    {
        switch (index)
        {
            case 0: return 0.25;
            case 1: return 0.5;
            case 3: return 2.0;
            case 4: return 3.0;
            case 5: return 4.0;
            default: return 1.0;
        }
    }
}

GuitarChannelizerAudioProcessor::GuitarChannelizerAudioProcessor()
    : AudioProcessor (BusesProperties()),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    clearFretboard();
    clearLiveRootMarkers();
    for (auto& value : livePitchActivity)
        value.store (0, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    resetLiveState();
}

bool GuitarChannelizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    juce::ignoreUnused (layouts);
    return true;
}

void GuitarChannelizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    double hostPpq = 0.0;
    auto hostBpm = 120.0;
    auto hostIsPlaying = false;
    const auto hasHostPosition = readHostPosition (hostPpq, hostBpm, hostIsPlaying);
    const auto wasPlaying = hostWasPlaying.exchange (hasHostPosition && hostIsPlaying);

    if (hasHostPosition && wasPlaying && ! hostIsPlaying)
        resetLiveState();
    else if (hasHostPosition && ! wasPlaying && hostIsPlaying)
    {
        melodyStartHostPpq.store (hostPpq, std::memory_order_relaxed);
        resetLiveState();
    }

    juce::MidiBuffer output;
    const auto inverted = getBoolParameter (ParameterIDs::invertChannels);

    if (getBoolParameter (ParameterIDs::processLiveInput))
        processLiveMidi (midiMessages, output, inverted);
    else
        output = midiMessages;

    if (hasHostPosition && hostIsPlaying && getBoolParameter (ParameterIDs::melodyOutputEnabled))
        emitGeneratedMelody (output, hostPpq, hostBpm, buffer.getNumSamples());

    midiMessages.swapWith (output);
}

juce::AudioProcessorEditor* GuitarChannelizerAudioProcessor::createEditor()
{
    return new GuitarChannelizerAudioProcessorEditor (*this);
}

void GuitarChannelizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GuitarChannelizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarChannelizerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::invertChannels, "Invert String Channels", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::processLiveInput, "Process Live Input", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::useInputChannels, "Use Input Channels", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::showSameChannelNotes, "Show Same-Channel String Notes", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::preferUpperFrets, "Prefer Higher Positions", false));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::preferredMinFret, "Preferred Minimum Fret", 1, 12, 5));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::handTargetRadius, "Hand Focus Span", 1, 8, 4));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::fretboardTheme, "Fretboard Theme", getFretboardThemeNames(), 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::fretboardShape, "Fretboard Shape", getFretboardShapeNames(), 0));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::fretboardColorSeed, "Fretboard Color Seed", 0, 4095, 180));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::fretboardColorMode, "Fretboard Color Mode", getFretboardColorModeNames(), 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::latchChordName, "Latch Chord Name", true));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::chordLatchReleaseMs, "Chord Latch Release Delay", 0, 1000, 250));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::melodyOutputEnabled, "Generated Melody Output", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::melodyGuitaristic, "Guitaristic Melody", true));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::melodySpeed, "Melody Speed", getMelodySpeedNames(), 2));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyComplexity, "Melody Complexity", 0, 100, 42));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyDensity, "Melody Density", 0, 100, 35));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyBars, "Melody Bars", 4, 32, 8));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterIDs::melodyRootKey, "Melody Root Key", getKeyNames(), 0));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyRootMask, "Melody Root Mask", 1, (1 << 12) - 1, (1 << 12) - 1));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyKeyCount, "Melody Key Changes", 1, 12, 1));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyScaleMask, "Melody Scale Mask", 1, (1 << static_cast<int> (scaleDefinitions().size())) - 1, 1));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyMinNote, "Melody Low Note", 36, 96, 45));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyMaxNote, "Melody High Note", 36, 96, 76));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::melodyPureLegato, "Pure Legato", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (ParameterIDs::melodyForceFirstNote, "Start Melody Immediately", true));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyLegatoPoly, "Legato Poly Blend", 0, 100, 0));
    params.push_back (std::make_unique<juce::AudioParameterInt> (ParameterIDs::melodyPolyDensity, "Poly Density", 2, 6, 3));
    return { params.begin(), params.end() };
}

juce::StringArray GuitarChannelizerAudioProcessor::getKeyNames()
{
    return { "Random", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
}

juce::StringArray GuitarChannelizerAudioProcessor::getScaleNames()
{
    juce::StringArray names;

    for (const auto& scale : scaleDefinitions())
        names.add (scale.name);

    return names;
}

juce::StringArray GuitarChannelizerAudioProcessor::getFretboardThemeNames()
{
    return { "Studio", "Maple", "Rosewood", "Neon", "Midivana" };
}

juce::StringArray GuitarChannelizerAudioProcessor::getFretboardShapeNames()
{
    return { "Rounded", "Square", "Circle", "Hex", "Diamond", "Octagon", "Pill", "Triangle", "Star", "Pentagon", "Spark" };
}

juce::StringArray GuitarChannelizerAudioProcessor::getFretboardColorModeNames()
{
    return { "Solid", "Gradient", "Shift" };
}

juce::StringArray GuitarChannelizerAudioProcessor::getMelodySpeedNames()
{
    return { "1/4x", "1/2x", "1x", "2x", "3x", "4x" };
}

juce::String GuitarChannelizerAudioProcessor::loadMidiFile (const juce::File& file)
{
    if (! isMidiFile (file))
        return "Load failed: choose a .mid or .midi file.";

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
        return "Load failed: could not open " + file.getFileName() + ".";

    juce::MidiFile midiFile;
    int midiFileType = 1;
    if (! midiFile.readFrom (stream, true, &midiFileType))
        return "Load failed: " + file.getFileName() + " is not a readable MIDI file.";

    auto ticksPerQuarter = midiFile.getTimeFormat();
    if (ticksPerQuarter <= 0)
        ticksPerQuarter = 960;

    auto processed = buildProcessedSequence (midiFile, ticksPerQuarter);
    const auto eventCount = processed.getNumEvents();
    auto lengthPpq = processed.getEndTime() / static_cast<double> (ticksPerQuarter);

    {
        const std::lock_guard<std::mutex> lock (loadedMutex);
        processedLoadedSequence = std::move (processed);
        loadedSourceMidiFile = midiFile;
        loadedTicksPerQuarter = ticksPerQuarter;
        loadedLengthPpq = lengthPpq;
        loadedEventsPreferUpperFrets = getBoolParameter (ParameterIDs::preferUpperFrets);
        loadedEventsPreferredMinFret = getIntParameter (ParameterIDs::preferredMinFret);
        loadedSummary = file.getFileName() + " loaded, " + juce::String (eventCount) + " events, "
                      + juce::String (lengthPpq, 2) + " quarter notes.";
    }

    return loadedSummary;
}

juce::String GuitarChannelizerAudioProcessor::exportProcessedMidiFile (const juce::File& file) const
{
    juce::MidiMessageSequence sequence;
    juce::MidiFile sourceMidiFile;
    short ticksPerQuarter = 960;

    {
        const std::lock_guard<std::mutex> lock (loadedMutex);
        if (processedLoadedSequence.getNumEvents() == 0)
            return "Export failed: no MIDI file has been loaded.";

        sourceMidiFile = loadedSourceMidiFile;
        ticksPerQuarter = loadedTicksPerQuarter;
    }

    sequence = buildProcessedSequence (sourceMidiFile, ticksPerQuarter);

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarter);
    sequence.updateMatchedPairs();
    midiFile.addTrack (sequence);

    auto target = file;
    if (! target.hasFileExtension ("mid;midi"))
        target = target.withFileExtension ("mid");

    std::unique_ptr<juce::FileOutputStream> stream (target.createOutputStream());
    if (stream == nullptr || ! stream->openedOk())
        return "Export failed: could not write " + target.getFileName() + ".";

    if (! midiFile.writeTo (*stream, 1))
        return "Export failed: MIDI writer rejected " + target.getFileName() + ".";

    stream->flush();
    return "Exported " + target.getFileName() + ".";
}

juce::String GuitarChannelizerAudioProcessor::getLoadedMidiSummary() const
{
    const std::lock_guard<std::mutex> lock (loadedMutex);
    return loadedSummary;
}

void GuitarChannelizerAudioProcessor::refreshLoadedMidiForCurrentSettings()
{
    const auto preferUpperFrets = getBoolParameter (ParameterIDs::preferUpperFrets);
    const auto preferredMinFret = getIntParameter (ParameterIDs::preferredMinFret);
    const std::lock_guard<std::mutex> lock (loadedMutex);

    if (processedLoadedSequence.getNumEvents() == 0
        || (loadedEventsPreferUpperFrets == preferUpperFrets && loadedEventsPreferredMinFret == preferredMinFret))
    {
        return;
    }

    processedLoadedSequence = buildProcessedSequence (loadedSourceMidiFile, loadedTicksPerQuarter);
    loadedLengthPpq = processedLoadedSequence.getEndTime() / static_cast<double> (loadedTicksPerQuarter);
    loadedEventsPreferUpperFrets = preferUpperFrets;
    loadedEventsPreferredMinFret = preferredMinFret;
}

FretboardSnapshot GuitarChannelizerAudioProcessor::getFretboardSnapshot() const noexcept
{
    FretboardSnapshot snapshot;

    for (size_t i = 0; i < snapshot.active.size(); ++i)
    {
        snapshot.active[i] = fretActivity[i].load (std::memory_order_relaxed) > 0;
        snapshot.chordRoot[i] = liveRootActivity[i].load (std::memory_order_relaxed) > 0;
        if (lowerOctaveFretActivity[i].load (std::memory_order_relaxed) > 0)
            snapshot.octaveOffset[i] = -1;
        else if (upperOctaveFretActivity[i].load (std::memory_order_relaxed) > 0)
            snapshot.octaveOffset[i] = 1;
        else
            snapshot.octaveOffset[i] = 0;
    }

    return snapshot;
}

juce::String GuitarChannelizerAudioProcessor::getLiveChordName() const
{
    std::vector<int> activeNotes;
    activeNotes.reserve (GuitarFingering::numStrings);

    for (int note = 0; note < static_cast<int> (livePitchActivity.size()); ++note)
        if (livePitchActivity[static_cast<size_t> (note)].load (std::memory_order_relaxed) > 0)
            activeNotes.push_back (note);

    const auto latchChordName = getBoolParameter (ParameterIDs::latchChordName);
    const auto releaseDelayMs = juce::jlimit (0, 1000, getIntParameter (ParameterIDs::chordLatchReleaseMs));

    if (activeNotes.size() < 3)
    {
        if (latchChordName)
        {
            const std::lock_guard<std::mutex> lock (liveChordMutex);
            return latchedLiveChordName;
        }

        return "Chord: --";
    }

    const auto chordName = "Chord: " + identifyChord (activeNotes).name;

    if (! latchChordName)
    {
        const std::lock_guard<std::mutex> lock (liveChordMutex);
        latchedLiveChordName = chordName;
        latchedLiveChordNotes = activeNotes;
        pendingLiveChordName = {};
        pendingLiveChordNotes.clear();
        pendingLiveChordStartMs = 0.0;
        return chordName;
    }

    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    {
        const std::lock_guard<std::mutex> lock (liveChordMutex);
        const auto looksLikeReleaseFragment = ! latchedLiveChordNotes.empty()
            && activeNotes.size() < latchedLiveChordNotes.size();

        if (looksLikeReleaseFragment && releaseDelayMs > 0)
        {
            if (pendingLiveChordName != chordName || pendingLiveChordNotes != activeNotes)
            {
                pendingLiveChordName = chordName;
                pendingLiveChordNotes = activeNotes;
                pendingLiveChordStartMs = nowMs;
            }

            if (nowMs - pendingLiveChordStartMs < static_cast<double> (releaseDelayMs))
                return latchedLiveChordName;
        }

        latchedLiveChordName = chordName;
        latchedLiveChordNotes = activeNotes;
        pendingLiveChordName = {};
        pendingLiveChordNotes.clear();
        pendingLiveChordStartMs = 0.0;
    }

    return chordName;
}

void GuitarChannelizerAudioProcessor::setLiveHandTargetFret (int fret) noexcept
{
    liveHandTargetFret.store (juce::jlimit (0, GuitarFingering::maxFret, fret), std::memory_order_relaxed);
    liveVoicingRefreshRequested.store (true, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::clearLiveHandTargetFret() noexcept
{
    liveHandTargetFret.store (-1, std::memory_order_relaxed);
    liveVoicingRefreshRequested.store (true, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::requestLiveVoicingRefresh() noexcept
{
    liveVoicingRefreshRequested.store (true, std::memory_order_relaxed);
}

GeneratedMelodySnapshot GuitarChannelizerAudioProcessor::getGeneratedMelodySnapshot() const
{
    const std::lock_guard<std::mutex> lock (generatedMutex);
    return generatedMelody;
}

juce::String GuitarChannelizerAudioProcessor::generateMelody()
{
    const auto bars = juce::jlimit (4, 32, getIntParameter (ParameterIDs::melodyBars));
    const auto complexity = juce::jlimit (0, 100, getIntParameter (ParameterIDs::melodyComplexity));
    const auto density = juce::jlimit (0, 100, getIntParameter (ParameterIDs::melodyDensity));
    const auto rootMask = clampRootMask (getIntParameter (ParameterIDs::melodyRootMask));
    const auto keyCount = juce::jlimit (1, 12, getIntParameter (ParameterIDs::melodyKeyCount));
    const auto scaleMask = clampScaleMask (getIntParameter (ParameterIDs::melodyScaleMask));
    auto minNote = juce::jlimit (36, 96, getIntParameter (ParameterIDs::melodyMinNote));
    auto maxNote = juce::jlimit (36, 96, getIntParameter (ParameterIDs::melodyMaxNote));
    if (minNote > maxNote)
        std::swap (minNote, maxNote);
    if (maxNote - minNote < 7)
        maxNote = juce::jmin (96, minNote + 7);
    const auto pureLegato = getBoolParameter (ParameterIDs::melodyPureLegato);
    const auto forceFirstNote = getBoolParameter (ParameterIDs::melodyForceFirstNote);
    const auto legatoPoly = juce::jlimit (0, 100, getIntParameter (ParameterIDs::melodyLegatoPoly));
    const auto polyDensity = juce::jlimit (2, 6, getIntParameter (ParameterIDs::melodyPolyDensity));
    const auto guitaristic = getBoolParameter (ParameterIDs::melodyGuitaristic);
    const auto inverted = getBoolParameter (ParameterIDs::invertChannels);
    const auto fingeringOptions = readFingeringOptions();
    const auto totalSteps = bars * 16;
    const auto stepPpq = 0.25;
    const auto melodyLengthPpq = static_cast<double> (bars) * 4.0;

    std::vector<int> enabledScales;
    for (int i = 0; i < static_cast<int> (scaleDefinitions().size()); ++i)
        if ((scaleMask & (1 << i)) != 0)
            enabledScales.push_back (i);

    if (enabledScales.empty())
        enabledScales.push_back (0);

    juce::Random random (static_cast<int64> (juce::Time::getMillisecondCounterHiRes() * 1000.0));
    std::vector<int> rootPool;

    for (int root = 0; root < 12; ++root)
        if ((rootMask & (1 << root)) != 0)
            rootPool.push_back (root);

    if (rootPool.empty())
        rootPool.push_back (0);

    while (static_cast<int> (rootPool.size()) > keyCount)
    {
        const auto index = random.nextInt (static_cast<int> (rootPool.size()));
        rootPool.erase (rootPool.begin() + index);
    }

    for (int i = static_cast<int> (rootPool.size()) - 1; i > 0; --i)
        std::swap (rootPool[static_cast<size_t> (i)], rootPool[static_cast<size_t> (random.nextInt (i + 1))]);

    std::vector<GeneratedMelodyNote> notes;
    std::vector<GeneratedMidiEvent> events;
    std::vector<GeneratedScaleSection> scaleSections;
    GuitarPosition previousPosition;
    auto previousPitch = juce::jlimit (minNote, maxNote, 60 + rootPool.front());
    auto phraseDirection = random.nextBool() ? 1 : -1;

    const auto densityNorm = static_cast<double> (density) / 100.0;
    const auto baseProbability = 0.015 + std::pow (densityNorm, 1.85) * 0.50;
    const auto polyNorm = static_cast<double> (legatoPoly) / 100.0;
    const auto chordProbability = std::pow (polyNorm, 2.25) * 0.58;
    const auto maxLeap = juce::jlimit (2, 10, 2 + complexity / 13);
    auto currentRoot = rootPool[static_cast<size_t> (random.nextInt (static_cast<int> (rootPool.size())))];
    auto currentScaleIndex = enabledScales[static_cast<size_t> (random.nextInt (static_cast<int> (enabledScales.size())))];

    for (int bar = 0; bar < bars; ++bar)
    {
        if (bar > 0)
        {
            auto rootChangeProbability = rootPool.size() > 1 ? 0.18 + densityNorm * 0.10 : 0.0;
            auto scaleChangeProbability = enabledScales.size() > 1 ? 0.20 + densityNorm * 0.08 : 0.0;

            if (bar % 4 == 0)
            {
                rootChangeProbability += 0.18;
                scaleChangeProbability += 0.12;
            }
            else if (bar % 2 == 0)
            {
                rootChangeProbability += 0.08;
                scaleChangeProbability += 0.06;
            }

            if (random.nextDouble() < rootChangeProbability)
            {
                auto nextRoot = currentRoot;
                for (int attempts = 0; attempts < 6 && nextRoot == currentRoot; ++attempts)
                    nextRoot = rootPool[static_cast<size_t> (random.nextInt (static_cast<int> (rootPool.size())))];
                currentRoot = nextRoot;
            }

            if (random.nextDouble() < scaleChangeProbability)
            {
                auto nextScale = currentScaleIndex;
                for (int attempts = 0; attempts < 6 && nextScale == currentScaleIndex; ++attempts)
                    nextScale = enabledScales[static_cast<size_t> (random.nextInt (static_cast<int> (enabledScales.size())))];
                currentScaleIndex = nextScale;
            }
        }

        const auto keyName = getKeyNames()[currentRoot + 1];
        scaleSections.push_back ({ bar, currentRoot, currentScaleIndex, keyName + " " + scaleDefinitions()[static_cast<size_t> (currentScaleIndex)].name });
    }

    for (int step = 0; step < totalSteps; ++step)
    {
        const auto bar = step / 16;
        const auto beatStep = step % 4;
        auto probability = baseProbability;

        if (beatStep == 0)
            probability += 0.035 + densityNorm * 0.05;
        if ((step % 16) == 0)
            probability += 0.06 + densityNorm * 0.08;

        probability = juce::jlimit (0.01, 0.82, probability);
        if (!(forceFirstNote && step == 0) && random.nextDouble() > probability)
            continue;

        if (step % 16 == 0 && random.nextDouble() < 0.55)
            phraseDirection *= -1;

        const auto root = scaleSections[static_cast<size_t> (bar)].root;
        const auto scaleIndex = scaleSections[static_cast<size_t> (bar)].scaleIndex;
        const auto& scale = scaleDefinitions()[static_cast<size_t> (scaleIndex)];

        std::vector<int> pitchPool;
        for (int pitch = minNote; pitch <= maxNote; ++pitch)
            if (pitchInScale (pitch, root, scale))
                pitchPool.push_back (pitch);

        if (pitchPool.empty())
            continue;

        const auto stepMove = random.nextDouble() < 0.72
            ? phraseDirection * (1 + random.nextInt (2))
            : phraseDirection * (3 + random.nextInt (juce::jmax (1, maxLeap - 2)));
        auto pitch = pitchPool[static_cast<size_t> (nearestPitchIndex (pitchPool, previousPitch + stepMove))];

        if (pitch <= minNote + 2)
            phraseDirection = 1;
        else if (pitch >= maxNote - 2)
            phraseDirection = -1;

        if (step % 16 == 0)
        {
            pitch = pitchPool[static_cast<size_t> (nearestPitchIndex (pitchPool, previousPitch + phraseDirection * 2))];
        }

        const auto chordSize = random.nextDouble() < chordProbability
            ? juce::jlimit (2, polyDensity, 2 + random.nextInt (juce::jmax (1, polyDensity - 1)))
            : 1;

        std::vector<int> chordNotes { pitch };
        auto poolIndex = static_cast<int> (std::find (pitchPool.begin(), pitchPool.end(), pitch) - pitchPool.begin());

        for (int voice = 1; voice < chordSize; ++voice)
        {
            const auto nextIndex = poolIndex + voice * 2;
            if (nextIndex >= 0 && nextIndex < static_cast<int> (pitchPool.size()))
                chordNotes.push_back (pitchPool[static_cast<size_t> (nextIndex)]);
        }

        const auto positions = fingering.chooseChordPositions (chordNotes, previousPosition, fingeringOptions);
        const auto start = static_cast<double> (step) * stepPpq;
        const auto lengthSteps = juce::jlimit (1, 8, 1 + (100 - density) / 28 + (legatoPoly < 30 ? 1 : 0));
        const auto length = juce::jmin (static_cast<double> (lengthSteps) * stepPpq * 0.92, melodyLengthPpq - start);

        for (size_t i = 0; i < chordNotes.size(); ++i)
        {
            auto note = GeneratedMelodyNote {};
            note.note = chordNotes[i];
            note.velocity = juce::jlimit (45, 118, 72 + random.nextInt (34) + complexity / 6);
            note.startPpq = start;
            note.lengthPpq = juce::jmax (0.08, length);

            if (i < positions.size() && positions[i].isPlayable())
            {
                note.stringIndex = positions[i].stringIndex;
                note.fret = positions[i].fret;
                previousPosition = positions[i];
            }
            else
            {
                const auto position = fingering.choosePosition (note.note, previousPosition, fingeringOptions);
                note.stringIndex = position.stringIndex;
                note.fret = position.fret;
                previousPosition = position;
            }

            if (guitaristic)
            {
                note.stringIndex = juce::jlimit (0, GuitarFingering::numStrings - 1, note.stringIndex);
                note.channel = GuitarFingering::channelForString (note.stringIndex, inverted);
            }
            else
            {
                note.channel = 1;
            }
            notes.push_back (note);

            auto noteOn = juce::MidiMessage::noteOn (note.channel, note.note, static_cast<juce::uint8> (note.velocity));
            noteOn.setTimeStamp (note.startPpq);
            events.push_back ({ noteOn, note.startPpq, note.stringIndex, note.fret });

            auto noteOff = juce::MidiMessage::noteOff (note.channel, note.note);
            noteOff.setTimeStamp (note.startPpq + note.lengthPpq);
            events.push_back ({ noteOff, note.startPpq + note.lengthPpq, note.stringIndex, note.fret });
        }

        previousPitch = pitch;
    }

    std::sort (notes.begin(), notes.end(), [] (const auto& a, const auto& b)
    {
        if (std::abs (a.startPpq - b.startPpq) > 0.0000001)
            return a.startPpq < b.startPpq;

        return a.note < b.note;
    });

    for (size_t i = 0; i < notes.size(); ++i)
    {
        auto nextStart = melodyLengthPpq;
        for (size_t j = i + 1; j < notes.size(); ++j)
        {
            if (notes[j].startPpq > notes[i].startPpq + 0.0000001)
            {
                nextStart = notes[j].startPpq;
                break;
            }
        }

        const auto available = juce::jmax (0.08, nextStart - notes[i].startPpq);
        notes[i].lengthPpq = pureLegato ? available : juce::jmin (notes[i].lengthPpq, available * 0.92);
    }

    events.clear();
    for (const auto& note : notes)
    {
        auto noteOn = juce::MidiMessage::noteOn (note.channel, note.note, static_cast<juce::uint8> (note.velocity));
        noteOn.setTimeStamp (note.startPpq);
        events.push_back ({ noteOn, note.startPpq, note.stringIndex, note.fret });

        auto noteOff = juce::MidiMessage::noteOff (note.channel, note.note);
        noteOff.setTimeStamp (note.startPpq + note.lengthPpq);
        events.push_back ({ noteOff, note.startPpq + note.lengthPpq, note.stringIndex, note.fret });
    }

    std::sort (events.begin(), events.end(), [] (const auto& a, const auto& b)
    {
        if (std::abs (a.ppq - b.ppq) > 0.0000001)
            return a.ppq < b.ppq;

        return a.message.isNoteOff() && b.message.isNoteOn();
    });

    const auto noteCount = static_cast<int> (notes.size());

    {
        const std::lock_guard<std::mutex> lock (generatedMutex);
        generatedMelody.notes = std::move (notes);
        generatedMelody.scaleSections = std::move (scaleSections);
        generatedMelody.bars = bars;
        generatedEvents = std::move (events);
        generatedLengthPpq = melodyLengthPpq;
    }

    return "Generated " + juce::String (noteCount) + " notes over " + juce::String (bars) + " bars.";
}

int GuitarChannelizerAudioProcessor::getMelodyScaleMask() const noexcept
{
    return clampScaleMask (getIntParameter (ParameterIDs::melodyScaleMask));
}

void GuitarChannelizerAudioProcessor::setMelodyScaleMask (int mask)
{
    auto* parameter = parameters.getParameter (ParameterIDs::melodyScaleMask);
    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (clampScaleMask (mask))));
    parameter->endChangeGesture();
}

juce::String GuitarChannelizerAudioProcessor::getMelodyScaleDescription() const
{
    const auto mask = getMelodyScaleMask();
    juce::StringArray selected;
    const auto names = getScaleNames();

    for (int i = 0; i < names.size(); ++i)
        if ((mask & (1 << i)) != 0)
            selected.add (names[i]);

    return selected.isEmpty() ? "Scales" : selected.joinIntoString (", ");
}

int GuitarChannelizerAudioProcessor::getMelodyRootMask() const noexcept
{
    return clampRootMask (getIntParameter (ParameterIDs::melodyRootMask));
}

void GuitarChannelizerAudioProcessor::setMelodyRootMask (int mask)
{
    auto* parameter = parameters.getParameter (ParameterIDs::melodyRootMask);
    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (clampRootMask (mask))));
    parameter->endChangeGesture();
}

juce::String GuitarChannelizerAudioProcessor::getMelodyRootDescription() const
{
    const auto mask = getMelodyRootMask();
    juce::StringArray selected;
    const auto names = getKeyNames();

    for (int i = 0; i < 12; ++i)
        if ((mask & (1 << i)) != 0)
            selected.add (names[i + 1]);

    if (selected.size() == 12)
        return "All roots";

    return selected.isEmpty() ? "Roots" : selected.joinIntoString (", ");
}

namespace
{
    void setParameterValue (juce::AudioProcessorValueTreeState& parameters, const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    }
}

void GuitarChannelizerAudioProcessor::randomizeGenerationSettings()
{
    juce::Random random (static_cast<int64> (juce::Time::getMillisecondCounterHiRes() * 1000.0));
    setParameterValue (parameters, ParameterIDs::melodyComplexity, static_cast<float> (12 + random.nextInt (79)));
    setParameterValue (parameters, ParameterIDs::melodyDensity, static_cast<float> (4 + random.nextInt (58)));
    setParameterValue (parameters, ParameterIDs::melodyBars, static_cast<float> (4 + random.nextInt (8) * 4));
    setParameterValue (parameters, ParameterIDs::melodyKeyCount, static_cast<float> (1 + random.nextInt (juce::jmax (1, 4))));
    setParameterValue (parameters, ParameterIDs::melodyMinNote, static_cast<float> (40 + random.nextInt (17)));
    setParameterValue (parameters, ParameterIDs::melodyMaxNote, static_cast<float> (68 + random.nextInt (21)));
    setParameterValue (parameters, ParameterIDs::melodyPureLegato, random.nextBool() ? 1.0f : 0.0f);
    setParameterValue (parameters, ParameterIDs::melodyForceFirstNote, 1.0f);
    setParameterValue (parameters, ParameterIDs::melodyLegatoPoly, static_cast<float> (random.nextInt (72)));
    setParameterValue (parameters, ParameterIDs::melodyPolyDensity, static_cast<float> (2 + random.nextInt (5)));
    setParameterValue (parameters, ParameterIDs::melodySpeed, static_cast<float> (juce::jlimit (0, 5, 1 + random.nextInt (4))));

    auto rootMask = 0;
    const auto rootCount = 1 + random.nextInt (4);
    while (__builtin_popcount (static_cast<unsigned int> (rootMask)) < rootCount)
        rootMask |= 1 << random.nextInt (12);
    setMelodyRootMask (rootMask);

    auto scaleMask = 0;
    const auto scaleCount = 1 + random.nextInt (4);
    while (__builtin_popcount (static_cast<unsigned int> (scaleMask)) < scaleCount)
        scaleMask |= 1 << random.nextInt (static_cast<int> (scaleDefinitions().size()));
    setMelodyScaleMask (scaleMask);
}

void GuitarChannelizerAudioProcessor::resetGenerationSettings()
{
    setParameterValue (parameters, ParameterIDs::melodyOutputEnabled, 1.0f);
    setParameterValue (parameters, ParameterIDs::melodyGuitaristic, 1.0f);
    setParameterValue (parameters, ParameterIDs::melodySpeed, 2.0f);
    setParameterValue (parameters, ParameterIDs::melodyComplexity, 42.0f);
    setParameterValue (parameters, ParameterIDs::melodyDensity, 35.0f);
    setParameterValue (parameters, ParameterIDs::melodyBars, 8.0f);
    setParameterValue (parameters, ParameterIDs::melodyKeyCount, 1.0f);
    setParameterValue (parameters, ParameterIDs::melodyMinNote, 45.0f);
    setParameterValue (parameters, ParameterIDs::melodyMaxNote, 76.0f);
    setParameterValue (parameters, ParameterIDs::melodyPureLegato, 0.0f);
    setParameterValue (parameters, ParameterIDs::melodyForceFirstNote, 1.0f);
    setParameterValue (parameters, ParameterIDs::melodyLegatoPoly, 0.0f);
    setParameterValue (parameters, ParameterIDs::melodyPolyDensity, 3.0f);
    setMelodyRootMask ((1 << 12) - 1);
    setMelodyScaleMask (1);
}

int GuitarChannelizerAudioProcessor::activeNoteIndex (int channel, int note) noexcept
{
    return juce::jlimit (0, 15, channel - 1) * 128 + juce::jlimit (0, 127, note);
}

int GuitarChannelizerAudioProcessor::fretIndex (int stringIndex, int fret) noexcept
{
    return juce::jlimit (0, GuitarFingering::numStrings - 1, stringIndex) * FretboardSnapshot::frets
         + juce::jlimit (0, FretboardSnapshot::frets - 1, fret);
}

bool GuitarChannelizerAudioProcessor::getBoolParameter (const char* id) const noexcept
{
    if (const auto* value = parameters.getRawParameterValue (id))
        return value->load() > 0.5f;

    return false;
}

int GuitarChannelizerAudioProcessor::getIntParameter (const char* id) const noexcept
{
    if (const auto* value = parameters.getRawParameterValue (id))
        return juce::roundToInt (value->load());

    return 0;
}

GuitarFingeringOptions GuitarChannelizerAudioProcessor::readFingeringOptions() const noexcept
{
    GuitarFingeringOptions options { getBoolParameter (ParameterIDs::preferUpperFrets),
                                     juce::jlimit (1, GuitarFingering::maxFret, getIntParameter (ParameterIDs::preferredMinFret)) };

    const auto handTarget = liveHandTargetFret.load (std::memory_order_relaxed);
    if (handTarget >= 0)
    {
        options.handTargetEnabled = true;
        options.handTargetFret = juce::jlimit (0, GuitarFingering::maxFret, handTarget);
        options.handTargetRadius = juce::jlimit (1, 8, getIntParameter (ParameterIDs::handTargetRadius));
    }

    return options;
}

bool GuitarChannelizerAudioProcessor::readHostPosition (double& ppqPosition, double& bpm, bool& isPlaying) const
{
    const auto* playHead = getPlayHead();
    if (playHead == nullptr)
        return false;

    const auto position = playHead->getPosition();
    if (! position.hasValue())
        return false;

    isPlaying = position->getIsPlaying();

    if (const auto hostBpm = position->getBpm())
        bpm = juce::jlimit (20.0, 320.0, *hostBpm);

    if (const auto ppq = position->getPpqPosition())
    {
        ppqPosition = *ppq;
        return true;
    }

    return false;
}

void GuitarChannelizerAudioProcessor::emitGeneratedMelody (juce::MidiBuffer& output,
                                                           double blockStartPpq,
                                                           double bpm,
                                                           int numSamples)
{
    if (numSamples <= 0 || currentSampleRate <= 0.0 || bpm <= 0.0)
        return;

    std::unique_lock<std::mutex> lock (generatedMutex, std::try_to_lock);
    if (! lock.owns_lock() || generatedEvents.empty() || generatedLengthPpq <= 0.0)
        return;

    const auto speed = melodySpeedMultiplierForIndex (getIntParameter (ParameterIDs::melodySpeed));
    const auto hostPpqPerSample = bpm / (60.0 * currentSampleRate);
    const auto ppqPerSample = hostPpqPerSample * speed;
    const auto blockLengthPpq = static_cast<double> (numSamples) * ppqPerSample;
    const auto melodyPpq = juce::jmax (0.0, blockStartPpq - melodyStartHostPpq.load (std::memory_order_relaxed));
    const auto startInLoop = std::fmod (melodyPpq * speed, generatedLengthPpq);
    auto endInLoop = startInLoop + blockLengthPpq;

    if (endInLoop < generatedLengthPpq)
    {
        for (const auto& event : generatedEvents)
            addGeneratedEventIfInRange (event, startInLoop, endInLoop, 0.0, generatedLengthPpq, ppqPerSample, numSamples, output);
    }
    else
    {
        for (const auto& event : generatedEvents)
        {
            addGeneratedEventIfInRange (event, startInLoop, generatedLengthPpq, 0.0, generatedLengthPpq, ppqPerSample, numSamples, output);
            addGeneratedEventIfInRange (event, 0.0, endInLoop - generatedLengthPpq, generatedLengthPpq - startInLoop, generatedLengthPpq, ppqPerSample, numSamples, output);
        }
    }
}

void GuitarChannelizerAudioProcessor::processLiveMidi (const juce::MidiBuffer& input,
                                                       juce::MidiBuffer& output,
                                                       bool inverted)
{
    const auto useInputChannels = getBoolParameter (ParameterIDs::useInputChannels);

    if (useInputChannels)
    {
        processLiveInputChannelMidi (input, output, inverted);
        return;
    }

    for (const auto metadata : input)
    {
        const auto samplePosition = metadata.samplePosition;
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            auto& activeNote = activeLiveNotes[static_cast<size_t> (activeNoteIndex (message.getChannel(), message.getNoteNumber()))];

            if (activeNote.active)
            {
                clearMappedLiveNote (activeNote, samplePosition, output);
                markLivePitch (activeNote.note, -1);
                activeNote = {};
            }

            activeNote.inputChannel = message.getChannel();
            activeNote.note = message.getNoteNumber();
            activeNote.velocity = juce::jlimit (1, 127, static_cast<int> (message.getVelocity()));
            activeNote.preferredString = useInputChannels ? GuitarFingering::stringForChannel (message.getChannel(), inverted) : -1;
            activeNote.active = true;
            activeNote.mapped = false;
            activeNote.order = ++liveNoteOrderCounter;
            markLivePitch (activeNote.note, 1);
            refreshLiveVoicing (samplePosition, output, inverted);

            continue;
        }

        if (message.isNoteOff())
        {
            auto& activeNote = activeLiveNotes[static_cast<size_t> (activeNoteIndex (message.getChannel(), message.getNoteNumber()))];
            if (activeNote.active)
            {
                clearMappedLiveNote (activeNote, samplePosition, output);
                markLivePitch (activeNote.note, -1);
                activeNote = {};
                refreshLiveVoicing (samplePosition, output, inverted);
            }

            continue;
        }

        output.addEvent (message, samplePosition);
    }

    if (liveVoicingRefreshRequested.exchange (false, std::memory_order_relaxed))
        refreshLiveVoicing (0, output, inverted);
}

void GuitarChannelizerAudioProcessor::processLiveInputChannelMidi (const juce::MidiBuffer& input,
                                                                   juce::MidiBuffer& output,
                                                                   bool inverted)
{
    const auto showSameChannelNotes = getBoolParameter (ParameterIDs::showSameChannelNotes);

    for (const auto metadata : input)
    {
        const auto samplePosition = metadata.samplePosition;
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            auto& activeNote = activeLiveNotes[static_cast<size_t> (activeNoteIndex (message.getChannel(), message.getNoteNumber()))];

            if (activeNote.active)
            {
                if (activeNote.mapped)
                    markDisplayedFret (activeNote.stringIndex, activeNote.fret, activeNote.displayOctaveOffset, -1);

                markLivePitch (activeNote.note, -1);
                activeNote = {};
            }

            const auto stringIndex = GuitarFingering::stringForChannel (message.getChannel(), inverted);
            const auto rawFret = stringIndex >= 0
                ? message.getNoteNumber() - GuitarFingering::standardTuning[static_cast<size_t> (stringIndex)]
                : -1;
            auto fret = rawFret;
            auto octaveOffset = 0;

            if (stringIndex >= 0)
            {
                while (fret < 0)
                {
                    fret += 12;
                    --octaveOffset;
                }

                while (fret > GuitarFingering::maxFret)
                {
                    fret -= 12;
                    ++octaveOffset;
                }
            }

            const auto isPlayableOnInputString = stringIndex >= 0 && fret >= 0 && fret <= GuitarFingering::maxFret;
            auto canShowOnString = isPlayableOnInputString;

            if (canShowOnString && ! showSameChannelNotes)
            {
                for (const auto& otherNote : activeLiveNotes)
                {
                    if (&otherNote == &activeNote || ! otherNote.active || ! otherNote.mapped)
                        continue;

                    if (otherNote.stringIndex == stringIndex)
                    {
                        canShowOnString = false;
                        break;
                    }
                }
            }

            activeNote.inputChannel = message.getChannel();
            activeNote.note = message.getNoteNumber();
            activeNote.velocity = juce::jlimit (1, 127, static_cast<int> (message.getVelocity()));
            activeNote.preferredString = stringIndex;
            activeNote.active = true;
            activeNote.mapped = canShowOnString;
            activeNote.order = ++liveNoteOrderCounter;
            activeNote.outputChannel = message.getChannel();
            activeNote.stringIndex = canShowOnString ? stringIndex : -1;
            activeNote.fret = canShowOnString ? fret : -1;
            activeNote.displayOctaveOffset = canShowOnString ? octaveOffset : 0;

            markLivePitch (activeNote.note, 1);
            if (activeNote.mapped)
                markDisplayedFret (activeNote.stringIndex, activeNote.fret, activeNote.displayOctaveOffset, 1);

            updateLiveRootMarkers();
            output.addEvent (message, samplePosition);
            continue;
        }

        if (message.isNoteOff())
        {
            auto& activeNote = activeLiveNotes[static_cast<size_t> (activeNoteIndex (message.getChannel(), message.getNoteNumber()))];
            if (activeNote.active)
            {
                if (activeNote.mapped)
                    markDisplayedFret (activeNote.stringIndex, activeNote.fret, activeNote.displayOctaveOffset, -1);

                markLivePitch (activeNote.note, -1);
                activeNote = {};
                updateLiveRootMarkers();
            }

            output.addEvent (message, samplePosition);
            continue;
        }

        output.addEvent (message, samplePosition);
    }

    liveVoicingRefreshRequested.store (false, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::refreshLiveVoicing (int samplePosition, juce::MidiBuffer& output, bool inverted)
{
    std::vector<ActiveLiveNote*> heldNotes;
    heldNotes.reserve (GuitarFingering::numStrings);

    for (auto& activeNote : activeLiveNotes)
        if (activeNote.active)
            heldNotes.push_back (&activeNote);

    if (heldNotes.empty())
        return;

    std::sort (heldNotes.begin(), heldNotes.end(), [] (const auto* a, const auto* b)
    {
        return a->order > b->order;
    });

    if (heldNotes.size() > static_cast<size_t> (GuitarFingering::numStrings))
        heldNotes.resize (GuitarFingering::numStrings);

    std::sort (heldNotes.begin(), heldNotes.end(), [] (const auto* a, const auto* b)
    {
        if (a->note != b->note)
            return a->note < b->note;

        return a->order < b->order;
    });

    std::vector<int> notes;
    std::vector<int> preferredStrings;
    notes.reserve (heldNotes.size());
    preferredStrings.reserve (heldNotes.size());

    std::array<bool, GuitarFingering::numStrings> explicitStringUsed {};
    for (const auto* activeNote : heldNotes)
        if (activeNote->preferredString >= 0)
            explicitStringUsed[static_cast<size_t> (activeNote->preferredString)] = true;

    auto explicitStringCount = 0;
    for (const auto used : explicitStringUsed)
        if (used)
            ++explicitStringCount;

    // Channel-1-only input is normal keyboard-style MIDI in Logic, so infer strings.
    // Multi-channel 1-6 input, or a single non-channel-1 note, is treated as an explicit string hint.
    const auto useExplicitStrings = explicitStringCount > 1
        || (heldNotes.size() == 1 && heldNotes.front()->inputChannel != 1);

    for (const auto* activeNote : heldNotes)
    {
        notes.push_back (activeNote->note);
        preferredStrings.push_back (useExplicitStrings ? activeNote->preferredString : -1);
    }

    const auto positions = fingering.chooseChordPositions (notes, lastLivePosition, readFingeringOptions(), &preferredStrings);
    std::array<bool, GuitarFingering::numStrings> usedStrings {};

    for (size_t i = 0; i < heldNotes.size(); ++i)
    {
        auto& activeNote = *heldNotes[i];
        const auto position = i < positions.size() ? positions[i] : GuitarPosition {};

        if (! position.isPlayable() || usedStrings[static_cast<size_t> (position.stringIndex)])
        {
            clearMappedLiveNote (activeNote, samplePosition, output);
            continue;
        }

        usedStrings[static_cast<size_t> (position.stringIndex)] = true;
        const auto outputChannel = GuitarFingering::channelForString (position.stringIndex, inverted);
        const auto mappingChanged = ! activeNote.mapped
            || activeNote.outputChannel != outputChannel
            || activeNote.stringIndex != position.stringIndex
            || activeNote.fret != position.fret;

        if (! mappingChanged)
            continue;

        clearMappedLiveNote (activeNote, samplePosition, output);

        auto noteOn = juce::MidiMessage::noteOn (outputChannel,
                                                 activeNote.note,
                                                 static_cast<juce::uint8> (juce::jlimit (1, 127, activeNote.velocity)));
        output.addEvent (noteOn, samplePosition);

        activeNote.outputChannel = outputChannel;
        activeNote.stringIndex = position.stringIndex;
        activeNote.fret = position.fret;
        activeNote.displayOctaveOffset = 0;
        activeNote.mapped = true;
        lastLivePosition = position;
        markDisplayedFret (activeNote.stringIndex, activeNote.fret, activeNote.displayOctaveOffset, 1);
    }

    for (auto& activeNote : activeLiveNotes)
    {
        if (! activeNote.active || ! activeNote.mapped)
            continue;

        const auto selected = std::find (heldNotes.begin(), heldNotes.end(), &activeNote) != heldNotes.end();
        if (! selected)
            clearMappedLiveNote (activeNote, samplePosition, output);
    }

    updateLiveRootMarkers();
}

void GuitarChannelizerAudioProcessor::clearMappedLiveNote (ActiveLiveNote& activeNote, int samplePosition, juce::MidiBuffer& output)
{
    if (! activeNote.mapped)
        return;

    auto noteOff = juce::MidiMessage::noteOff (activeNote.outputChannel, activeNote.note);
    output.addEvent (noteOff, samplePosition);
    markDisplayedFret (activeNote.stringIndex, activeNote.fret, activeNote.displayOctaveOffset, -1);
    activeNote.displayOctaveOffset = 0;
    activeNote.outputChannel = 0;
    activeNote.stringIndex = -1;
    activeNote.fret = -1;
    activeNote.mapped = false;
    updateLiveRootMarkers();
}

void GuitarChannelizerAudioProcessor::markLivePitch (int note, int delta) noexcept
{
    if (note < 0 || note >= static_cast<int> (livePitchActivity.size()))
        return;

    auto& value = livePitchActivity[static_cast<size_t> (note)];

    if (delta > 0)
    {
        value.fetch_add (delta, std::memory_order_relaxed);
        return;
    }

    auto current = value.load (std::memory_order_relaxed);
    while (current > 0 && ! value.compare_exchange_weak (current, current - 1, std::memory_order_relaxed))
    {
    }
}

void GuitarChannelizerAudioProcessor::markDisplayedFret (int stringIndex, int fret, int octaveOffset, int delta) noexcept
{
    markFret (stringIndex, fret, delta);

    if (stringIndex < 0 || stringIndex >= GuitarFingering::numStrings || fret < 0 || fret >= FretboardSnapshot::frets || octaveOffset == 0)
        return;

    auto& value = octaveOffset < 0
        ? lowerOctaveFretActivity[static_cast<size_t> (fretIndex (stringIndex, fret))]
        : upperOctaveFretActivity[static_cast<size_t> (fretIndex (stringIndex, fret))];

    if (delta > 0)
    {
        value.fetch_add (delta, std::memory_order_relaxed);
        return;
    }

    auto current = value.load (std::memory_order_relaxed);
    while (current > 0 && ! value.compare_exchange_weak (current, current - 1, std::memory_order_relaxed))
    {
    }
}

void GuitarChannelizerAudioProcessor::markFret (int stringIndex, int fret, int delta) noexcept
{
    if (stringIndex < 0 || stringIndex >= GuitarFingering::numStrings || fret < 0 || fret >= FretboardSnapshot::frets)
        return;

    auto& value = fretActivity[static_cast<size_t> (fretIndex (stringIndex, fret))];

    if (delta > 0)
    {
        value.fetch_add (delta, std::memory_order_relaxed);
        return;
    }

    auto current = value.load (std::memory_order_relaxed);
    while (current > 0 && ! value.compare_exchange_weak (current, current - 1, std::memory_order_relaxed))
    {
    }
}

void GuitarChannelizerAudioProcessor::clearLiveRootMarkers() noexcept
{
    for (auto& value : liveRootActivity)
        value.store (0, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::updateLiveRootMarkers() noexcept
{
    clearLiveRootMarkers();

    std::vector<int> activeNotes;
    activeNotes.reserve (GuitarFingering::numStrings);

    for (const auto& activeNote : activeLiveNotes)
        if (activeNote.active)
            activeNotes.push_back (activeNote.note);

    const auto chord = identifyChord (activeNotes);
    if (chord.rootPitchClass < 0)
        return;

    for (const auto& activeNote : activeLiveNotes)
    {
        if (! activeNote.active || ! activeNote.mapped || positiveMod12 (activeNote.note) != chord.rootPitchClass)
            continue;

        liveRootActivity[static_cast<size_t> (fretIndex (activeNote.stringIndex, activeNote.fret))].store (1, std::memory_order_relaxed);
    }
}

void GuitarChannelizerAudioProcessor::clearFretboard() noexcept
{
    for (auto& value : fretActivity)
        value.store (0, std::memory_order_relaxed);
    for (auto& value : lowerOctaveFretActivity)
        value.store (0, std::memory_order_relaxed);
    for (auto& value : upperOctaveFretActivity)
        value.store (0, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::resetLiveState() noexcept
{
    std::fill (activeLiveNotes.begin(), activeLiveNotes.end(), ActiveLiveNote {});
    lastLivePosition = {};
    liveNoteOrderCounter = 0;
    liveVoicingRefreshRequested.store (false, std::memory_order_relaxed);
    clearFretboard();
    clearLiveRootMarkers();
    for (auto& value : livePitchActivity)
        value.store (0, std::memory_order_relaxed);
}

void GuitarChannelizerAudioProcessor::addGeneratedEventIfInRange (const GeneratedMidiEvent& event,
                                                                  double startPpq,
                                                                  double endPpq,
                                                                  double blockOffsetPpq,
                                                                  double melodyLengthPpq,
                                                                  double ppqPerSample,
                                                                  int numSamples,
                                                                  juce::MidiBuffer& output)
{
    if (event.ppq < startPpq || event.ppq >= endPpq)
        return;

    auto eventOffsetPpq = blockOffsetPpq + event.ppq - startPpq;
    if (eventOffsetPpq < 0.0)
        eventOffsetPpq += melodyLengthPpq;

    const auto sample = juce::jlimit (0, numSamples - 1, static_cast<int> (std::round (eventOffsetPpq / ppqPerSample)));

    if (event.message.isNoteOn())
        markFret (event.stringIndex, event.fret, 1);
    else if (event.message.isNoteOff())
        markFret (event.stringIndex, event.fret, -1);

    output.addEvent (event.message, sample);
}

juce::MidiMessageSequence GuitarChannelizerAudioProcessor::buildProcessedSequence (const juce::MidiFile& midiFile,
                                                                                   short ticksPerQuarter) const
{
    juce::ignoreUnused (ticksPerQuarter);

    std::vector<SourceMidiEvent> sourceEvents;
    auto order = 0;

    for (int track = 0; track < midiFile.getNumTracks(); ++track)
    {
        if (const auto* sequence = midiFile.getTrack (track))
        {
            for (int eventIndex = 0; eventIndex < sequence->getNumEvents(); ++eventIndex)
            {
                if (const auto* event = sequence->getEventPointer (eventIndex))
                    sourceEvents.push_back ({ event->message, event->message.getTimeStamp(), order++ });
            }
        }
    }

    std::stable_sort (sourceEvents.begin(), sourceEvents.end(), [] (const auto& a, const auto& b)
    {
        if (std::abs (a.tick - b.tick) > 0.0000001)
            return a.tick < b.tick;

        if (a.message.isNoteOff() != b.message.isNoteOff())
            return a.message.isNoteOff();

        return a.order < b.order;
    });

    const auto inverted = getBoolParameter (ParameterIDs::invertChannels);
    const auto fingeringOptions = readFingeringOptions();
    std::array<std::vector<GuitarPosition>, 16 * 128> activeFileNotes;
    GuitarPosition previous;
    juce::MidiMessageSequence processed;

    auto addProcessedEvent = [&] (const juce::MidiMessage& message, GuitarPosition position)
    {
        juce::ignoreUnused (position);
        processed.addEvent (message);
    };

    for (size_t index = 0; index < sourceEvents.size();)
    {
        const auto tick = sourceEvents[index].tick;
        auto end = index + 1;

        while (end < sourceEvents.size() && std::abs (sourceEvents[end].tick - tick) <= 0.0000001)
            ++end;

        for (auto i = index; i < end; ++i)
        {
            auto message = sourceEvents[i].message;
            if (! message.isNoteOff())
                continue;

            GuitarPosition position;
            auto& stack = activeFileNotes[static_cast<size_t> (activeNoteIndex (message.getChannel(), message.getNoteNumber()))];
            if (! stack.empty())
            {
                position = stack.back();
                stack.pop_back();
                message.setChannel (GuitarFingering::channelForString (position.stringIndex, inverted));
            }

            addProcessedEvent (message, position);
        }

        std::vector<size_t> noteOnIndexes;
        std::vector<int> noteNumbers;

        for (auto i = index; i < end; ++i)
        {
            if (sourceEvents[i].message.isNoteOn())
            {
                noteOnIndexes.push_back (i);
                noteNumbers.push_back (sourceEvents[i].message.getNoteNumber());
            }
            else if (! sourceEvents[i].message.isNoteOff())
            {
                addProcessedEvent (sourceEvents[i].message, {});
            }
        }

        const auto positions = fingering.chooseChordPositions (noteNumbers, previous, fingeringOptions);

        for (size_t noteIndex = 0; noteIndex < noteOnIndexes.size(); ++noteIndex)
        {
            auto message = sourceEvents[noteOnIndexes[noteIndex]].message;

            if (noteIndex < positions.size() && positions[noteIndex].isPlayable())
            {
                const auto position = positions[noteIndex];
                message.setChannel (GuitarFingering::channelForString (position.stringIndex, inverted));
                activeFileNotes[static_cast<size_t> (activeNoteIndex (sourceEvents[noteOnIndexes[noteIndex]].message.getChannel(),
                                                                      message.getNoteNumber()))].push_back (position);
                previous = position;
            }

            addProcessedEvent (message, noteIndex < positions.size() ? positions[noteIndex] : GuitarPosition {});
        }

        index = end;
    }

    processed.sort();
    processed.updateMatchedPairs();

    return processed;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarChannelizerAudioProcessor();
}
