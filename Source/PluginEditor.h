#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <functional>

class FretboardView final : public juce::Component
{
public:
    void setSnapshot (const FretboardSnapshot& newSnapshot);
    void setHandTargetCallbacks (std::function<void (int)> changed, std::function<void()> cleared);
    void setHandTargetRadius (int radius);
    void setTheme (int theme);
    void setShape (int shape);
    void setColorSeed (int seed);
    void setColorMode (int mode);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

private:
    int fretForPoint (juce::Point<float> point) const;
    float fretPositionForPoint (juce::Point<float> point) const;
    juce::Rectangle<float> getNeckBounds() const;

    FretboardSnapshot snapshot;
    std::function<void (int)> handTargetChanged;
    std::function<void()> handTargetCleared;
    int hoverTargetFret = -1;
    float hoverFretPosition = -1.0f;
    int hoverRadius = 4;
    int themeIndex = 0;
    int shapeIndex = 0;
    int colorSeed = 180;
    int colorMode = 0;
};

class PopupPanel final : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
};

class PianoRollView final : public juce::Component
{
public:
    void setSnapshot (const GeneratedMelodySnapshot& newSnapshot);
    void paint (juce::Graphics& g) override;

private:
    GeneratedMelodySnapshot snapshot;
};

class GuitarChannelizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    public juce::FileDragAndDropTarget,
                                                    private juce::Timer
{
public:
    explicit GuitarChannelizerAudioProcessorEditor (GuitarChannelizerAudioProcessor& processor);
    ~GuitarChannelizerAudioProcessorEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void chooseMidiFile();
    void chooseExportFile();
    void loadMidiFile (const juce::File& file);
    void setMelodyExpanded (bool shouldExpand);
    void updateMelodyVisibility();
    void updateCleanViewVisibility();
    void showScaleMenu();
    void showRootMenu();
    void refreshScaleButtonText();
    void refreshRootButtonText();
    void syncScaleToggles();
    void syncRootToggles();
    void selectAllScales();
    void randomizeScales();
    void selectAllRoots();
    void randomizeRoots();
    void randomizeGenerationSettings();
    void resetGenerationSettings();
    void configureHorizontalSlider (juce::Slider& slider, int min, int max);
    juce::Label& makeLabel (juce::Label& label, const juce::String& text);

    GuitarChannelizerAudioProcessor& processorRef;
    FretboardView fretboard;
    PianoRollView pianoRoll;
    juce::TextButton loadButton { "Load MIDI" };
    juce::TextButton exportButton { "Export MIDI" };
    juce::TextButton melodyButton { "Melody" };
    juce::TextButton cleanViewButton { "Hide Settings" };
    juce::TextButton generateButton { "Generate" };
    juce::TextButton scaleButton { "Scales" };
    juce::TextButton rootButton { "Roots" };
    juce::TextButton randomizeButton { "Randomize" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton scaleAllButton { "All" };
    juce::TextButton scaleRandomButton { "Random" };
    juce::TextButton rootAllButton { "All" };
    juce::TextButton rootRandomButton { "Random" };
    juce::TextButton fretboardRandomButton { "Random" };
    PopupPanel scalePanel;
    PopupPanel rootPanel;
    juce::OwnedArray<juce::ToggleButton> scaleToggles;
    juce::OwnedArray<juce::ToggleButton> rootToggles;
    juce::ToggleButton invertButton { "Invert channels" };
    juce::ToggleButton useInputChannelsButton { "Use input channels" };
    juce::ToggleButton showSameChannelNotesButton { "Show string stacks" };
    juce::ToggleButton latchChordButton { "Latch chord" };
    juce::ToggleButton preferUpperButton { "Prefer higher frets" };
    juce::ToggleButton liveButton { "Process live input" };
    juce::ToggleButton melodyOutputButton { "Output generated" };
    juce::ToggleButton guitaristicButton { "Guitaristic" };
    juce::Slider preferredMinFretSlider;
    juce::Slider handSpanSlider;
    juce::Slider chordLatchDelaySlider;
    juce::Slider complexitySlider;
    juce::Slider densitySlider;
    juce::Slider barsSlider;
    juce::Slider keyCountSlider;
    juce::Slider minNoteSlider;
    juce::Slider maxNoteSlider;
    juce::Slider legatoPolySlider;
    juce::Slider polyDensitySlider;
    juce::ComboBox themeBox;
    juce::ComboBox shapeBox;
    juce::ComboBox colorModeBox;
    juce::ComboBox melodySpeedBox;
    juce::ToggleButton pureLegatoButton { "Pure legato" };
    juce::ToggleButton forceFirstNoteButton { "Start immediately" };
    juce::Label preferredMinFretLabel;
    juce::Label handSpanLabel;
    juce::Label chordLatchDelayLabel;
    juce::Label complexityLabel;
    juce::Label densityLabel;
    juce::Label barsLabel;
    juce::Label rootKeyLabel;
    juce::Label keyCountLabel;
    juce::Label minNoteLabel;
    juce::Label maxNoteLabel;
    juce::Label scaleLabel;
    juce::Label legatoPolyLabel;
    juce::Label polyDensityLabel;
    juce::Label melodySpeedLabel;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label chordLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<ButtonAttachment> invertAttachment;
    std::unique_ptr<ButtonAttachment> useInputChannelsAttachment;
    std::unique_ptr<ButtonAttachment> showSameChannelNotesAttachment;
    std::unique_ptr<ButtonAttachment> latchChordAttachment;
    std::unique_ptr<ButtonAttachment> preferUpperAttachment;
    std::unique_ptr<ButtonAttachment> liveAttachment;
    std::unique_ptr<SliderAttachment> preferredMinFretAttachment;
    std::unique_ptr<SliderAttachment> handSpanAttachment;
    std::unique_ptr<SliderAttachment> chordLatchDelayAttachment;
    std::unique_ptr<ButtonAttachment> melodyOutputAttachment;
    std::unique_ptr<ButtonAttachment> guitaristicAttachment;
    std::unique_ptr<SliderAttachment> complexityAttachment;
    std::unique_ptr<SliderAttachment> densityAttachment;
    std::unique_ptr<SliderAttachment> barsAttachment;
    std::unique_ptr<SliderAttachment> keyCountAttachment;
    std::unique_ptr<SliderAttachment> minNoteAttachment;
    std::unique_ptr<SliderAttachment> maxNoteAttachment;
    std::unique_ptr<ButtonAttachment> pureLegatoAttachment;
    std::unique_ptr<ButtonAttachment> forceFirstNoteAttachment;
    std::unique_ptr<SliderAttachment> legatoPolyAttachment;
    std::unique_ptr<SliderAttachment> polyDensityAttachment;
    std::unique_ptr<ComboBoxAttachment> themeAttachment;
    std::unique_ptr<ComboBoxAttachment> shapeAttachment;
    std::unique_ptr<ComboBoxAttachment> colorModeAttachment;
    std::unique_ptr<ComboBoxAttachment> melodySpeedAttachment;
    bool melodyExpanded = false;
    bool cleanView = false;
    bool scalePanelOpen = false;
    bool rootPanelOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarChannelizerAudioProcessorEditor)
};
