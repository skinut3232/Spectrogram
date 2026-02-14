#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;

    // Colours
    static inline const juce::Colour bgDark      {0xff12121a};
    static inline const juce::Colour bgMedium    {0xff1e1e2e};
    static inline const juce::Colour bgLight     {0xff2a2a3e};
    static inline const juce::Colour accent       {0xff6366f1};
    static inline const juce::Colour accentDim    {0xff4f46e5};
    static inline const juce::Colour textPrimary  {0xffe2e8f0};
    static inline const juce::Colour textSecondary{0xff94a3b8};
    static inline const juce::Colour border       {0xff3f3f5a};
    static inline const juce::Colour separator    {0xff2d2d44};

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
};
