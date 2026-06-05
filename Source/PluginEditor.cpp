#include "PluginEditor.h"

#include <limits>

namespace
{
    juce::Colour backgroundColour() { return juce::Colour (0xff171a1d); }
    juce::Colour panelColour()      { return juce::Colour (0xff22272b); }
    juce::Colour lineColour()       { return juce::Colour (0xff6e767e); }
    juce::Colour accentColour()     { return juce::Colour (0xff2fbf71); }

    struct NeckTheme
    {
        juce::Colour board;
        juce::Colour line;
        juce::Colour text;
        juce::Colour note;
        juce::Colour root;
        int markerStyle = 0;
        int noteShape = 0;
    };

    NeckTheme neckTheme (int index)
    {
        switch (index)
        {
            case 1: return { juce::Colour (0xffd0a15d), juce::Colour (0xff5f3d1f), juce::Colour (0xff24160e), juce::Colour (0xff0a6f66), juce::Colour (0xffd8293f), 1, 1 };
            case 2: return { juce::Colour (0xff3a2118), juce::Colour (0xffb58b66), juce::Colour (0xfff1d5b5), juce::Colour (0xfff0b449), juce::Colour (0xff4fb6ff), 2, 2 };
            case 3: return { juce::Colour (0xff10131a), juce::Colour (0xff4ad6ff), juce::Colour (0xffe8fbff), juce::Colour (0xffa6ff4d), juce::Colour (0xffff4fd8), 3, 3 };
            case 4: return { juce::Colour (0xff060708), juce::Colour (0xff2f3440), juce::Colour (0xfff6fbff), juce::Colour (0xff36f1ad), juce::Colour (0xffffe066), 3, 3 };
            default: return { panelColour(), lineColour(), juce::Colours::white.withAlpha (0.74f), accentColour(), juce::Colour (0xffffd166), 0, 0 };
        }
    }

    float randomUnit (int seed, int string, int fret, int salt)
    {
        const auto value = std::sin (static_cast<float> (seed * 37 + string * 113 + fret * 271 + salt * 19) * 12.9898f) * 43758.5453f;
        return value - std::floor (value);
    }

    juce::Colour randomPadColour (int seed, int string, int fret, int octaveOffset)
    {
        auto hue = randomUnit (seed, string, fret, 1);
        const auto saturation = 0.62f + randomUnit (seed, string, fret, 2) * 0.32f;
        const auto brightness = 0.72f + randomUnit (seed, string, fret, 3) * 0.24f;

        if (octaveOffset < 0)
            hue = std::fmod (hue + 0.53f, 1.0f);
        else if (octaveOffset > 0)
            hue = std::fmod (hue + 0.14f, 1.0f);

        return juce::Colour::fromHSV (hue, saturation, brightness, 1.0f);
    }

    juce::Colour hueShifted (juce::Colour colour, float amount)
    {
        return juce::Colour::fromHSV (std::fmod (colour.getHue() + amount + 1.0f, 1.0f),
                                      colour.getSaturation(),
                                      colour.getBrightness(),
                                      colour.getFloatAlpha());
    }

    juce::Colour randomBackgroundColour (int seed, int index, float shift)
    {
        auto hue = std::fmod (randomUnit (seed, index, 0, 11) + 0.38f + shift, 1.0f);
        const auto saturation = 0.34f + randomUnit (seed, index, 0, 12) * 0.22f;
        const auto brightness = 0.055f + randomUnit (seed, index, 0, 13) * 0.075f;
        return juce::Colour::fromHSV (hue, saturation, brightness, 1.0f);
    }

    juce::Path makeShapePath (juce::Rectangle<float> bounds, int shape)
    {
        juce::Path path;
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto rx = bounds.getWidth() * 0.5f;
        const auto ry = bounds.getHeight() * 0.5f;

        auto addPoint = [&] (float xNorm, float yNorm)
        {
            if (path.isEmpty())
                path.startNewSubPath (bounds.getX() + bounds.getWidth() * xNorm, bounds.getY() + bounds.getHeight() * yNorm);
            else
                path.lineTo (bounds.getX() + bounds.getWidth() * xNorm, bounds.getY() + bounds.getHeight() * yNorm);
        };

        const auto addPolygon = [&] (std::initializer_list<std::pair<float, float>> points)
        {
            for (const auto& point : points)
                addPoint (point.first, point.second);
        };

        switch (shape)
        {
            case 3:
                addPolygon ({ { 0.20f, 0.0f }, { 0.80f, 0.0f }, { 1.0f, 0.50f }, { 0.80f, 1.0f }, { 0.20f, 1.0f }, { 0.0f, 0.50f } });
                break;
            case 4:
                addPolygon ({ { 0.50f, 0.02f }, { 0.98f, 0.50f }, { 0.50f, 0.98f }, { 0.02f, 0.50f } });
                break;
            case 5:
                addPolygon ({ { 0.24f, 0.0f }, { 0.76f, 0.0f }, { 1.0f, 0.24f }, { 1.0f, 0.76f }, { 0.76f, 1.0f }, { 0.24f, 1.0f }, { 0.0f, 0.76f }, { 0.0f, 0.24f } });
                break;
            case 7:
                addPolygon ({ { 0.50f, 0.04f }, { 0.96f, 0.96f }, { 0.04f, 0.96f } });
                break;
            case 8:
                addPolygon ({ { 0.50f, 0.04f }, { 0.61f, 0.38f }, { 0.96f, 0.38f }, { 0.68f, 0.58f }, { 0.78f, 0.92f }, { 0.50f, 0.72f }, { 0.22f, 0.92f }, { 0.32f, 0.58f }, { 0.04f, 0.38f }, { 0.39f, 0.38f } });
                break;
            case 9:
                addPolygon ({ { 0.50f, 0.02f }, { 0.98f, 0.38f }, { 0.80f, 0.98f }, { 0.20f, 0.98f }, { 0.02f, 0.38f } });
                break;
            case 10:
                addPolygon ({ { 0.50f, 0.0f }, { 0.62f, 0.32f }, { 1.0f, 0.50f }, { 0.62f, 0.68f }, { 0.50f, 1.0f }, { 0.38f, 0.68f }, { 0.0f, 0.50f }, { 0.38f, 0.32f } });
                break;
            default:
                path.addEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
                return path;
        }

        path.closeSubPath();
        return path;
    }

    bool fileLooksLikeMidi (const juce::String& path)
    {
        return juce::File (path).hasFileExtension ("mid;midi");
    }

    float fretX (juce::Rectangle<float> board, int fret)
    {
        if (fret <= 0)
            return board.getX();

        const auto scalePosition = 1.0f - std::pow (2.0f, -static_cast<float> (fret) / 12.0f);
        const auto maxPosition = 1.0f - std::pow (2.0f, -static_cast<float> (GuitarFingering::maxFret) / 12.0f);
        return board.getX() + board.getWidth() * scalePosition / maxPosition;
    }

