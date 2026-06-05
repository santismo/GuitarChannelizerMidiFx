#pragma once

#include <JuceHeader.h>
#include "GuitarFingering.h"

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

namespace ParameterIDs
{
    static constexpr auto invertChannels = "invertChannels";
    static constexpr auto processLiveInput = "processLiveInput";
    static constexpr auto useInputChannels = "useInputChannels";
    static constexpr auto showSameChannelNotes = "showSameChannelNotes";
    static constexpr auto preferUpperFrets = "preferUpperFrets";
    static constexpr auto preferredMinFret = "preferredMinFret";
    static constexpr auto handTargetRadius = "handTargetRadius";
    static constexpr auto fretboardTheme = "fretboardTheme";
    static constexpr auto fretboardShape = "fretboardShape";
    static constexpr auto fretboardColorSeed = "fretboardColorSeed";
    static constexpr auto fretboardColorMode = "fretboardColorMode";
    static constexpr auto latchChordName = "latchChordName";
    static constexpr auto chordLatchReleaseMs = "chordLatchReleaseMs";
    static constexpr auto melodyOutputEnabled = "melodyOutputEnabled";
    static constexpr auto melodyGuitaristic = "melodyGuitaristic";
    static constexpr auto melodySpeed = "melodySpeed";
    static constexpr auto melodyComplexity = "melodyComplexity";
    static constexpr auto melodyDensity = "melodyDensity";
    static constexpr auto melodyBars = "melodyBars";
    static constexpr auto melodyRootKey = "melodyRootKey";
    static constexpr auto melodyRootMask = "melodyRootMask";
    static constexpr auto melodyKeyCount = "melodyKeyCount";
    static constexpr auto melodyScaleMask = "melodyScaleMask";
    static constexpr auto melodyMinNote = "melodyMinNote";
    static constexpr auto melodyMaxNote = "melodyMaxNote";
    static constexpr auto melodyPureLegato = "melodyPureLegato";
    static constexpr auto melodyForceFirstNote = "melodyForceFirstNote";
    static constexpr auto melodyLegatoPoly = "melodyLegatoPoly";
    static constexpr auto melodyPolyDensity = "melodyPolyDensity";
}

struct FretboardSnapshot
{
    static constexpr int frets = GuitarFingering::maxFret + 1;
    std::array<bool, GuitarFingering::numStrings * frets> active {};
    std::array<bool, GuitarFingering::numStrings * frets> chordRoot {};
    std::array<int, GuitarFingering::numStrings * frets> octaveOffset {};
};

struct GeneratedMelodyNote
{
    int note = 60;
    int velocity = 90;
    int channel = 1;
    int stringIndex = -1;
    int fret = -1;
    double startPpq = 0.0;
    double lengthPpq = 0.25;
};

struct GeneratedScaleSection
{
    int bar = 0;
    int root = 0;
    int scaleIndex = 0;
    juce::String name;
};

struct GeneratedMelodySnapshot
{
    std::vector<GeneratedMelodyNote> notes;
    std::vector<GeneratedScaleSection> scaleSections;
    int bars = 4;
};

class GuitarChannelizerAudioProcessor final : public juce::AudioProcessor
{
public:
    GuitarChannelizerAudioProcessor();
    ~GuitarChannelizerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

    juce::String loadMidiFile (const juce::File& file);
    juce::String exportProcessedMidiFile (const juce::File& file) const;
    juce::String getLoadedMidiSummary() const;
    void refreshLoadedMidiForCurrentSettings();
    FretboardSnapshot getFretboardSnapshot() const noexcept;
    juce::String getLiveChordName() const;
    void setLiveHandTargetFret (int fret) noexcept;
    void clearLiveHandTargetFret() noexcept;
    void requestLiveVoicingRefresh() noexcept;
    GeneratedMelodySnapshot getGeneratedMelodySnapshot() const;
    juce::String generateMelody();
    int getMelodyScaleMask() const noexcept;
    void setMelodyScaleMask (int mask);
    juce::String getMelodyScaleDescription() const;
    int getMelodyRootMask() const noexcept;
    void setMelodyRootMask (int mask);
    juce::String getMelodyRootDescription() const;
    void randomizeGenerationSettings();
    void resetGenerationSettings();

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getKeyNames();
    static juce::StringArray getScaleNames();
    static juce::StringArray getFretboardThemeNames();
    static juce::StringArray getFretboardShapeNames();
    static juce::StringArray getFretboardColorModeNames();
    static juce::StringArray getMelodySpeedNames();

private:
    struct ActiveLiveNote
    {
        int inputChannel = 0;
        int note = -1;
        int velocity = 90;
        int preferredString = -1;
        int stringIndex = -1;
        int fret = -1;
        int displayOctaveOffset = 0;
        int outputChannel = 0;
        bool active = false;
        bool mapped = false;
        uint32_t order = 0;
    };

