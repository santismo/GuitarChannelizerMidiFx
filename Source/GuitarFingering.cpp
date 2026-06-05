#include "GuitarFingering.h"

#include <cmath>
#include <functional>
#include <limits>

int GuitarFingering::channelForString (int stringIndex, bool inverted) noexcept
{
    if (stringIndex < 0 || stringIndex >= numStrings)
        return 1;

    return inverted ? numStrings - stringIndex : stringIndex + 1;
}

int GuitarFingering::stringForChannel (int channel, bool inverted) noexcept
{
    if (channel < 1 || channel > numStrings)
        return -1;

    return inverted ? numStrings - channel : channel - 1;
}

juce::String GuitarFingering::stringName (int stringIndex)
{
    static const std::array<const char*, numStrings> names { "E", "A", "D", "G", "B", "E" };

    if (stringIndex < 0 || stringIndex >= numStrings)
        return {};

    return names[static_cast<size_t> (stringIndex)];
}

std::vector<GuitarPosition> GuitarFingering::getCandidates (int midiNote) const
{
    std::vector<GuitarPosition> positions;

    for (int string = 0; string < numStrings; ++string)
    {
        const auto fret = midiNote - standardTuning[static_cast<size_t> (string)];
        if (fret >= 0 && fret <= maxFret)
            positions.push_back ({ string, fret });
    }

    return positions;
}

GuitarPosition GuitarFingering::choosePosition (int midiNote,
                                                const GuitarPosition& previous,
                                                GuitarFingeringOptions options) const
{
    const auto candidates = getCandidates (midiNote);
    GuitarPosition best;
    auto bestScore = std::numeric_limits<double>::max();

    for (const auto candidate : candidates)
    {
        const auto score = scorePosition (candidate, previous, options);
        if (score < bestScore)
        {
            bestScore = score;
            best = candidate;
        }
    }

    return best.isPlayable() ? best : nearestPlayablePosition (midiNote);
}

std::vector<GuitarPosition> GuitarFingering::chooseChordPositions (const std::vector<int>& midiNotes,
                                                                   const GuitarPosition& previous,
                                                                   GuitarFingeringOptions options,
                                                                   const std::vector<int>* preferredStrings) const
{
    std::vector<std::vector<GuitarPosition>> candidateLists;
    candidateLists.reserve (midiNotes.size());

    for (const auto note : midiNotes)
    {
        auto candidates = getCandidates (note);
        if (candidates.empty())
            candidates.push_back (nearestPlayablePosition (note));

        candidateLists.push_back (std::move (candidates));
    }

    if (candidateLists.empty())
        return {};

    if (candidateLists.size() > static_cast<size_t> (numStrings))
    {
        std::vector<GuitarPosition> individual;
        individual.reserve (midiNotes.size());

        for (const auto note : midiNotes)
            individual.push_back (choosePosition (note, previous, options));

        return individual;
    }

    std::vector<GuitarPosition> current (candidateLists.size());
    std::vector<GuitarPosition> best (candidateLists.size());
    auto bestScore = std::numeric_limits<double>::max();

    std::function<void (size_t)> search = [&] (size_t index)
    {
        if (index == candidateLists.size())
        {
            const auto score = scoreChord (current, previous, options, preferredStrings);
            if (score < bestScore)
            {
                bestScore = score;
                best = current;
            }

            return;
        }

        for (const auto candidate : candidateLists[index])
        {
            current[index] = candidate;
            search (index + 1);
        }
    };

    search (0);
    return best;
}

GuitarPosition GuitarFingering::nearestPlayablePosition (int midiNote) noexcept
{
    GuitarPosition best;
    auto bestDistance = std::numeric_limits<int>::max();

    for (int string = 0; string < numStrings; ++string)
    {
        const auto unclampedFret = midiNote - standardTuning[static_cast<size_t> (string)];
        const auto fret = juce::jlimit (0, maxFret, unclampedFret);
        const auto playablePitch = standardTuning[static_cast<size_t> (string)] + fret;
        const auto distance = std::abs (playablePitch - midiNote);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = { string, fret };
        }
    }

    return best;
}