    float fretCenterX (juce::Rectangle<float> board, float fretPosition)
    {
        if (fretPosition <= 0.0f)
            return board.getX();

        const auto clamped = juce::jlimit (0.0f, static_cast<float> (GuitarFingering::maxFret), fretPosition);
        const auto lower = juce::jlimit (0, GuitarFingering::maxFret, static_cast<int> (std::floor (clamped)));
        const auto upper = juce::jlimit (0, GuitarFingering::maxFret, lower + 1);
        const auto amount = clamped - static_cast<float> (lower);
        const auto lowerX = lower == 0 ? board.getX() : (fretX (board, lower - 1) + fretX (board, lower)) * 0.5f;
        const auto upperX = upper == 0 ? board.getX() : (fretX (board, upper - 1) + fretX (board, upper)) * 0.5f;
        return lowerX + (upperX - lowerX) * amount;
    }

    juce::Rectangle<float> fretboardStringArea (juce::Rectangle<float> localBounds)
    {
        constexpr auto labelWidth = 54.0f;
        return localBounds.reduced (18.0f, 14.0f).withTrimmedLeft (labelWidth).reduced (0.0f, 10.0f);
    }
}

void FretboardView::setSnapshot (const FretboardSnapshot& newSnapshot)
{
    snapshot = newSnapshot;
    repaint();
}

void FretboardView::setHandTargetCallbacks (std::function<void (int)> changed, std::function<void()> cleared)
{
    handTargetChanged = std::move (changed);
    handTargetCleared = std::move (cleared);
}

void FretboardView::setHandTargetRadius (int radius)
{
    const auto clamped = juce::jlimit (1, 8, radius);
    if (hoverRadius == clamped)
        return;

    hoverRadius = clamped;
    repaint();
}

void FretboardView::setTheme (int theme)
{
    const auto clamped = juce::jlimit (0, 4, theme);
    if (themeIndex == clamped)
        return;

    themeIndex = clamped;
    repaint();
}

void FretboardView::setShape (int shape)
{
    const auto clamped = juce::jlimit (0, GuitarChannelizerAudioProcessor::getFretboardShapeNames().size() - 1, shape);
    if (shapeIndex == clamped)
        return;

    shapeIndex = clamped;
    repaint();
}

void FretboardView::setColorSeed (int seed)
{
    const auto clamped = juce::jlimit (0, 4095, seed);
    if (colorSeed == clamped)
        return;

    colorSeed = clamped;
    repaint();
}

void FretboardView::setColorMode (int mode)
{
    const auto clamped = juce::jlimit (0, GuitarChannelizerAudioProcessor::getFretboardColorModeNames().size() - 1, mode);
    if (colorMode == clamped)
        return;

    colorMode = clamped;
    repaint();
}

juce::Rectangle<float> FretboardView::getNeckBounds() const
{
    return getLocalBounds().toFloat();
}

