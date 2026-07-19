#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DeltaMatchProcessor.h"
#include "DeltaMatchLookAndFeel.h"

class DeltaMatchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DeltaMatchEditor(DeltaMatchProcessor&);
    ~DeltaMatchEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateAbButtonStates();

    DeltaMatchProcessor& processorRef;
    DeltaMatchLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label brandLabel;

    juce::TextButton aButton { "A" };
    juce::TextButton bButton { "B" };
    juce::TextButton matchButton { "MATCH" };
    juce::TextButton loadAButton { "LOAD A" };
    juce::TextButton loadBButton { "LOAD B" };
    juce::TextButton clearFilesButton { "CLEAR" };
    juce::TextButton testSignalButton { "TEST SIG" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> matchAttachment;

    juce::Rectangle<int> meterABounds, meterBBounds, matchReadoutBounds;
    juce::Rectangle<int> bottomBar;
    juce::Rectangle<int> fileStatusTextArea, brandTextArea;

    float displayLevelA = -100.0f, displayLevelB = -100.0f, displayMatchGain = 0.0f;
    bool currentSidechainPresent = false;
    bool currentFileModeEnabled = false;
    juce::String currentFileNameA, currentFileNameB;

    double introStartMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeltaMatchEditor)
};
