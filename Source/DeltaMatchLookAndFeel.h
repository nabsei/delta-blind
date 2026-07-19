#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Same palette and chrome conventions as Delta -- same audience (audio
// engineers), same family. See DeltaLookAndFeel.h for the full rationale;
// this is the identical treatment, just renamed for this second tool in
// the Delta line.
class DeltaMatchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    inline static const juce::Colour bg        { 0xff0a0912 };
    inline static const juce::Colour deepBlue  { 0xff1a1a4a };
    inline static const juce::Colour cyan      { 0xff29c2d6 };
    inline static const juce::Colour cyanDim   { 0xff123a40 };
    inline static const juce::Colour violet    { 0xff7a5ad6 };
    inline static const juce::Colour magenta   { 0xffe0479c };
    inline static const juce::Colour grid      { 0xff141228 };
    inline static const juce::Colour textDim   { 0xff7a7a95 };

    static constexpr float familyCornerSize = 6.0f;

    DeltaMatchLookAndFeel()
    {
        setColour(juce::TextButton::buttonColourId, bg);
        setColour(juce::TextButton::buttonOnColourId, magenta.withAlpha(0.5f));
        setColour(juce::TextButton::textColourOffId, cyanDim.brighter(0.6f));
        setColour(juce::TextButton::textColourOnId, magenta);
    }

    static juce::Font monoFont(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                             height, bold ? juce::Font::bold : juce::Font::plain));
    }

    static juce::Font familyFont(float height, bool bold = false)
    {
        return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
    }

    static juce::Font brandTitleFont(float height)
    {
        return juce::Font(juce::FontOptions(juce::String("Avenir Next"), height, juce::Font::bold));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        bool engaged = button.getToggleState() || shouldDrawButtonAsDown;

        float baseAlpha = engaged ? 0.5f : (shouldDrawButtonAsHighlighted ? 0.16f : 0.0f);
        if (baseAlpha > 0.0f)
        {
            juce::Colour base = engaged ? magenta : cyan;
            juce::ColourGradient fillGrad(base.withAlpha(baseAlpha * 1.3f), bounds.getCentreX(), bounds.getY(),
                                           base.withAlpha(baseAlpha * 0.55f), bounds.getCentreX(), bounds.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(bounds, familyCornerSize * 0.6f);
        }

        {
            auto gloss = bounds.withHeight(bounds.getHeight() * 0.45f).reduced(1.0f, 0.0f);
            juce::ColourGradient glossGrad(juce::Colours::white.withAlpha(0.09f), gloss.getCentreX(), gloss.getY(),
                                            juce::Colours::white.withAlpha(0.0f), gloss.getCentreX(), gloss.getBottom(), false);
            g.setGradientFill(glossGrad);
            g.fillRoundedRectangle(gloss, familyCornerSize * 0.5f);
        }

        g.setColour(engaged ? magenta : (shouldDrawButtonAsHighlighted ? cyan : cyanDim));
        g.drawRoundedRectangle(bounds, familyCornerSize * 0.6f, shouldDrawButtonAsHighlighted ? 1.5f : 1.0f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int height) override
    {
        return monoFont((float) height * 0.42f, true);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return monoFont(13.0f);
    }
};