void FretboardView::paint (juce::Graphics& g)
{
    const auto theme = neckTheme (themeIndex);
    const auto neck = getNeckBounds();
    auto bounds = neck.reduced (18.0f, 14.0f);
    g.setColour (theme.board);
    g.fillRoundedRectangle (neck.reduced (1.0f), 8.0f);

    const auto labelWidth = 54.0f;
    auto board = fretboardStringArea (neck);
    const auto stringGap = board.getHeight() / static_cast<float> (GuitarFingering::numStrings - 1);

    auto fillShape = [&] (juce::Rectangle<float> shapeBounds,
                          int shape,
                          juce::Colour fill,
                          juce::Colour fill2,
                          bool useGradient,
                          juce::Colour stroke,
                          float strokeWidth)
    {
        const auto roundedRadius = juce::jmin (16.0f, shapeBounds.getHeight() * 0.32f);
        if (useGradient)
            g.setGradientFill (juce::ColourGradient (fill, shapeBounds.getX(), shapeBounds.getY(),
                                                     fill2, shapeBounds.getRight(), shapeBounds.getBottom(), false));
        else
            g.setColour (fill);

        if (shape == 0)
            g.fillRoundedRectangle (shapeBounds, roundedRadius);
        else if (shape == 1)
            g.fillRect (shapeBounds);
        else if (shape == 6)
            g.fillRoundedRectangle (shapeBounds, shapeBounds.getHeight() * 0.5f);
        else if (shape == 2)
            g.fillEllipse (shapeBounds);
        else
            g.fillPath (makeShapePath (shapeBounds, shape));

        g.setColour (stroke);
        if (shape == 0)
            g.drawRoundedRectangle (shapeBounds, roundedRadius, strokeWidth);
        else if (shape == 1)
            g.drawRect (shapeBounds, strokeWidth);
        else if (shape == 6)
            g.drawRoundedRectangle (shapeBounds, shapeBounds.getHeight() * 0.5f, strokeWidth);
        else if (shape == 2)
            g.drawEllipse (shapeBounds, strokeWidth);
        else
            g.strokePath (makeShapePath (shapeBounds, shape), juce::PathStrokeType (strokeWidth));
    };

    if (hoverFretPosition >= 0.0f)
    {
        const auto leftFret = hoverFretPosition - static_cast<float> (hoverRadius);
        const auto rightFret = hoverFretPosition + static_cast<float> (hoverRadius);
        const auto left = leftFret <= 0.0f ? board.getX() : fretCenterX (board, leftFret);
        const auto right = rightFret >= static_cast<float> (GuitarFingering::maxFret) ? board.getRight() : fretCenterX (board, rightFret);

        g.setColour (accentColour().withAlpha (0.10f));
        g.fillRoundedRectangle (juce::Rectangle<float> (left, board.getY(), right - left, board.getHeight()), 4.0f);
    }

    if (themeIndex == 4)
    {
        const auto shift = colorMode == 2
            ? static_cast<float> (std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.000055, 1.0))
            : 0.0f;
        const auto useGradients = colorMode != 0;
        const auto bgA = randomBackgroundColour (colorSeed, 0, shift);
        const auto bgB = randomBackgroundColour (colorSeed, 1, shift + 0.08f);
        const auto bgC = randomBackgroundColour (colorSeed, 2, shift + 0.16f);

        if (useGradients)
        {
            g.setGradientFill (juce::ColourGradient (bgA, neck.getX(), neck.getY(),
                                                     bgB, neck.getRight(), neck.getBottom(), false));
            g.fillRoundedRectangle (neck.reduced (1.0f), 8.0f);
        }
        else
        {
            g.setColour (bgA);
            g.fillRoundedRectangle (neck.reduced (1.0f), 8.0f);
        }

        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));

        constexpr int visibleFrets = 13;
        auto padBoard = neck.reduced (14.0f, 18.0f).withTrimmedTop (8.0f).withTrimmedBottom (8.0f);
        if (useGradients)
        {
            g.setGradientFill (juce::ColourGradient (bgB.darker (0.55f), padBoard.getX(), padBoard.getY(),
                                                     bgC.darker (0.65f), padBoard.getRight(), padBoard.getBottom(), false));
        }
        else
        {
            g.setColour (bgB.darker (0.55f));
        }
        g.fillRoundedRectangle (padBoard.expanded (10.0f, 12.0f), 7.0f);

        const auto cellWidth = padBoard.getWidth() / static_cast<float> (visibleFrets);
        const auto cellHeight = padBoard.getHeight() / static_cast<float> (GuitarFingering::numStrings);
        const auto padGap = juce::jlimit (6.0f, 14.0f, juce::jmin (cellWidth, cellHeight) * 0.18f);
        const auto padSize = juce::jmax (6.0f, juce::jmin (cellWidth, cellHeight) - padGap);

        auto drawMarkerDot = [&] (float x, float y, bool large)
        {
            const auto markerSize = large ? 8.0f : 6.0f;
            auto marker = juce::Rectangle<float> (x - markerSize * 0.5f, y - markerSize * 0.5f, markerSize, markerSize);
            g.setColour (juce::Colours::white.withAlpha (0.68f));
            g.fillEllipse (marker);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.drawEllipse (marker.expanded (3.0f), 1.0f);
        };

        auto cellStateFor = [&] (int string, int visibleFret, bool& active, bool& root, int& octaveOffset, int& activeFret)
        {
            active = false;
            root = false;
            octaveOffset = 0;
            activeFret = visibleFret;

            for (int fret = visibleFret; fret <= GuitarFingering::maxFret; fret += 12)
            {
                if (visibleFret == 0 && fret != 0)
                    continue;

                const auto index = string * FretboardSnapshot::frets + fret;
                if (! snapshot.active[static_cast<size_t> (index)])
                    continue;

                active = true;
                root = root || snapshot.chordRoot[static_cast<size_t> (index)];
                octaveOffset = snapshot.octaveOffset[static_cast<size_t> (index)];
                activeFret = fret;
            }
        };

        for (const auto fret : { 3, 5, 7, 9, 12 })
        {
            const auto x = padBoard.getX() + (static_cast<float> (fret) + 0.5f) * cellWidth;
            drawMarkerDot (x, padBoard.getY() - 10.0f, fret == 12);
            drawMarkerDot (x, padBoard.getBottom() + 10.0f, fret == 12);
        }

        for (int string = 0; string < GuitarFingering::numStrings; ++string)
        {
            const auto displayRow = GuitarFingering::numStrings - 1 - string;
            const auto y = padBoard.getY() + static_cast<float> (displayRow) * cellHeight;
            auto rowColour = randomPadColour (colorSeed, string, 0, 0);
            if (colorMode == 2)
                rowColour = hueShifted (rowColour, shift + static_cast<float> (string) * 0.035f);

            for (int fret = 0; fret < visibleFrets; ++fret)
            {
                const auto x = padBoard.getX() + static_cast<float> (fret) * cellWidth;
                auto cell = juce::Rectangle<float> (x + cellWidth * 0.5f - padSize * 0.5f,
                                                    y + cellHeight * 0.5f - padSize * 0.5f,
                                                    padSize,
                                                    padSize);
                bool isActive = false;
                bool isRoot = false;
                int octaveOffset = 0;
                int activeFret = fret;
                cellStateFor (string, fret, isActive, isRoot, octaveOffset, activeFret);

                auto padColour = isActive ? randomPadColour (colorSeed, string, activeFret, octaveOffset) : rowColour;
                if (colorMode == 2)
                    padColour = hueShifted (padColour, shift + static_cast<float> (fret) * 0.012f);

                auto fill = isActive
                    ? juce::Colour::fromHSV (padColour.getHue(), 0.30f, 0.18f, 1.0f)
                    : juce::Colour::fromHSV (padColour.getHue(), 0.18f, 0.035f, 1.0f);
                auto fill2 = isActive
                    ? juce::Colour::fromHSV (std::fmod (padColour.getHue() + 0.10f, 1.0f), 0.42f, 0.25f, 1.0f)
                    : juce::Colour::fromHSV (std::fmod (padColour.getHue() + 0.08f, 1.0f), 0.22f, 0.055f, 1.0f);
                auto stroke = padColour.withAlpha (isActive ? 0.96f : 0.75f);
                if (isRoot)
                    stroke = juce::Colour (0xfffff1a0);

                if (isActive)
                {
                    g.setColour (stroke.withAlpha (0.28f));
                    fillShape (cell.expanded (3.0f), shapeIndex,
                               juce::Colours::transparentBlack, juce::Colours::transparentBlack, false,
                               stroke.withAlpha (0.22f), 3.0f);
                }

                fillShape (cell, shapeIndex, fill, fill2, useGradients, stroke, isActive ? 2.2f : 1.7f);

                if (isActive && octaveOffset != 0)
                {
                    g.setColour (octaveOffset < 0 ? juce::Colour (0xff61e6ff) : juce::Colour (0xffff79d7));
                    g.drawText (octaveOffset < 0 ? "-8" : "+8", cell.toNearestInt(), juce::Justification::centred);
                }
            }
        }

        return;
    }

    g.setFont (juce::FontOptions (12.0f));

    for (int fret = 0; fret <= GuitarFingering::maxFret; ++fret)
    {
        const auto x = fretX (board, fret);
        g.setColour (fret == 0 ? theme.text.withAlpha (0.78f) : theme.line.withAlpha (0.65f));
        g.drawLine (x, board.getY(), x, board.getBottom(), fret == 0 ? 3.0f : 1.0f);

        if (fret > 0 && (fret == 3 || fret == 5 || fret == 7 || fret == 9 || fret == 12 || fret == 15 || fret == 17 || fret == 19 || fret == 21 || fret == 24))
        {
            const auto labelX = (fretX (board, fret - 1) + fretX (board, fret)) * 0.5f;
            g.setColour (theme.text.withAlpha (0.50f));
            g.drawText (juce::String (fret), juce::Rectangle<float> (labelX - 12.0f, board.getBottom() + 2.0f, 24.0f, 16.0f),
                        juce::Justification::centred);
        }
    }

    auto drawMarker = [&] (int fret, float yNorm, float radius)
    {
        const auto x = (fretX (board, fret - 1) + fretX (board, fret)) * 0.5f;
        const auto y = board.getY() + board.getHeight() * yNorm;
        auto marker = juce::Rectangle<float> (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (theme.text.withAlpha (0.22f));

        if (theme.markerStyle == 1)
            g.fillRoundedRectangle (marker, 2.0f);
        else if (theme.markerStyle == 2)
            g.drawEllipse (marker, 1.6f);
        else if (theme.markerStyle == 3)
            g.drawLine (marker.getX(), marker.getY(), marker.getRight(), marker.getBottom(), 1.4f),
            g.drawLine (marker.getRight(), marker.getY(), marker.getX(), marker.getBottom(), 1.4f);
        else
            g.fillEllipse (marker);
    };

    for (const auto fret : { 3, 5, 7, 9, 15, 17, 19, 21 })
        drawMarker (fret, 0.5f, 4.4f);
    for (const auto fret : { 12, 24 })
    {
        drawMarker (fret, 0.38f, 4.0f);
        drawMarker (fret, 0.62f, 4.0f);
    }

    for (int string = 0; string < GuitarFingering::numStrings; ++string)
    {
        const auto displayRow = GuitarFingering::numStrings - 1 - string;
        const auto y = board.getY() + static_cast<float> (displayRow) * stringGap;
        const auto thickness = 1.6f + static_cast<float> (GuitarFingering::numStrings - 1 - displayRow) * 0.24f;

        g.setColour (theme.text.withAlpha (0.78f));
        g.drawText (GuitarFingering::stringName (string),
                    juce::Rectangle<float> (bounds.getX(), y - 9.0f, labelWidth - 8.0f, 18.0f),
                    juce::Justification::centredRight);

        g.setColour (theme.line);
        g.drawLine (board.getX(), y, board.getRight(), y, thickness);
    }

    for (int string = 0; string < GuitarFingering::numStrings; ++string)
    {
        const auto displayRow = GuitarFingering::numStrings - 1 - string;
        const auto y = board.getY() + static_cast<float> (displayRow) * stringGap;

        for (int fret = 0; fret <= GuitarFingering::maxFret; ++fret)
        {
            const auto index = string * FretboardSnapshot::frets + fret;
            if (! snapshot.active[static_cast<size_t> (index)])
                continue;

            const auto x = fret == 0 ? board.getX() - 10.0f
                                     : (fretX (board, fret - 1) + fretX (board, fret)) * 0.5f;
            const auto isRoot = snapshot.chordRoot[static_cast<size_t> (index)];
            const auto octaveOffset = snapshot.octaveOffset[static_cast<size_t> (index)];
            const auto size = isRoot ? 23.0f : 17.0f;
            auto dot = juce::Rectangle<float> (x - size * 0.5f, y - size * 0.5f, size, size);

            auto noteColour = isRoot ? theme.root : theme.note;
            if (octaveOffset < 0)
                noteColour = juce::Colour (0xff61e6ff);
            else if (octaveOffset > 0)
                noteColour = juce::Colour (0xffff79d7);

            g.setColour (noteColour);
            if (isRoot || theme.noteShape == 2)
            {
                juce::Path diamond;
                diamond.startNewSubPath (dot.getCentreX(), dot.getY());
                diamond.lineTo (dot.getRight(), dot.getCentreY());
                diamond.lineTo (dot.getCentreX(), dot.getBottom());
                diamond.lineTo (dot.getX(), dot.getCentreY());
                diamond.closeSubPath();
                g.fillPath (diamond);
            }
            else if (theme.noteShape == 1)
            {
                g.fillRoundedRectangle (dot, 3.0f);
            }
            else if (theme.noteShape == 3)
            {
                g.fillEllipse (dot);
                g.setColour (theme.root.withAlpha (0.55f));
                g.drawEllipse (dot.reduced (4.0f), 1.4f);
            }
            else
            {
                g.fillEllipse (dot);
            }
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawEllipse (dot, 1.4f);

            if (octaveOffset != 0)
            {
                g.setColour (juce::Colours::black.withAlpha (0.62f));
                g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
                g.drawText (octaveOffset < 0 ? "-8" : "+8", dot.toNearestInt(), juce::Justification::centred);
                g.setFont (juce::FontOptions (12.0f));
            }
        }
    }
}