double GuitarFingering::scorePosition (GuitarPosition position,
                                       const GuitarPosition& previous,
                                       GuitarFingeringOptions options) noexcept
{
    if (! position.isPlayable())
        return 10000.0;

    auto score = static_cast<double> (position.fret) * 0.65;

    if (position.fret == 0)
        score += options.preferUpperFrets ? 180.0 : -1.5;

    if (options.preferUpperFrets)
    {
        const auto preferredMinFret = juce::jlimit (1, maxFret, options.preferredMinFret);

        if (position.fret > 0 && position.fret < preferredMinFret)
            score += static_cast<double> (preferredMinFret - position.fret) * 24.0;
        else if (position.fret >= preferredMinFret && position.fret <= preferredMinFret + 4)
            score -= 12.0;
    }

    if (position.fret > 12)
        score += static_cast<double> (position.fret - 12) * 3.0;

    if (options.handTargetEnabled)
    {
        const auto targetFret = juce::jlimit (0, maxFret, options.handTargetFret);
        const auto radius = juce::jlimit (1, 8, options.handTargetRadius);
        const auto distance = std::abs (position.fret - targetFret);
        score += static_cast<double> (distance) * 3.25;

        if (distance > radius)
            score += static_cast<double> (distance - radius) * 28.0;

        if (position.fret == 0 && targetFret > 4)
            score += 32.0;
    }

    if (previous.isPlayable())
    {
        score += static_cast<double> (std::abs (position.fret - previous.fret)) * 4.0;
        score += static_cast<double> (std::abs (position.stringIndex - previous.stringIndex)) * 6.0;
    }
    else
    {
        const auto targetFret = options.preferUpperFrets ? juce::jlimit (1, maxFret, options.preferredMinFret) : 5;
        score += std::abs (static_cast<double> (position.fret) - static_cast<double> (targetFret)) * 0.75;
    }

    return score;
}

double GuitarFingering::scoreChord (const std::vector<GuitarPosition>& positions,
                                    const GuitarPosition& previous,
                                    GuitarFingeringOptions options,
                                    const std::vector<int>* preferredStrings) noexcept
{
    auto score = 0.0;
    std::array<int, numStrings> stringUse {};
    auto minFret = maxFret;
    auto maxUsedFret = 0;
    auto playableCount = 0;

    for (size_t i = 0; i < positions.size(); ++i)
    {
        const auto position = positions[i];
        score += scorePosition (position, previous, options);

        if (! position.isPlayable())
            continue;

        ++stringUse[static_cast<size_t> (position.stringIndex)];
        minFret = juce::jmin (minFret, position.fret);
        maxUsedFret = juce::jmax (maxUsedFret, position.fret);
        ++playableCount;

        if (preferredStrings != nullptr && i < preferredStrings->size())
        {
            const auto preferredString = (*preferredStrings)[i];
            if (preferredString >= 0 && preferredString < numStrings && position.stringIndex != preferredString)
                score += static_cast<double> (std::abs (position.stringIndex - preferredString)) * 45.0 + 18.0;
        }
    }

    for (const auto count : stringUse)
        if (count > 1)
            score += static_cast<double> (count - 1) * 5000.0;

    if (playableCount > 1)
    {
        const auto span = maxUsedFret - minFret;
        score += static_cast<double> (span) * 5.0;

        if (span > 5)
            score += static_cast<double> (span - 5) * 40.0;

        const auto preferredMinFret = juce::jlimit (1, maxFret, options.preferredMinFret);
        if (options.preferUpperFrets && minFret < preferredMinFret && maxUsedFret >= preferredMinFret)
            score += static_cast<double> (preferredMinFret - minFret) * 18.0;
    }

    return score;
}
