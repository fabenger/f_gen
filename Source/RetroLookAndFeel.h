#pragma once

#include <JuceHeader.h>

namespace fgen
{
// Light "drum machine hardware" palette: white/silver body, charcoal print, orange accents.
namespace Colours
{
    const juce::Colour bg          { 0xffdedcd6 };   // chassis
    const juce::Colour panel       { 0xfff7f6f2 };   // white panels
    const juce::Colour panelEdge   { 0xffc4c1b9 };
    const juce::Colour ink         { 0xff26251f };   // printed text
    const juce::Colour inkDim      { 0xff8b887f };
    const juce::Colour orange      { 0xfff26a1b };
    const juce::Colour red         { 0xffe0352b };
    const juce::Colour teal        { 0xff2f9e8f };
    const juce::Colour display     { 0xffe8e6df };   // inset display
    const juce::Colour displayInk  { 0xff2d2c27 };
    const juce::Colour displayDim  { 0xffb9b5ab };
    const juce::Colour cap         { 0xffe2dfd7 };   // step key caps
    const juce::Colour capDark     { 0xff3a3934 };   // black knob caps
}

inline juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font monoFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

class FGenLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FGenLookAndFeel()
    {
        using namespace juce;
        setColour (ResizableWindow::backgroundColourId, Colours::bg);
        setColour (Slider::rotarySliderFillColourId, Colours::orange);
        setColour (Label::textColourId, Colours::ink);
        setColour (ComboBox::backgroundColourId, Colours::panel);
        setColour (ComboBox::textColourId, Colours::ink);
        setColour (ComboBox::outlineColourId, Colours::panelEdge);
        setColour (ComboBox::arrowColourId, Colours::ink);
        setColour (PopupMenu::backgroundColourId, Colours::panel);
        setColour (PopupMenu::textColourId, Colours::ink);
        setColour (PopupMenu::headerTextColourId, Colours::inkDim);
        setColour (PopupMenu::highlightedBackgroundColourId, Colours::orange);
        setColour (PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (TextButton::buttonColourId, Colours::cap);
        setColour (TextButton::textColourOffId, Colours::ink);
        setColour (TextButton::buttonOnColourId, Colours::orange);
        setColour (TextButton::textColourOnId, juce::Colours::white);
        setColour (BubbleComponent::backgroundColourId, Colours::ink);
        setColour (BubbleComponent::outlineColourId, Colours::ink);
        setColour (TooltipWindow::backgroundColourId, Colours::ink);
        setColour (TooltipWindow::textColourId, Colours::panel);
        setColour (TooltipWindow::outlineColourId, Colours::ink);
    }

    juce::Font getLabelFont (juce::Label&) override                      { return uiFont (11.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override                { return uiFont (13.0f, true); }
    juce::Font getPopupMenuFont() override                               { return uiFont (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override     { return uiFont (juce::jmin (13.0f, h * 0.5f), true); }
    juce::Font getSliderPopupFont (juce::Slider&) override               { return uiFont (12.0f, true); }

    void drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body) override
    {
        g.setColour (Colours::ink);
        g.fillRoundedRectangle (body, 3.0f);
    }

    juce::Colour getBubbleTextColour() { return Colours::panel; }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // printed scale
        g.setColour (Colours::ink.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const float len = (i % 5 == 0) ? 4.0f : 2.5f;
            g.drawLine ({ c.getPointOnCircumference (radius, a), c.getPointOnCircumference (radius - len, a) }, 1.0f);
        }

        // value arc
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zeroAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
        Path track, arc;
        track.addCentredArc (c.x, c.y, radius - 6.0f, radius - 6.0f, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::panelEdge.withAlpha (0.6f));
        g.strokePath (track, PathStrokeType (2.0f));
        arc.addCentredArc (c.x, c.y, radius - 6.0f, radius - 6.0f, 0.0f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
        g.setColour (Colours::orange);
        g.strokePath (arc, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));

        // black knob cap with skirt
        const float kr = radius - 9.5f;
        auto skirt = Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
        g.setColour (Colour (0x33000000));
        g.fillEllipse (skirt.translated (0.0f, 2.0f));
        g.setColour (Colour (0xff1e1d1a));
        g.fillEllipse (skirt);
        auto top = skirt.reduced (kr * 0.18f);
        g.setGradientFill (ColourGradient (Colour (0xff55534c), top.getX(), top.getY(), Colour (0xff2b2a26), top.getRight(), top.getBottom(), false));
        g.fillEllipse (top);

        // white pointer line
        g.setColour (juce::Colours::white);
        g.drawLine ({ c.getPointOnCircumference (kr * 0.3f, angle), c.getPointOnCircumference (kr - 1.0f, angle) }, 2.2f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto base = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgc;
        if (over) base = base.brighter (0.05f);
        if (down) base = base.darker (0.1f);

        g.setColour (Colour (0x40000000));
        g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 1.5f), 3.0f);
        auto face = r.translated (0.0f, down ? 1.0f : 0.0f).withTrimmedBottom (1.0f);
        g.setGradientFill (ColourGradient (base.brighter (0.06f), face.getX(), face.getY(), base.darker (0.06f), face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 3.0f);
        g.setColour (base.darker (0.25f));
        g.drawRoundedRectangle (face, 3.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        auto col = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
        g.setColour (col);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        auto led = Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 6.0f, r.getCentreY() });
        auto col = b.findColour (ToggleButton::tickColourId);
        if (col == Colour() || col == Colours::ink) col = Colours::red;
        if (on)
        {
            g.setColour (col.withAlpha (0.3f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on ? col : Colour (0xffc9c5bb));
        g.fillEllipse (led);
        g.setColour (Colour (0x55000000));
        g.drawEllipse (led, 0.8f);
        g.setColour (over ? Colours::ink : Colours::ink.withAlpha (0.85f));
        g.setFont (uiFont (11.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (15.0f), Justification::centredLeft);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        Path arrow;
        const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (Colours::orange);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};

} // namespace fgen