void FretboardView::mouseMove (const juce::MouseEvent& event)
{
    const auto fret = fretForPoint (event.position);
    const auto position = fretPositionForPoint (event.position);
    if (fret == hoverTargetFret && std::abs (position - hoverFretPosition) < 0.01f)
        return;

    hoverTargetFret = fret;
    hoverFretPosition = position;
    if (handTargetChanged != nullptr)
        handTargetChanged (hoverTargetFret);

    repaint();
}

void FretboardView::mouseExit (const juce::MouseEvent&)
{
    if (hoverTargetFret < 0)
        return;

    hoverTargetFret = -1;
    hoverFretPosition = -1.0f;
    if (handTargetCleared != nullptr)
        handTargetCleared();

    repaint();
}

int FretboardView::fretForPoint (juce::Point<float> point) const
{
    const auto board = fretboardStringArea (getNeckBounds());
    auto bestFret = 0;
    auto bestDistance = std::numeric_limits<float>::max();

    for (int fret = 0; fret <= GuitarFingering::maxFret; ++fret)
    {
        const auto center = fret == 0 ? board.getX()
                                      : (fretX (board, fret - 1) + fretX (board, fret)) * 0.5f;
        const auto distance = std::abs (center - point.x);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestFret = fret;
        }
    }

    return bestFret;
}

float FretboardView::fretPositionForPoint (juce::Point<float> point) const
{
    const auto board = fretboardStringArea (getNeckBounds());
    if (point.x <= board.getX())
        return 0.0f;
    if (point.x >= board.getRight())
        return static_cast<float> (GuitarFingering::maxFret);

    for (int fret = 0; fret < GuitarFingering::maxFret; ++fret)
    {
        const auto left = fretCenterX (board, static_cast<float> (fret));
        const auto right = fretCenterX (board, static_cast<float> (fret + 1));
        if (point.x >= left && point.x <= right)
        {
            const auto amount = (point.x - left) / juce::jmax (1.0f, right - left);
            return static_cast<float> (fret) + amount;
        }
    }

    return static_cast<float> (GuitarFingering::maxFret);
}

void PopupPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff1c2125));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (lineColour().withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
}

void PianoRollView::setSnapshot (const GeneratedMelodySnapshot& newSnapshot)
{
    snapshot = newSnapshot;
    repaint();
}

