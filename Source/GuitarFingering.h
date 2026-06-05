#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

struct GuitarPosition
{
    int stringIndex = -1; // 0 = low E, 5 = high E
    int fret = -1;

    bool isPlayable() const noexcept { return stringIndex >= 0 && fret >= 0; }
};

struct GuitarFingeringOptions
{
    bool preferUpperFrets = false;
    int preferredMinFret = 5;
    bool handTargetEnabled = false;
    int handTargetFret = 5;
    int handTargetRadius = 4;
};

class GuitarFingering
{
public:
    static constexpr int numStrings = 6;
    static constexpr int maxFret = 24;
    static constexpr std::array<int, numStrings> standardTuning { 40, 45, 50, 55, 59, 64 };

    static int channelForString (int stringIndex, bool inverted) noexcept;
    static int stringForChannel (int channel, bool inverted) noexcept;
    static juce::String stringName (int stringIndex);

    std::vector<GuitarPosition> getCandidates (int midiNote) const;
    GuitarPosition choosePosition (int midiNote,
                                   const GuitarPosition& previous,
                                   GuitarFingeringOptions options) const;
    std::vector<GuitarPosition> chooseChordPositions (const std::vector<int>& midiNotes,
                                                      const GuitarPosition& previous,
                                                      GuitarFingeringOptions options,
                                                      const std::vector<int>* preferredStrings = nullptr) const;

private:
    static GuitarPosition nearestPlayablePosition (int midiNote) noexcept;
    static double scorePosition (GuitarPosition position,
                                 const GuitarPosition& previous,
                                 GuitarFingeringOptions options) noexcept;
    static double scoreChord (const std::vector<GuitarPosition>& positions,
                              const GuitarPosition& previous,
                              GuitarFingeringOptions options,
                              const std::vector<int>* preferredStrings = nullptr) noexcept;
};
