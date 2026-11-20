#pragma once

#include <JuceHeader.h>

namespace fgen
{
namespace Colours
{
    const juce::Colour bg        { 0xff1d1a17 };
    const juce::Colour panel     { 0xff2c2823 };
    const juce::Colour panelHi   { 0xff3b352e };
    const juce::Colour cream     { 0xffe9dfc7 };
    const juce::Colour creamDim  { 0xffa89f8a };
    const juce::Colour orange    { 0xffee7a2e };
    const juce::Colour red       { 0xffd8453a };
    const juce::Colour teal      { 0xff57b8a6 };
    const juce::Colour lcdBg     { 0xff17120a };
    const juce::Colour lcd       { 0xffffb347 };
    const juce::Colour lcdDim    { 0xff6b4a1f };
}

inline juce::Font monoFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, Colours::bg);
        setColour (Slider::rotarySliderFillColourId, Colours::orange);
        setColour (Slider::thumbColourId, Colours::cream);
        setColour (Label::textColourId, Colours::cream);
        setColour (ComboBox::backgroundColourId, Colours::lcdBg);
        setColour (ComboBox::textColourId, Colours::lcd);
        setColour (ComboBox::outlineColourId, Colours::panelHi);
        setColour (ComboBox::arrowColourId, Colours::lcd);
        setColour (PopupMenu::backgroundColourId, Colours::lcdBg);
        setColour (PopupMenu::textColourId, Colours::lcd);
        setColour (PopupMenu::highlightedBackgroundColourId, Colours::orange);
        setColour (PopupMenu::highlightedTextColourId, Colours::lcdBg);
        setColour (TextButton::buttonColourId, Colours::cream);
        setColour (TextButton::textColourOffId, Colours::bg);
        setColour (TextButton::buttonOnColourId, Colours::orange);
        setColour (TextButton::textColourOnId, Colours::bg);
        setColour (BubbleComponent::backgroundColourId, Colours::lcdBg);
        setColour (BubbleComponent::outlineColourId, Colours::orange);
        setColour (TooltipWindow::backgroundColourId, Colours::lcdBg);
        setColour (TooltipWindow::textColourId, Colours::lcd);
    }

    juce::Font getLabelFont (juce::Label&) override                      { return monoFont (11.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override                { return monoFont (13.0f, true); }
    juce::Font getPopupMenuFont() override                               { return monoFont (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override     { return monoFont (juce::jmin (13.0f, h * 0.5f), true); }
    juce::Font getSliderPopupFont (juce::Slider&) override               { return monoFont (12.0f, true); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // scale ticks
        g.setColour (Colours::creamDim.withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const auto p1 = c.getPointOnCircumference (radius, a);
            const auto p2 = c.getPointOnCircumference (radius - 3.0f, a);
            g.drawLine ({ p1, p2 }, 1.0f);
        }

        // value arc
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
        Path arc;
        arc.addCentredArc (c.x, c.y, radius - 5.5f, radius - 5.5f, 0.0f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
        g.setColour (s.isEnabled() ? Colours::orange : Colours::creamDim);
        g.strokePath (arc, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));

        // knob body
        const float kr = radius - 9.0f;
        auto knob = Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
        g.setColour (Colour (0x66000000));
        g.fillEllipse (knob.translated (0.0f, 2.0f));
        g.setGradientFill (ColourGradient (Colour (0xff4a433a), knob.getX(), knob.getY(),
                                           Colour (0xff221e1a), knob.getRight(), knob.getBottom(), false));
        g.fillEllipse (knob);
        g.setColour (Colour (0xff595145));
        g.drawEllipse (knob.reduced (1.0f), 1.0f);

        // pointer
        g.setColour (Colours::cream);
        g.drawLine ({ c.getPointOnCircumference (kr * 0.25f, angle), c.getPointOnCircumference (kr - 1.5f, angle) }, 2.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto base = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgc;
        if (over) base = base.brighter (0.08f);
        if (down) base = base.darker (0.15f);

        g.setColour (Colour (0x88000000));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 2.0f), 3.0f);
        auto face = r.translated (0.0f, down ? 1.5f : 0.0f).withTrimmedBottom (1.5f);
        g.setGradientFill (ColourGradient (base.brighter (0.1f), face.getX(), face.getY(), base.darker (0.12f), face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 3.0f);
        g.setColour (base.darker (0.4f));
        g.drawRoundedRectangle (face, 3.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        auto led = Rectangle<float> (9.0f, 9.0f).withCentre ({ r.getX() + 7.0f, r.getCentreY() });
        auto col = b.findColour (ToggleButton::tickColourId);
        if (col == Colour()) col = Colours::orange;
        if (on)
        {
            g.setColour (col.withAlpha (0.35f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on ? col : Colour (0xff3e2c1c));
        g.fillEllipse (led);
        g.setColour (Colour (0xff111111));
        g.drawEllipse (led, 1.0f);
        g.setColour (over ? Colours::cream : Colours::cream.withAlpha (0.85f));
        g.setFont (monoFont (11.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (16.0f), Justification::centredLeft);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (Colours::lcdBg);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.hasKeyboardFocus (true) ? Colours::orange : Colours::panelHi);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        Path arrow;
        const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (Colours::lcd);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};

} // namespace fgen