void PianoRollView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (panelColour());
    g.fillRoundedRectangle (bounds.reduced (1.0f), 8.0f);

    auto roll = bounds.reduced (14.0f, 12.0f);
    const auto labelWidth = 34.0f;
    auto grid = roll.withTrimmedLeft (labelWidth).withTrimmedBottom (18.0f);
    const auto totalPpq = static_cast<float> (juce::jmax (1, snapshot.bars)) * 4.0f;
    constexpr int minPitch = 40;
    constexpr int maxPitch = 84;

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (juce::Colours::white.withAlpha (0.68f));
    g.drawText ("C6", juce::Rectangle<float> (roll.getX(), grid.getY() - 5.0f, labelWidth - 5.0f, 14.0f),
                juce::Justification::centredRight);
    g.drawText ("E2", juce::Rectangle<float> (roll.getX(), grid.getBottom() - 9.0f, labelWidth - 5.0f, 14.0f),
                juce::Justification::centredRight);

    for (int bar = 0; bar <= snapshot.bars; ++bar)
    {
        const auto x = grid.getX() + grid.getWidth() * static_cast<float> (bar) / static_cast<float> (juce::jmax (1, snapshot.bars));
        g.setColour (bar == 0 ? juce::Colours::white.withAlpha (0.45f) : lineColour().withAlpha (0.35f));
        g.drawLine (x, grid.getY(), x, grid.getBottom(), bar % 4 == 0 ? 1.3f : 1.0f);
    }

    for (int row = 0; row <= 12; ++row)
    {
        const auto y = grid.getY() + grid.getHeight() * static_cast<float> (row) / 12.0f;
        g.setColour (lineColour().withAlpha (0.18f));
        g.drawLine (grid.getX(), y, grid.getRight(), y);
    }

    for (const auto& note : snapshot.notes)
    {
        const auto x = grid.getX() + grid.getWidth() * static_cast<float> (note.startPpq) / totalPpq;
        const auto w = juce::jmax (3.0f, grid.getWidth() * static_cast<float> (note.lengthPpq) / totalPpq);
        const auto pitch = juce::jlimit (minPitch, maxPitch, note.note);
        const auto yNorm = 1.0f - static_cast<float> (pitch - minPitch) / static_cast<float> (maxPitch - minPitch);
        const auto y = grid.getY() + yNorm * grid.getHeight();
        auto rect = juce::Rectangle<float> (x, y - 4.0f, w, 8.0f).reduced (0.5f);

        g.setColour (accentColour().withAlpha (0.86f));
        g.fillRoundedRectangle (rect, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.drawRoundedRectangle (rect, 2.0f, 1.0f);
    }

    auto scaleBand = roll.withTrimmedLeft (labelWidth).withTrimmedTop (grid.getHeight() + 4.0f);
    g.setFont (juce::FontOptions (10.5f));
    for (const auto& section : snapshot.scaleSections)
    {
        const auto x = scaleBand.getX() + scaleBand.getWidth() * static_cast<float> (section.bar) / static_cast<float> (juce::jmax (1, snapshot.bars));
        const auto w = scaleBand.getWidth() / static_cast<float> (juce::jmax (1, snapshot.bars));
        auto labelBounds = juce::Rectangle<float> (x + 2.0f, scaleBand.getY(), w - 4.0f, scaleBand.getHeight());
        g.setColour (accentColour().withAlpha (0.18f));
        g.fillRoundedRectangle (labelBounds, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.drawFittedText (section.name, labelBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

GuitarChannelizerAudioProcessorEditor::GuitarChannelizerAudioProcessorEditor (GuitarChannelizerAudioProcessor& owner)
    : AudioProcessorEditor (&owner),
      processorRef (owner)
{
    titleLabel.setText ("Guitarizer", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    statusLabel.setText (processorRef.getLoadedMidiSummary(), juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.72f));
    addAndMakeVisible (statusLabel);

    chordLabel.setText ("Chord: --", juce::dontSendNotification);
    chordLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    chordLabel.setJustificationType (juce::Justification::centredRight);
    chordLabel.setColour (juce::Label::textColourId, accentColour().withAlpha (0.96f));
    addAndMakeVisible (chordLabel);

    loadButton.onClick = [this] { chooseMidiFile(); };
    exportButton.onClick = [this] { chooseExportFile(); };
    melodyButton.onClick = [this] { setMelodyExpanded (! melodyExpanded); };
    cleanViewButton.onClick = [this]
    {
        cleanView = ! cleanView;
        if (cleanView)
        {
            scalePanelOpen = false;
            rootPanelOpen = false;
        }
        updateMelodyVisibility();
        updateCleanViewVisibility();
        resized();
    };
    generateButton.onClick = [this]
    {
        statusLabel.setText (processorRef.generateMelody(), juce::dontSendNotification);
        pianoRoll.setSnapshot (processorRef.getGeneratedMelodySnapshot());
    };
    scaleButton.onClick = [this] { showScaleMenu(); };
    rootButton.onClick = [this] { showRootMenu(); };
    randomizeButton.onClick = [this] { randomizeGenerationSettings(); };
    resetButton.onClick = [this] { resetGenerationSettings(); };
    scaleAllButton.onClick = [this] { selectAllScales(); };
    scaleRandomButton.onClick = [this] { randomizeScales(); };
    rootAllButton.onClick = [this] { selectAllRoots(); };
    rootRandomButton.onClick = [this] { randomizeRoots(); };
    fretboardRandomButton.onClick = [this]
    {
        if (auto* parameter = processorRef.getValueTreeState().getParameter (ParameterIDs::fretboardColorSeed))
        {
            const auto nextSeed = juce::Random::getSystemRandom().nextInt (4096);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (nextSeed)));
        }
    };

    for (auto* button : { &loadButton, &exportButton, &melodyButton, &cleanViewButton, &generateButton, &scaleButton, &rootButton,
                          &randomizeButton, &resetButton, &scaleAllButton, &scaleRandomButton, &rootAllButton, &rootRandomButton,
                          &fretboardRandomButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d343a));
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible (*button);
    }

    for (auto* button : { &invertButton, &useInputChannelsButton, &showSameChannelNotesButton, &latchChordButton, &preferUpperButton, &liveButton,
                          &melodyOutputButton, &guitaristicButton, &pureLegatoButton, &forceFirstNoteButton })
    {
        button->setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.86f));
        addAndMakeVisible (*button);
    }

    makeLabel (preferredMinFretLabel, "Min fret");
    makeLabel (handSpanLabel, "Focus span");
    makeLabel (chordLatchDelayLabel, "Latch ms");
    makeLabel (complexityLabel, "Complexity");
    makeLabel (densityLabel, "Density");
    makeLabel (barsLabel, "Bars");
    makeLabel (rootKeyLabel, "Root");
    makeLabel (keyCountLabel, "Key changes");
    makeLabel (minNoteLabel, "Low note");
    makeLabel (maxNoteLabel, "High note");
    makeLabel (scaleLabel, "Scales");
    makeLabel (legatoPolyLabel, "Legato / poly");
    makeLabel (polyDensityLabel, "Poly voices");
    makeLabel (melodySpeedLabel, "Speed");

    configureHorizontalSlider (preferredMinFretSlider, 1, 12);
    configureHorizontalSlider (handSpanSlider, 1, 8);
    configureHorizontalSlider (chordLatchDelaySlider, 0, 1000);
    configureHorizontalSlider (complexitySlider, 0, 100);
    configureHorizontalSlider (densitySlider, 0, 100);
    configureHorizontalSlider (barsSlider, 4, 32);
    configureHorizontalSlider (keyCountSlider, 1, 12);
    configureHorizontalSlider (minNoteSlider, 36, 96);
    configureHorizontalSlider (maxNoteSlider, 36, 96);
    configureHorizontalSlider (legatoPolySlider, 0, 100);
    configureHorizontalSlider (polyDensitySlider, 2, 6);

    themeBox.addItemList (GuitarChannelizerAudioProcessor::getFretboardThemeNames(), 1);
    shapeBox.addItemList (GuitarChannelizerAudioProcessor::getFretboardShapeNames(), 1);
    colorModeBox.addItemList (GuitarChannelizerAudioProcessor::getFretboardColorModeNames(), 1);
    melodySpeedBox.addItemList (GuitarChannelizerAudioProcessor::getMelodySpeedNames(), 1);
    for (auto* box : { &themeBox, &shapeBox, &colorModeBox, &melodySpeedBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2d343a));
        box->setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box->setColour (juce::ComboBox::outlineColourId, lineColour().withAlpha (0.45f));
        addAndMakeVisible (*box);
    }

    addAndMakeVisible (scalePanel);
    addAndMakeVisible (rootPanel);
    rootPanel.addAndMakeVisible (rootAllButton);
    rootPanel.addAndMakeVisible (rootRandomButton);
    const auto rootNames = GuitarChannelizerAudioProcessor::getKeyNames();
    for (int i = 0; i < 12; ++i)
    {
        auto* toggle = rootToggles.add (new juce::ToggleButton (rootNames[i + 1]));
        toggle->setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.86f));
        toggle->onClick = [this, i]
        {
            auto mask = processorRef.getMelodyRootMask();
            mask ^= (1 << i);
            if (mask == 0)
                mask = (1 << i);

            processorRef.setMelodyRootMask (mask);
            syncRootToggles();
            refreshRootButtonText();
        };
        rootPanel.addAndMakeVisible (toggle);
    }

    const auto scaleNames = GuitarChannelizerAudioProcessor::getScaleNames();
    scalePanel.addAndMakeVisible (scaleAllButton);
    scalePanel.addAndMakeVisible (scaleRandomButton);
    for (int i = 0; i < scaleNames.size(); ++i)
    {
        auto* toggle = scaleToggles.add (new juce::ToggleButton (scaleNames[i]));
        toggle->setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.86f));
        toggle->onClick = [this, i]
        {
            auto mask = processorRef.getMelodyScaleMask();
            mask ^= (1 << i);
            if (mask == 0)
                mask = (1 << i);

            processorRef.setMelodyScaleMask (mask);
            syncScaleToggles();
            refreshScaleButtonText();
        };
        scalePanel.addAndMakeVisible (toggle);
    }

    addAndMakeVisible (fretboard);
    fretboard.setHandTargetCallbacks ([this] (int fret)
                                      {
                                          processorRef.setLiveHandTargetFret (fret);
                                      },
                                      [this]
                                      {
                                          processorRef.clearLiveHandTargetFret();
                                      });
    addAndMakeVisible (pianoRoll);

    auto& state = processorRef.getValueTreeState();
    invertAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::invertChannels, invertButton);
    useInputChannelsAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::useInputChannels, useInputChannelsButton);
    showSameChannelNotesAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::showSameChannelNotes, showSameChannelNotesButton);
    latchChordAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::latchChordName, latchChordButton);
    preferUpperAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::preferUpperFrets, preferUpperButton);
    liveAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::processLiveInput, liveButton);
    preferredMinFretAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::preferredMinFret, preferredMinFretSlider);
    handSpanAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::handTargetRadius, handSpanSlider);
    chordLatchDelayAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::chordLatchReleaseMs, chordLatchDelaySlider);
    melodyOutputAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::melodyOutputEnabled, melodyOutputButton);
    guitaristicAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::melodyGuitaristic, guitaristicButton);
    complexityAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyComplexity, complexitySlider);
    densityAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyDensity, densitySlider);
    barsAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyBars, barsSlider);
    keyCountAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyKeyCount, keyCountSlider);
    minNoteAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyMinNote, minNoteSlider);
    maxNoteAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyMaxNote, maxNoteSlider);
    pureLegatoAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::melodyPureLegato, pureLegatoButton);
    forceFirstNoteAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::melodyForceFirstNote, forceFirstNoteButton);
    legatoPolyAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyLegatoPoly, legatoPolySlider);
    polyDensityAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::melodyPolyDensity, polyDensitySlider);
    themeAttachment = std::make_unique<ComboBoxAttachment> (state, ParameterIDs::fretboardTheme, themeBox);
    shapeAttachment = std::make_unique<ComboBoxAttachment> (state, ParameterIDs::fretboardShape, shapeBox);
    colorModeAttachment = std::make_unique<ComboBoxAttachment> (state, ParameterIDs::fretboardColorMode, colorModeBox);
    melodySpeedAttachment = std::make_unique<ComboBoxAttachment> (state, ParameterIDs::melodySpeed, melodySpeedBox);
    handSpanSlider.onValueChange = [this] { processorRef.requestLiveVoicingRefresh(); };

    setResizable (true, true);
    setResizeLimits (760, 280, 1680, 1040);
    setSize (1240, 360);
    updateMelodyVisibility();
    updateCleanViewVisibility();
    refreshScaleButtonText();
    refreshRootButtonText();
    syncScaleToggles();
    syncRootToggles();
    pianoRoll.setSnapshot (processorRef.getGeneratedMelodySnapshot());
    startTimerHz (30);
}

void GuitarChannelizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour());
}

void GuitarChannelizerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (18);
    auto header = bounds.removeFromTop (36);
    titleLabel.setBounds (header.removeFromLeft (260));
    cleanViewButton.setBounds (header.removeFromRight (118).reduced (3));
    melodyButton.setBounds (header.removeFromRight (96).reduced (3));
    loadButton.setBounds (header.removeFromRight (104).reduced (3));
    exportButton.setBounds (header.removeFromRight (114).reduced (3));

    if (cleanView)
    {
        auto cleanBounds = getLocalBounds().reduced (18);
        auto cleanHeader = cleanBounds.removeFromTop (30);
        cleanViewButton.setBounds (cleanHeader.removeFromRight (118).reduced (3));
        chordLabel.setBounds (cleanHeader);
        fretboard.setBounds (cleanBounds.withTrimmedTop (4));
        pianoRoll.setBounds ({});
        return;
    }

    auto status = bounds.removeFromTop (24);
    statusLabel.setBounds (status.removeFromLeft (juce::roundToInt (static_cast<float> (status.getWidth()) * 0.62f)));
    chordLabel.setBounds (status);

    auto controls = bounds.removeFromTop (34);
    liveButton.setBounds (controls.removeFromLeft (170));
    invertButton.setBounds (controls.removeFromLeft (156));
    useInputChannelsButton.setBounds (controls.removeFromLeft (178));
    preferUpperButton.setBounds (controls.removeFromLeft (176));
    preferredMinFretLabel.setBounds (controls.removeFromLeft (58));
    preferredMinFretSlider.setBounds (controls.removeFromLeft (150).reduced (0, 4));

    auto viewControls = bounds.removeFromTop (34);
    handSpanLabel.setBounds (viewControls.removeFromLeft (74));
    handSpanSlider.setBounds (viewControls.removeFromLeft (106).reduced (0, 4));
    themeBox.setBounds (viewControls.removeFromLeft (118).reduced (0, 4));
    shapeBox.setBounds (viewControls.removeFromLeft (118).reduced (0, 4));
    colorModeBox.setBounds (viewControls.removeFromLeft (104).reduced (0, 4));
    fretboardRandomButton.setBounds (viewControls.removeFromLeft (108).reduced (2, 4));
    showSameChannelNotesButton.setBounds (viewControls.removeFromLeft (150));
    latchChordButton.setBounds (viewControls.removeFromLeft (116));
    chordLatchDelayLabel.setBounds (viewControls.removeFromLeft (62));
    chordLatchDelaySlider.setBounds (viewControls.removeFromLeft (120).reduced (0, 4));

    bounds.removeFromTop (8);

    if (! melodyExpanded)
    {
        fretboard.setBounds (bounds);
        pianoRoll.setBounds ({});
        return;
    }

    auto melodyArea = bounds.removeFromTop (450);
    auto pianoBounds = melodyArea.removeFromLeft (760);
    pianoBounds.removeFromRight (12);
    pianoRoll.setBounds (pianoBounds);

    auto settings = melodyArea;
    const auto rowH = 30;
    generateButton.setBounds (settings.removeFromTop (32).removeFromRight (116).reduced (2));
    randomizeButton.setBounds (settings.removeFromTop (30).removeFromLeft (116).reduced (2));
    resetButton.setBounds (settings.getX() + 120, randomizeButton.getY(), 92, 26);
    auto toggles = settings.removeFromTop (32);
    melodyOutputButton.setBounds (toggles.removeFromLeft (164));
    guitaristicButton.setBounds (toggles.removeFromLeft (128));
    forceFirstNoteButton.setBounds (toggles.removeFromLeft (168));

    auto placeRow = [&] (juce::Label& label, juce::Component& control)
    {
        auto row = settings.removeFromTop (rowH);
        label.setBounds (row.removeFromLeft (100));
        control.setBounds (row.reduced (2, 4));
    };

    placeRow (complexityLabel, complexitySlider);
    placeRow (densityLabel, densitySlider);
    placeRow (barsLabel, barsSlider);
    placeRow (melodySpeedLabel, melodySpeedBox);
    auto rootRow = settings.removeFromTop (rowH);
    rootKeyLabel.setBounds (rootRow.removeFromLeft (100));
    rootButton.setBounds (rootRow.reduced (2, 4));
    rootPanel.setBounds (rootButton.getX(), rootButton.getBottom() + 2, rootButton.getWidth(), 206);
    auto rootPanelBounds = rootPanel.getLocalBounds().reduced (8, 7);
    auto rootPanelTop = rootPanelBounds.removeFromTop (28);
    rootAllButton.setBounds (rootPanelTop.removeFromLeft (72).reduced (1, 3));
    rootRandomButton.setBounds (rootPanelTop.removeFromLeft (96).reduced (1, 3));
    for (int i = 0; i < rootToggles.size(); ++i)
    {
        const auto column = i / 6;
        const auto row = i % 6;
        rootToggles[i]->setBounds (rootPanelBounds.getX() + column * (rootPanelBounds.getWidth() / 2),
                                   rootPanelBounds.getY() + row * 27,
                                   rootPanelBounds.getWidth() / 2,
                                   24);
    }
    rootPanel.toFront (false);
    placeRow (keyCountLabel, keyCountSlider);
    placeRow (minNoteLabel, minNoteSlider);
    placeRow (maxNoteLabel, maxNoteSlider);
    auto scaleRow = settings.removeFromTop (rowH);
    scaleLabel.setBounds (scaleRow.removeFromLeft (100));
    scaleButton.setBounds (scaleRow.reduced (2, 4));
    scalePanel.setBounds (scaleButton.getX(), scaleButton.getBottom() + 2, scaleButton.getWidth(), 260);
    auto scalePanelBounds = scalePanel.getLocalBounds().reduced (8, 6);
    auto scalePanelTop = scalePanelBounds.removeFromTop (28);
    scaleAllButton.setBounds (scalePanelTop.removeFromLeft (72).reduced (1, 3));
    scaleRandomButton.setBounds (scalePanelTop.removeFromLeft (96).reduced (1, 3));
    for (int i = 0; i < scaleToggles.size(); ++i)
    {
        const auto column = i / 8;
        const auto row = i % 8;
        scaleToggles[i]->setBounds (scalePanelBounds.getX() + column * (scalePanelBounds.getWidth() / 2),
                                    scalePanelBounds.getY() + row * 27,
                                    scalePanelBounds.getWidth() / 2,
                                    24);
    }
    scalePanel.toFront (false);
    pureLegatoButton.setBounds (settings.removeFromTop (rowH).removeFromLeft (130));
    placeRow (legatoPolyLabel, legatoPolySlider);
    placeRow (polyDensityLabel, polyDensitySlider);

    bounds.removeFromTop (10);
    fretboard.setBounds (bounds);
}

