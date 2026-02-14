#include "CustomLookAndFeel.h"

CustomLookAndFeel::CustomLookAndFeel()
{
    // Set global colour scheme
    setColour(juce::ResizableWindow::backgroundColourId, bgDark);
    setColour(juce::Label::textColourId, textPrimary);

    setColour(juce::ComboBox::backgroundColourId, bgLight);
    setColour(juce::ComboBox::textColourId, textPrimary);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, textSecondary);

    setColour(juce::TextButton::buttonColourId, bgLight);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, textPrimary);
    setColour(juce::TextButton::textColourOnId, textPrimary);

    setColour(juce::Slider::backgroundColourId, bgLight);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Slider::thumbColourId, textPrimary);
    setColour(juce::Slider::textBoxTextColourId, textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, bgDark);
    setColour(juce::Slider::textBoxOutlineColourId, border);

    setColour(juce::PopupMenu::backgroundColourId, bgMedium);
    setColour(juce::PopupMenu::textColourId, textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour(juce::PopupMenu::highlightedTextColourId, textPrimary);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(bgLight);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    // Draw arrow
    auto arrowBounds = bounds.removeFromRight(static_cast<float>(height)).reduced(8.0f);
    juce::Path arrow;
    arrow.addTriangle(arrowBounds.getX(), arrowBounds.getCentreY() - 2.0f,
                      arrowBounds.getRight(), arrowBounds.getCentreY() - 2.0f,
                      arrowBounds.getCentreX(), arrowBounds.getCentreY() + 3.0f);
    g.setColour(box.isEnabled() ? textSecondary : textSecondary.withAlpha(0.3f));
    g.fillPath(arrow);
}

void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour&,
                                             bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    bool isOn = button.getToggleState();

    juce::Colour bg = isOn ? accent : bgLight;
    if (isDown) bg = bg.brighter(0.15f);
    else if (isHighlighted) bg = bg.brighter(0.08f);

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(isOn ? accentDim : border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    float trackY = bounds.getCentreY();
    float trackH = 4.0f;

    // Background track
    g.setColour(bgLight);
    g.fillRoundedRectangle(bounds.getX(), trackY - trackH * 0.5f,
                           bounds.getWidth(), trackH, 2.0f);

    // Active track
    g.setColour(accent);
    g.fillRoundedRectangle(bounds.getX(), trackY - trackH * 0.5f,
                           sliderPos - bounds.getX(), trackH, 2.0f);

    // Thumb
    float thumbSize = 10.0f;
    g.setColour(textPrimary);
    g.fillEllipse(sliderPos - thumbSize * 0.5f, trackY - thumbSize * 0.5f,
                  thumbSize, thumbSize);
}

void CustomLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    g.drawText(label.getText(), label.getLocalBounds(),
               label.getJustificationType(), true);
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(12.0f));
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(11.0f));
}

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(bgMedium);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool,
                                          const juce::String& text, const juce::String&,
                                          const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(separator);
        g.fillRect(area.reduced(4, 0).withHeight(1));
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(accent);
        g.fillRoundedRectangle(area.reduced(2).toFloat(), 3.0f);
    }

    g.setColour(isActive ? textPrimary : textSecondary);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));

    auto textArea = area.reduced(8, 0);
    if (isTicked)
    {
        g.drawText(juce::String::charToString(0x2713) + " " + text, textArea,
                   juce::Justification::centredLeft);
    }
    else
    {
        g.drawText(text, textArea, juce::Justification::centredLeft);
    }
}