    struct GeneratedMidiEvent
    {
        juce::MidiMessage message;
        double ppq = 0.0;
        int stringIndex = -1;
        int fret = -1;
    };

    static int activeNoteIndex (int channel, int note) noexcept;
    static int fretIndex (int stringIndex, int fret) noexcept;

    bool getBoolParameter (const char* id) const noexcept;
    int getIntParameter (const char* id) const noexcept;
    GuitarFingeringOptions readFingeringOptions() const noexcept;
    bool readHostPosition (double& ppqPosition, double& bpm, bool& isPlaying) const;
    void processLiveMidi (const juce::MidiBuffer& input, juce::MidiBuffer& output, bool inverted);
    void processLiveInputChannelMidi (const juce::MidiBuffer& input, juce::MidiBuffer& output, bool inverted);
    void refreshLiveVoicing (int samplePosition, juce::MidiBuffer& output, bool inverted);
    void clearMappedLiveNote (ActiveLiveNote& activeNote, int samplePosition, juce::MidiBuffer& output);
    void emitGeneratedMelody (juce::MidiBuffer& output,
                              double blockStartPpq,
                              double bpm,
                              int numSamples);
    void markFret (int stringIndex, int fret, int delta) noexcept;
    void markDisplayedFret (int stringIndex, int fret, int octaveOffset, int delta) noexcept;
    void clearLiveRootMarkers() noexcept;
    void updateLiveRootMarkers() noexcept;
    void markLivePitch (int note, int delta) noexcept;
    void clearFretboard() noexcept;
    void resetLiveState() noexcept;
    void addGeneratedEventIfInRange (const GeneratedMidiEvent& event,
                                     double startPpq,
                                     double endPpq,
                                     double blockOffsetPpq,
                                     double melodyLengthPpq,
                                     double ppqPerSample,
                                     int numSamples,
                                     juce::MidiBuffer& output);

    juce::MidiMessageSequence buildProcessedSequence (const juce::MidiFile& midiFile,
                                                      short ticksPerQuarter) const;

    juce::AudioProcessorValueTreeState parameters;
    GuitarFingering fingering;
    GuitarPosition lastLivePosition;
    std::array<ActiveLiveNote, 16 * 128> activeLiveNotes {};
    std::array<std::atomic<int>, GuitarFingering::numStrings * FretboardSnapshot::frets> fretActivity {};
    std::array<std::atomic<int>, GuitarFingering::numStrings * FretboardSnapshot::frets> liveRootActivity {};
    std::array<std::atomic<int>, GuitarFingering::numStrings * FretboardSnapshot::frets> lowerOctaveFretActivity {};
    std::array<std::atomic<int>, GuitarFingering::numStrings * FretboardSnapshot::frets> upperOctaveFretActivity {};
    std::array<std::atomic<int>, 128> livePitchActivity {};
    std::atomic<int> liveHandTargetFret { -1 };
    std::atomic<bool> liveVoicingRefreshRequested { false };
    uint32_t liveNoteOrderCounter = 0;
    mutable std::mutex liveChordMutex;
    mutable juce::String latchedLiveChordName = "Chord: --";
    mutable std::vector<int> latchedLiveChordNotes;
    mutable juce::String pendingLiveChordName;
    mutable std::vector<int> pendingLiveChordNotes;
    mutable double pendingLiveChordStartMs = 0.0;

    mutable std::mutex loadedMutex;
    juce::MidiMessageSequence processedLoadedSequence;
    juce::MidiFile loadedSourceMidiFile;
    short loadedTicksPerQuarter = 960;
    double loadedLengthPpq = 0.0;
    bool loadedEventsPreferUpperFrets = false;
    int loadedEventsPreferredMinFret = 5;
    juce::String loadedSummary = "Drop a .mid file or use Load MIDI.";

    mutable std::mutex generatedMutex;
    GeneratedMelodySnapshot generatedMelody;
    std::vector<GeneratedMidiEvent> generatedEvents;
    double generatedLengthPpq = 16.0;

    double currentSampleRate = 44100.0;
    std::atomic<bool> hostWasPlaying { false };
    std::atomic<double> melodyStartHostPpq { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarChannelizerAudioProcessor)
};