bool GuitarChannelizerAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& file : files)
        if (fileLooksLikeMidi (file))
            return true;

    return false;
}

void GuitarChannelizerAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        juce::File file (path);
        if (file.hasFileExtension ("mid;midi"))
        {
            loadMidiFile (file);
            return;
        }
    }
}

void GuitarChannelizerAudioProcessorEditor::timerCallback()
{
    processorRef.refreshLoadedMidiForCurrentSettings();
    fretboard.setHandTargetRadius (juce::roundToInt (handSpanSlider.getValue()));
    fretboard.setTheme (themeBox.getSelectedItemIndex());
    fretboard.setShape (shapeBox.getSelectedItemIndex());
    fretboard.setColorMode (colorModeBox.getSelectedItemIndex());
    if (auto* colorSeedParameter = processorRef.getValueTreeState().getRawParameterValue (ParameterIDs::fretboardColorSeed))
        fretboard.setColorSeed (juce::roundToInt (colorSeedParameter->load()));
    fretboard.setSnapshot (processorRef.getFretboardSnapshot());
    chordLabel.setText (processorRef.getLiveChordName(), juce::dontSendNotification);
    if (melodyExpanded)
        pianoRoll.setSnapshot (processorRef.getGeneratedMelodySnapshot());
}

void GuitarChannelizerAudioProcessorEditor::chooseMidiFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load MIDI file", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.mid;*.midi");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile())
                                      loadMidiFile (file);
                              });
}

void GuitarChannelizerAudioProcessorEditor::chooseExportFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Export processed MIDI", juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("guitar-channelized.mid"), "*.mid");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (file != juce::File())
                                      statusLabel.setText (processorRef.exportProcessedMidiFile (file), juce::dontSendNotification);
                              });
}

void GuitarChannelizerAudioProcessorEditor::loadMidiFile (const juce::File& file)
{
    statusLabel.setText (processorRef.loadMidiFile (file), juce::dontSendNotification);
}

void GuitarChannelizerAudioProcessorEditor::setMelodyExpanded (bool shouldExpand)
{
    melodyExpanded = shouldExpand;
    cleanView = false;
    setSize (1240, melodyExpanded ? 820 : 360);
    updateMelodyVisibility();
    updateCleanViewVisibility();
    resized();
}

void GuitarChannelizerAudioProcessorEditor::updateMelodyVisibility()
{
    melodyButton.setButtonText (melodyExpanded ? "Hide Melody" : "Melody");

    for (auto* component : { static_cast<juce::Component*> (&pianoRoll),
                             static_cast<juce::Component*> (&generateButton),
                             static_cast<juce::Component*> (&randomizeButton),
                             static_cast<juce::Component*> (&resetButton),
                             static_cast<juce::Component*> (&scaleButton),
                             static_cast<juce::Component*> (&rootButton),
                             static_cast<juce::Component*> (&melodyOutputButton),
                             static_cast<juce::Component*> (&guitaristicButton),
                             static_cast<juce::Component*> (&forceFirstNoteButton),
                             static_cast<juce::Component*> (&complexitySlider),
                             static_cast<juce::Component*> (&densitySlider),
                             static_cast<juce::Component*> (&barsSlider),
                             static_cast<juce::Component*> (&melodySpeedBox),
                             static_cast<juce::Component*> (&keyCountSlider),
                             static_cast<juce::Component*> (&minNoteSlider),
                             static_cast<juce::Component*> (&maxNoteSlider),
                             static_cast<juce::Component*> (&pureLegatoButton),
                             static_cast<juce::Component*> (&legatoPolySlider),
                             static_cast<juce::Component*> (&polyDensitySlider),
                             static_cast<juce::Component*> (&complexityLabel),
                             static_cast<juce::Component*> (&densityLabel),
                             static_cast<juce::Component*> (&barsLabel),
                             static_cast<juce::Component*> (&melodySpeedLabel),
                             static_cast<juce::Component*> (&rootKeyLabel),
                             static_cast<juce::Component*> (&keyCountLabel),
                             static_cast<juce::Component*> (&minNoteLabel),
                             static_cast<juce::Component*> (&maxNoteLabel),
                             static_cast<juce::Component*> (&scaleLabel),
                             static_cast<juce::Component*> (&legatoPolyLabel),
                             static_cast<juce::Component*> (&polyDensityLabel) })
    {
        component->setVisible (melodyExpanded);
    }

    scalePanel.setVisible (melodyExpanded && scalePanelOpen);
    rootPanel.setVisible (melodyExpanded && rootPanelOpen);
}

