#include "PluginEditor.h"
#include <cmath>

namespace
{
    juce::String formatDb(float db)
    {
        if (db <= -99.5f) return "-inf dB";
        return juce::String(db, 1) + " dB";
    }

    juce::String formatGain(float db)
    {
        juce::String sign = db >= 0.0f ? "+" : "";
        return sign + juce::String(db, 1) + " dB";
    }

    constexpr double introDurationMs = 550.0;
}

DeltaMatchEditor::DeltaMatchEditor(DeltaMatchProcessor& p)
    : juce::AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&lookAndFeel);

    titleLabel.setText("DELTA BLIND", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, DeltaMatchLookAndFeel::cyan);
    titleLabel.setFont(DeltaMatchLookAndFeel::brandTitleFont(27.0f).withExtraKerningFactor(0.05f));
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("LOUDNESS-MATCHED A/B COMPARE", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setColour(juce::Label::textColourId, DeltaMatchLookAndFeel::textDim);
    subtitleLabel.setFont(DeltaMatchLookAndFeel::familyFont(12.0f).withExtraKerningFactor(0.08f));
    addAndMakeVisible(subtitleLabel);

    brandLabel.setText(juce::String::fromUTF8("Bumpin Audio \xe2\x80\x94 UNLICENSED"), juce::dontSendNotification);
    brandLabel.setJustificationType(juce::Justification::centredRight);
    brandLabel.setColour(juce::Label::textColourId, DeltaMatchLookAndFeel::textDim.withAlpha(0.5f));
    brandLabel.setFont(DeltaMatchLookAndFeel::familyFont(10.0f));
    addAndMakeVisible(brandLabel);

    aButton.setClickingTogglesState(false);
    aButton.onClick = [this]
    {
        processorRef.apvts.getParameter("useB")->setValueNotifyingHost(0.0f);
        updateAbButtonStates();
    };
    addAndMakeVisible(aButton);

    bButton.setClickingTogglesState(false);
    bButton.onClick = [this]
    {
        processorRef.apvts.getParameter("useB")->setValueNotifyingHost(1.0f);
        updateAbButtonStates();
    };
    addAndMakeVisible(bButton);
    updateAbButtonStates();

    matchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.apvts, "matchEnabled", matchButton);
    addAndMakeVisible(matchButton);

    testSignalButton.setClickingTogglesState(true);
    testSignalButton.setToggleState(processorRef.isTestSignalEnabled(), juce::dontSendNotification);
    testSignalButton.onClick = [this] { processorRef.setTestSignalEnabled(testSignalButton.getToggleState()); };
    addAndMakeVisible(testSignalButton);

    loadAButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load source A...", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& fc)
                                  {
                                      auto file = fc.getResult();
                                      if (file.existsAsFile())
                                          processorRef.loadFileIntoA(file);
                                  });
    };
    addAndMakeVisible(loadAButton);

    loadBButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Load source B...", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this](const juce::FileChooser& fc)
                                  {
                                      auto file = fc.getResult();
                                      if (file.existsAsFile())
                                          processorRef.loadFileIntoB(file);
                                  });
    };
    addAndMakeVisible(loadBButton);

    clearFilesButton.onClick = [this] { processorRef.clearFiles(); };
    addAndMakeVisible(clearFilesButton);

    setSize(640, 460);
    setResizable(true, true);
    // Minimum height must fit: header(64) + bottomBar(64) + content margins
    // (24) + meterRow(110) + gaps(16+12+8) + buttonRow(64) + matchButton(36)
    // + matchReadout(20) = 418px -- rounded up with a small safety margin.
    setResizeLimits(480, 430, 1400, 900);

    introStartMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(30);
}

DeltaMatchEditor::~DeltaMatchEditor()
{
    setLookAndFeel(nullptr);
}

void DeltaMatchEditor::updateAbButtonStates()
{
    bool useB = *processorRef.apvts.getRawParameterValue("useB") > 0.5f;
    aButton.setToggleState(! useB, juce::dontSendNotification);
    bButton.setToggleState(useB, juce::dontSendNotification);
}

void DeltaMatchEditor::timerCallback()
{
    currentSidechainPresent = processorRef.hasSidechainSignal();
    float a = processorRef.getLevelADb();
    float b = processorRef.getLevelBDb();
    float m = processorRef.getMatchGainDb();
    displayLevelA += (a - displayLevelA) * 0.3f;
    displayLevelB += (b - displayLevelB) * 0.3f;
    displayMatchGain += (m - displayMatchGain) * 0.3f;

    currentFileModeEnabled = processorRef.isFileModeEnabled();
    if (currentFileModeEnabled)
    {
        currentFileNameA = processorRef.getFileNameA();
        currentFileNameB = processorRef.getFileNameB();
    }

    updateAbButtonStates();
    repaint();
}

void DeltaMatchEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bgGradient(DeltaMatchLookAndFeel::bg.brighter(0.04f), bounds.getCentre(),
                                     DeltaMatchLookAndFeel::bg.darker(0.2f), bounds.getBottomLeft(), true);
    g.setGradientFill(bgGradient);
    g.fillAll();

    bool useB = *processorRef.apvts.getRawParameterValue("useB") > 0.5f;

    auto drawMeter = [&](juce::Rectangle<int> box, const juce::String& label, float levelDb, bool active)
    {
        auto rb = box.toFloat();
        auto accent = active ? DeltaMatchLookAndFeel::magenta : DeltaMatchLookAndFeel::cyan;

        juce::ColourGradient boxFill(DeltaMatchLookAndFeel::bg.brighter(0.15f).withAlpha(0.75f), rb.getCentreX(), rb.getY(),
                                      DeltaMatchLookAndFeel::bg.withAlpha(0.6f), rb.getCentreX(), rb.getBottom(), false);
        g.setGradientFill(boxFill);
        g.fillRoundedRectangle(rb, DeltaMatchLookAndFeel::familyCornerSize);
        g.setColour(accent.withAlpha(active ? 0.9f : 0.5f));
        g.drawRoundedRectangle(rb, DeltaMatchLookAndFeel::familyCornerSize, active ? 2.0f : 1.0f);

        auto inner = box.reduced(12, 10);
        auto labelArea = inner.removeFromTop(20);
        g.setColour(accent);
        g.setFont(DeltaMatchLookAndFeel::monoFont(13.0f, true).withExtraKerningFactor(0.1f));
        g.drawText(label, labelArea, juce::Justification::centredLeft);

        juce::String valueText = currentSidechainPresent ? formatDb(levelDb) : juce::String("-inf dB");

        if (currentSidechainPresent)
        {
            g.setColour(accent.withAlpha(0.25f));
            g.setFont(DeltaMatchLookAndFeel::monoFont(26.0f, true));
            g.drawText(valueText, inner.translated(0, 1), juce::Justification::centred);
            g.drawText(valueText, inner.expanded(1, 0), juce::Justification::centred);
        }
        g.setColour(currentSidechainPresent ? accent : DeltaMatchLookAndFeel::textDim);
        g.setFont(DeltaMatchLookAndFeel::monoFont(26.0f, true));
        g.drawText(valueText, inner, juce::Justification::centred);

        // Simple horizontal level bar beneath the number.
        auto barArea = inner.removeFromBottom(6);
        g.setColour(DeltaMatchLookAndFeel::grid);
        g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
        if (currentSidechainPresent)
        {
            float t = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f); // -60..0dB -> 0..1
            auto filled = barArea.withWidth((int) (barArea.getWidth() * t));
            g.setColour(accent);
            g.fillRoundedRectangle(filled.toFloat(), 3.0f);
        }
    };

    drawMeter(meterABounds, "A", displayLevelA, ! useB);
    drawMeter(meterBBounds, "B", displayLevelB, useB);

    // Match gain readout.
    {
        auto rb = matchReadoutBounds.toFloat();
        bool matchOn = *processorRef.apvts.getRawParameterValue("matchEnabled") > 0.5f;
        g.setColour(DeltaMatchLookAndFeel::textDim);
        g.setFont(DeltaMatchLookAndFeel::monoFont(11.0f).withExtraKerningFactor(0.04f));
        juce::String matchText = matchOn && currentSidechainPresent
            ? ("APPLIED TO B: " + formatGain(displayMatchGain))
            : juce::String("MATCH OFF -- B PLAYS UNCORRECTED");
        g.drawText(matchText, matchReadoutBounds, juce::Justification::centred);
        juce::ignoreUnused(rb);
    }

    // Bottom bar.
    g.setColour(DeltaMatchLookAndFeel::cyanDim);
    g.drawHorizontalLine(bottomBar.getY(), 0.0f, (float) getWidth());

    g.setFont(DeltaMatchLookAndFeel::monoFont(11.0f));
    g.setColour(currentFileModeEnabled ? DeltaMatchLookAndFeel::cyan : DeltaMatchLookAndFeel::textDim);
    juce::String fileStatus = currentFileModeEnabled
                                   ? ("FILES  A: " + currentFileNameA + "   B: " + currentFileNameB)
                                   : juce::String("LIVE SIDECHAIN MODE (load files to compare offline)");
    g.drawText(fileStatus, fileStatusTextArea, juce::Justification::centredLeft);

    // Brief power-on sweep, same touch as Delta.
    double elapsed = juce::Time::getMillisecondCounterHiRes() - introStartMs;
    if (elapsed < introDurationMs)
    {
        float progress = (float) (elapsed / introDurationMs);
        float fade = 1.0f - progress;
        g.setColour(DeltaMatchLookAndFeel::cyan.withAlpha(0.10f * fade));
        g.fillRect(getLocalBounds());
    }
}

void DeltaMatchEditor::resized()
{
    auto full = getLocalBounds();

    auto header = full.removeFromTop(64).reduced(16, 0);
    titleLabel.setBounds(header.removeFromTop(36));
    subtitleLabel.setBounds(header.removeFromTop(20));

    bottomBar = full.removeFromBottom(64);

    auto content = full.reduced(20, 12);

    auto meterRow = content.removeFromTop(110);
    meterABounds = meterRow.removeFromLeft(meterRow.getWidth() / 2 - 8);
    meterRow.removeFromLeft(16);
    meterBBounds = meterRow;

    content.removeFromTop(16);

    auto buttonRow = content.removeFromTop(64);
    aButton.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2 - 8));
    buttonRow.removeFromLeft(16);
    bButton.setBounds(buttonRow);

    content.removeFromTop(12);
    matchButton.setBounds(content.removeFromTop(36).withSizeKeepingCentre(140, 32));

    content.removeFromTop(8);
    matchReadoutBounds = content.removeFromTop(20);

    auto buttonRowBottom = bottomBar.removeFromTop(40).reduced(8, 6);
    loadAButton.setBounds(buttonRowBottom.removeFromLeft(70));
    buttonRowBottom.removeFromLeft(6);
    loadBButton.setBounds(buttonRowBottom.removeFromLeft(70));
    buttonRowBottom.removeFromLeft(6);
    clearFilesButton.setBounds(buttonRowBottom.removeFromLeft(60));
    buttonRowBottom.removeFromLeft(20);
    testSignalButton.setBounds(buttonRowBottom.removeFromLeft(90));

    auto textRow = bottomBar.reduced(8, 2);
    brandTextArea = textRow.removeFromRight(150);
    brandLabel.setBounds(brandTextArea);
    fileStatusTextArea = textRow;
}