void GuitarChannelizerAudioProcessorEditor::updateCleanViewVisibility()
{
    cleanViewButton.setButtonText (cleanView ? "Show Settings" : "Hide Settings");

    for (auto* component : { static_cast<juce::Component*> (&titleLabel),
                             static_cast<juce::Component*> (&statusLabel),
                             static_cast<juce::Component*> (&loadButton),
                             static_cast<juce::Component*> (&exportButton),
                             static_cast<juce::Component*> (&melodyButton),
                             static_cast<juce::Component*> (&liveButton),
                             static_cast<juce::Component*> (&invertButton),
                             static_cast<juce::Component*> (&useInputChannelsButton),
                             static_cast<juce::Component*> (&showSameChannelNotesButton),
                             static_cast<juce::Component*> (&latchChordButton),
                             static_cast<juce::Component*> (&preferUpperButton),
                             static_cast<juce::Component*> (&preferredMinFretLabel),
                             static_cast<juce::Component*> (&preferredMinFretSlider),
                             static_cast<juce::Component*> (&handSpanLabel),
                             static_cast<juce::Component*> (&handSpanSlider),
                             static_cast<juce::Component*> (&chordLatchDelayLabel),
                             static_cast<juce::Component*> (&chordLatchDelaySlider),
                             static_cast<juce::Component*> (&themeBox),
                             static_cast<juce::Component*> (&shapeBox),
                             static_cast<juce::Component*> (&colorModeBox),
                             static_cast<juce::Component*> (&fretboardRandomButton) })
    {
        component->setVisible (! cleanView);
    }

    cleanViewButton.setVisible (true);
    chordLabel.setVisible (true);

    if (cleanView)
    {
        for (auto* component : { static_cast<juce::Component*> (&pianoRoll),
                                 static_cast<juce::Component*> (&generateButton),
                                 static_cast<juce::Component*> (&randomizeButton),
                                 static_cast<juce::Component*> (&resetButton),
                                 static_cast<juce::Component*> (&scaleButton),
                                 static_cast<juce::Component*> (&rootButton),
                                 static_cast<juce::Component*> (&melodyOutputButton),
                                 static_cast<juce::Component*> (&guitaristicButton),
                                 static_cast<juce::Component*> (&forceFirstNoteButton),
                                 static_cast<juce::Component*> (&complexitySlider),
                                 static_cast<juce::Component*> (&densitySlider),
                                 static_cast<juce::Component*> (&barsSlider),
                                 static_cast<juce::Component*> (&melodySpeedBox),
                                 static_cast<juce::Component*> (&keyCountSlider),
                                 static_cast<juce::Component*> (&minNoteSlider),
                                 static_cast<juce::Component*> (&maxNoteSlider),
                                 static_cast<juce::Component*> (&pureLegatoButton),
                                 static_cast<juce::Component*> (&legatoPolySlider),
                                 static_cast<juce::Component*> (&polyDensitySlider),
                                 static_cast<juce::Component*> (&complexityLabel),
                                 static_cast<juce::Component*> (&densityLabel),
                                 static_cast<juce::Component*> (&barsLabel),
                                 static_cast<juce::Component*> (&melodySpeedLabel),
                                 static_cast<juce::Component*> (&rootKeyLabel),
                                 static_cast<juce::Component*> (&keyCountLabel),
                                 static_cast<juce::Component*> (&minNoteLabel),
                                 static_cast<juce::Component*> (&maxNoteLabel),
                                 static_cast<juce::Component*> (&scaleLabel),
                                 static_cast<juce::Component*> (&legatoPolyLabel),
                                 static_cast<juce::Component*> (&polyDensityLabel),
                                 static_cast<juce::Component*> (&scalePanel),
                                 static_cast<juce::Component*> (&rootPanel) })
        {
            component->setVisible (false);
        }
    }
}

void GuitarChannelizerAudioProcessorEditor::showScaleMenu()
{
    scalePanelOpen = ! scalePanelOpen;
    if (scalePanelOpen)
        rootPanelOpen = false;
    syncScaleToggles();
    updateMelodyVisibility();
    scalePanel.toFront (false);
}

void GuitarChannelizerAudioProcessorEditor::showRootMenu()
{
    rootPanelOpen = ! rootPanelOpen;
    if (rootPanelOpen)
        scalePanelOpen = false;
    syncRootToggles();
    updateMelodyVisibility();
    rootPanel.toFront (false);
}

void GuitarChannelizerAudioProcessorEditor::refreshScaleButtonText()
{
    scaleButton.setButtonText (processorRef.getMelodyScaleDescription());
}

void GuitarChannelizerAudioProcessorEditor::refreshRootButtonText()
{
    rootButton.setButtonText (processorRef.getMelodyRootDescription());
}

void GuitarChannelizerAudioProcessorEditor::syncScaleToggles()
{
    const auto mask = processorRef.getMelodyScaleMask();
    for (int i = 0; i < scaleToggles.size(); ++i)
        scaleToggles[i]->setToggleState ((mask & (1 << i)) != 0, juce::dontSendNotification);
}

void GuitarChannelizerAudioProcessorEditor::syncRootToggles()
{
    const auto mask = processorRef.getMelodyRootMask();
    for (int i = 0; i < rootToggles.size(); ++i)
        rootToggles[i]->setToggleState ((mask & (1 << i)) != 0, juce::dontSendNotification);
}

void GuitarChannelizerAudioProcessorEditor::selectAllScales()
{
    auto mask = 0;
    for (int i = 0; i < scaleToggles.size(); ++i)
        mask |= 1 << i;

    processorRef.setMelodyScaleMask (mask);
    syncScaleToggles();
    refreshScaleButtonText();
}

void GuitarChannelizerAudioProcessorEditor::randomizeScales()
{
    juce::Random random;
    auto mask = 0;
    const auto count = 1 + random.nextInt (juce::jmin (4, scaleToggles.size()));

    while (__builtin_popcount (static_cast<unsigned int> (mask)) < count)
        mask |= 1 << random.nextInt (scaleToggles.size());

    processorRef.setMelodyScaleMask (mask);
    syncScaleToggles();
    refreshScaleButtonText();
}

void GuitarChannelizerAudioProcessorEditor::selectAllRoots()
{
    processorRef.setMelodyRootMask ((1 << 12) - 1);
    syncRootToggles();
    refreshRootButtonText();
}

void GuitarChannelizerAudioProcessorEditor::randomizeRoots()
{
    juce::Random random;
    auto mask = 0;
    const auto count = 1 + random.nextInt (4);

    while (__builtin_popcount (static_cast<unsigned int> (mask)) < count)
        mask |= 1 << random.nextInt (12);

    processorRef.setMelodyRootMask (mask);
    syncRootToggles();
    refreshRootButtonText();
}

void GuitarChannelizerAudioProcessorEditor::randomizeGenerationSettings()
{
    processorRef.randomizeGenerationSettings();
    syncRootToggles();
    syncScaleToggles();
    refreshRootButtonText();
    refreshScaleButtonText();
}

void GuitarChannelizerAudioProcessorEditor::resetGenerationSettings()
{
    processorRef.resetGenerationSettings();
    syncRootToggles();
    syncScaleToggles();
    refreshRootButtonText();
    refreshScaleButtonText();
}

void GuitarChannelizerAudioProcessorEditor::configureHorizontalSlider (juce::Slider& slider, int min, int max)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    slider.setRange (static_cast<double> (min), static_cast<double> (max), 1.0);
    slider.setColour (juce::Slider::trackColourId, accentColour());
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible (slider);
}

juce::Label& GuitarChannelizerAudioProcessorEditor::makeLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.76f));
    label.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (label);
    return label;
}
