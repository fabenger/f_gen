#include "PluginEditor.h"

using namespace fgen;
namespace C = fgen::Colours;

//==============================================================================
Knob::Knob (const juce::String& labelText) : text (labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr, 1500);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedBottom (14));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (labelColour);
    g.setFont (monoFont (10.0f, true));
    g.drawText (text, getLocalBounds().removeFromBottom (14), juce::Justification::centred);
}

//==============================================================================
SlotStrip::SlotStrip (FGenEditor& e, FGenProcessor& p, int index) : editor (e), proc (p), slot (index)
{
    addAndMakeVisible (mute);
    addAndMakeVisible (lock);
    addAndMakeVisible (rand);
    mute.setColour (juce::ToggleButton::tickColourId, C::red);
    lock.setColour (juce::ToggleButton::tickColourId, C::teal);
    muteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, FGenProcessor::slotParam (slot, "mute"), mute);
    lockAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, FGenProcessor::slotParam (slot, "lock"), lock);
    lock.setTooltip ("Locked slots are skipped by RANDOMIZE ALL");
    rand.setTooltip ("New rhythm for this slot");
    rand.onClick = [this] { proc.randomizeSlot (slot); editor.selectSlot (slot); };
}

void SlotStrip::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto bottom = r.removeFromBottom (22);
    mute.setBounds (bottom.removeFromLeft (70));
    lock.setBounds (bottom.removeFromLeft (70));
    rand.setBounds (bottom.removeFromRight (52));
}

void SlotStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? C::panelHi : C::panel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (dragOver ? C::lcd : (selected ? C::orange : juce::Colour (0xff15130f)));
    g.drawRoundedRectangle (r, 5.0f, selected || dragOver ? 2.0f : 1.0f);

    auto inner = getLocalBounds().reduced (10, 8);
    auto top = inner.removeFromTop (34);

    // big slot letter
    auto letterBox = top.removeFromLeft (34).toFloat();
    g.setColour (selected ? C::orange : C::cream.withAlpha (0.8f));
    g.fillRoundedRectangle (letterBox, 4.0f);
    g.setColour (C::bg);
    g.setFont (monoFont (22.0f, true));
    g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + slot)), letterBox, juce::Justification::centred);

    // activity LED
    auto ledR = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) top.getRight() - 6.0f, (float) top.getY() + 8.0f });
    if (led > 0.02f)
    {
        g.setColour (C::orange.withAlpha (0.4f * led));
        g.fillEllipse (ledR.expanded (4.0f));
    }
    g.setColour (C::orange.withAlpha (0.25f + 0.75f * led));
    g.fillEllipse (ledR);

    // LCD with sample info
    auto lcd = inner.removeFromTop (inner.getHeight() - 26).reduced (0, 4);
    g.setColour (C::lcdBg);
    g.fillRoundedRectangle (lcd.toFloat(), 3.0f);
    auto sd = proc.getSample (slot);
    auto styleIdx = (int) proc.apvts.getRawParameterValue (FGenProcessor::slotParam (slot, "style"))->load();
    g.setFont (monoFont (12.0f, true));
    g.setColour (C::lcd);
    auto t = lcd.reduced (6, 3);
    g.drawText (sd != nullptr ? sd->name : juce::String ("-- drop sample --"), t.removeFromTop (16), juce::Justification::centredLeft, true);
    g.setFont (monoFont (11.0f));
    g.setColour (C::lcd.withAlpha (0.7f));
    juce::String info = sd != nullptr ? ("ROOT " + sd->root.description) : juce::String ("wav/aif/flac/mp3");
    g.drawText (info, t.removeFromTop (14), juce::Justification::centredLeft, true);
    g.drawText (styleNames()[styleIdx], t.removeFromTop (14), juce::Justification::centredLeft, true);

}

void SlotStrip::mouseDown (const juce::MouseEvent& e)
{
    editor.selectSlot (slot);
    if (e.getNumberOfClicks() > 1) editor.openLoadDialog (slot);
}

void SlotStrip::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (! files.isEmpty()) editor.loadFileInto (slot, juce::File (files[0]));
    repaint();
}

//==============================================================================
void WaveDisplay::rebuild()
{
    peaks.clear();
    auto sd = proc.getSample (slot);
    if (sd != nullptr)
    {
        const int w = juce::jmax (1, getWidth() - 12);
        const int n = sd->buffer.getNumSamples();
        peaks.resize ((size_t) w, 0.0f);
        for (int x = 0; x < w; ++x)
        {
            const int a = (int) ((juce::int64) x * n / w), b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * n / w));
            float m = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                m = juce::jmax (m, sd->buffer.getMagnitude (ch, a, juce::jmin (b, n) - a));
            peaks[(size_t) x] = m;
        }
        float mx = 0.0f;
        for (auto p : peaks) mx = juce::jmax (mx, p);
        if (mx > 0.0f) for (auto& p : peaks) p /= mx;
    }
    repaint();
}

void WaveDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (C::lcdBg);
    g.fillRoundedRectangle (r, 4.0f);

    // scanlines for a CRT-ish feel
    g.setColour (juce::Colour (0x0affb347));
    for (int y = 0; y < getHeight(); y += 3) g.drawHorizontalLine (y, 2.0f, r.getRight() - 2.0f);

    auto sd = proc.getSample (slot);
    auto inner = r.reduced (6.0f, 18.0f);
    if (sd == nullptr || peaks.empty())
    {
        g.setColour (C::lcdDim);
        g.setFont (monoFont (14.0f, true));
        g.drawText ("DROP A ONE-SHOT HERE  /  CLICK LOAD", r, juce::Justification::centred);
        return;
    }

    const float start = proc.apvts.getRawParameterValue (FGenProcessor::slotParam (slot, "start"))->load();
    const bool rev = proc.apvts.getRawParameterValue (FGenProcessor::slotParam (slot, "reverse"))->load() > 0.5f;
    const float cy = inner.getCentreY(), hh = inner.getHeight() * 0.5f;
    for (size_t x = 0; x < peaks.size(); ++x)
    {
        const size_t idx = rev ? peaks.size() - 1 - x : x;
        const float prop = (float) idx / (float) peaks.size();
        g.setColour (prop < start ? C::lcdDim : C::lcd);
        const float h = juce::jmax (0.5f, peaks[idx] * hh);
        g.drawVerticalLine ((int) (inner.getX() + (float) x), cy - h, cy + h);
    }

    // start marker
    const float sx = inner.getX() + (rev ? 1.0f - start : start) * inner.getWidth();
    g.setColour (C::orange);
    g.drawVerticalLine ((int) sx, inner.getY() - 8.0f, inner.getBottom() + 8.0f);

    g.setFont (monoFont (11.0f, true));
    g.setColour (C::lcd);
    const double secs = sd->buffer.getNumSamples() / sd->sampleRate;
    g.drawText (sd->name, r.reduced (8.0f, 3.0f).removeFromTop (14.0f), juce::Justification::centredLeft, true);
    g.drawText (juce::String (secs, 2) + " s  " + juce::String ((int) sd->sampleRate) + " Hz", r.reduced (8.0f, 3.0f).removeFromTop (14.0f),
                juce::Justification::centredRight);
    const int effRoot = proc.getEffectiveRoot (slot);
    const bool autoRoot = proc.apvts.getRawParameterValue (FGenProcessor::slotParam (slot, "root"))->load() < 0.5f;
    g.drawText ("DETECTED: " + sd->root.description + (autoRoot ? "" : "   ROOT SET: " + noteNames()[effRoot]),
                r.reduced (8.0f, 3.0f).removeFromBottom (14.0f), juce::Justification::centredLeft);
}

void WaveDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty()) editor.loadFileInto (slot, juce::File (files[0]));
}

//==============================================================================
juce::Rectangle<float> StepGrid::cellBounds (int step) const
{
    const int bars = proc.getBars();
    const float labelW = 30.0f;
    const float w = (getWidth() - labelW - 9.0f) / 16.0f;
    const float h = juce::jmin (40.0f, (float) getHeight() / (float) bars);
    const int row = step / 16, col = step % 16;
    const float groupGap = (col / 4) * 3.0f;
    return { labelW + col * w + groupGap - 4.5f, row * h, w - 3.0f, h - 3.0f };
}

int StepGrid::stepAt (juce::Point<int> p) const
{
    const int total = proc.getBars() * 16;
    for (int i = 0; i < total; ++i)
        if (cellBounds (i).expanded (1.5f).contains (p.toFloat())) return i;
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    const int bars = proc.getBars();
    const int total = bars * 16;
    const int play = proc.getPlayStep();
    const float h = juce::jmin (40.0f, (float) getHeight() / (float) bars);

    for (int b = 0; b < bars; ++b)
    {
        g.setColour (C::creamDim);
        g.setFont (monoFont (11.0f, true));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (0.0f, b * h, 24.0f, h - 3.0f), juce::Justification::centred);
    }

    for (int i = 0; i < total; ++i)
    {
        auto r = cellBounds (i);
        const auto st = proc.getStep (slot, i);
        const bool beat = (i % 4) == 0;
        g.setColour (beat ? juce::Colour (0xff3d362f) : juce::Colour (0xff302a24));
        g.fillRoundedRectangle (r, 2.5f);

        if (st.on)
        {
            const float v = st.vel / 127.0f;
            auto fill = r.reduced (2.0f);
            g.setColour (C::orange.withAlpha (0.25f));
            g.fillRoundedRectangle (fill, 2.0f);
            auto bar = fill.withTrimmedTop (fill.getHeight() * (1.0f - v));
            g.setColour (C::orange.interpolatedWith (C::lcd, 1.0f - v));
            g.fillRoundedRectangle (bar, 2.0f);

            if (st.ratchet > 1)
            {
                g.setColour (C::bg);
                for (int k = 0; k < st.ratchet; ++k)
                    g.fillEllipse (fill.getX() + 3.0f + k * 5.0f, fill.getY() + 3.0f, 3.0f, 3.0f);
            }
            if (st.degree != 0)
            {
                g.setColour (C::bg);
                g.setFont (monoFont (10.0f, true));
                g.drawText ((st.degree > 0 ? "+" : "") + juce::String (st.degree), fill.withTrimmedTop (fill.getHeight() * 0.4f),
                            juce::Justification::centred);
            }
        }

        if (i == play)
        {
            g.setColour (C::cream.withAlpha (0.85f));
            g.drawRoundedRectangle (r, 2.5f, 1.5f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int s = stepAt (e.getPosition());
    if (s < 0) return;

    if (e.mods.isPopupMenu())
    {
        showStepMenu (s);
        return;
    }

    proc.pushUndo (slot);
    auto st = proc.getStep (slot, s);
    st.on = ! st.on;
    if (st.on && st.vel < 20) st.vel = 100;
    paintOn = st.on;
    lastPainted = s;
    proc.setStep (slot, s, st);
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const int s = stepAt (e.getPosition());
    if (s < 0 || s == lastPainted) return;
    auto st = proc.getStep (slot, s);
    st.on = paintOn;
    proc.setStep (slot, s, st);
    lastPainted = s;
    repaint();
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int s = stepAt (e.getPosition());
    if (s < 0) return;
    auto st = proc.getStep (slot, s);
    if (! st.on) return;
    st.vel = juce::jlimit (10, 127, st.vel + (w.deltaY > 0 ? 8 : -8));
    proc.setStep (slot, s, st);
    repaint();
}

void StepGrid::showStepMenu (int step)
{
    auto st = proc.getStep (slot, step);
    juce::PopupMenu m, vel, deg, rat;
    for (int v : { 30, 55, 80, 100, 127 }) vel.addItem (100 + v, juce::String (v), true, st.vel == v);
    for (int d = -7; d <= 7; ++d) deg.addItem (300 + d + 7, (d > 0 ? "+" : "") + juce::String (d) + (d == 0 ? " (root)" : ""), true, st.degree == d);
    for (int r = 1; r <= 3; ++r) rat.addItem (400 + r, r == 1 ? "off" : juce::String (r) + "x", true, st.ratchet == r);
    m.addSectionHeader ("STEP " + juce::String (step % 16 + 1) + "  BAR " + juce::String (step / 16 + 1));
    m.addItem (1, st.on ? "Turn off" : "Turn on");
    m.addSubMenu ("Velocity", vel);
    m.addSubMenu ("Note (scale degree)", deg);
    m.addSubMenu ("Ratchet", rat);

    juce::Component::SafePointer<StepGrid> safe (this);
    const int s = slot;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, step, s] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        p.pushUndo (s);
        auto st2 = p.getStep (s, step);
        if (r == 1) st2.on = ! st2.on;
        else if (r >= 100 && r < 300) { st2.vel = r - 100; st2.on = true; }
        else if (r >= 300 && r < 400) { st2.degree = r - 300 - 7; st2.on = true; }
        else if (r >= 400) { st2.ratchet = r - 400; st2.on = true; }
        p.setStep (s, step, st2);
        safe->repaint();
    });
}

//==============================================================================
FGenEditor::FGenEditor (FGenProcessor& p)
    : AudioProcessorEditor (p), proc (p), wave (*this, p), grid (p)
{
    setLookAndFeel (&lnf);

    // global
    keyBox.addItemList (noteNames(), 1);
    scaleBox.addItemList (scaleNames(), 1);
    barsBox.addItemList ({ "2 BARS", "4 BARS", "8 BARS" }, 1);
    modeBox.addItemList ({ "HOST SYNC", "MIDI TRIGGER" }, 1);
    for (auto* c : { &keyBox, &scaleBox, &barsBox, &modeBox }) addAndMakeVisible (c);
    keyAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    barsAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "bars", barsBox);
    modeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", modeBox);
    modeBox.setTooltip ("HOST SYNC: plays with the FL Studio transport.  MIDI TRIGGER: plays while a key is held, key transposes (C5 = original).");
    addAndMakeVisible (swing);
    addAndMakeVisible (master);
    swing.labelColour = master.labelColour = C::bg;
    swingAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "swing", swing.slider);
    masterAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", master.slider);
    addAndMakeVisible (randAll);
    randAll.setColour (juce::TextButton::buttonColourId, C::orange);
    randAll.onClick = [this] { proc.randomizeAll(); grid.repaint(); };
    barsBox.onChange = [this] { grid.repaint(); };

    for (int s = 0; s < kNumSlots; ++s)
    {
        strips.push_back (std::make_unique<SlotStrip> (*this, proc, s));
        addAndMakeVisible (*strips.back());
    }

    // sample section
    addAndMakeVisible (wave);
    for (auto* b : { &loadBtn, &previewBtn, &clearBtn }) addAndMakeVisible (b);
    addAndMakeVisible (reverseBtn);
    addAndMakeVisible (tuneBtn);
    tuneBtn.setTooltip ("Transpose the sample from its root note to the selected key (turn off for drums)");
    rootBox.addItem ("ROOT: AUTO", 1);
    for (int i = 0; i < 12; ++i) rootBox.addItem ("ROOT: " + noteNames()[i], i + 2);
    addAndMakeVisible (rootBox);
    loadBtn.onClick = [this] { openLoadDialog (selectedSlot); };
    previewBtn.onClick = [this] { proc.preview (selectedSlot); };
    clearBtn.onClick = [this] { proc.clearSample (selectedSlot); refreshAll(); };
    rootBox.onChange = [this] { wave.repaint(); };
    reverseBtn.onClick = [this] { wave.repaint(); };

    addAndMakeVisible (grid);

    // generator
    styleBox.addItemList (styleNames(), 1);
    addAndMakeVisible (styleBox);
    for (auto* k : { &density, &variation, &melody }) addAndMakeVisible (k);
    melody.slider.setTooltip ("Adds notes from the selected scale (for chord stabs / melodic one-shots)");
    for (auto* b : { &randBtn, &undoBtn, &clearPatBtn }) addAndMakeVisible (b);
    randBtn.setColour (juce::TextButton::buttonColourId, C::orange);
    randBtn.onClick = [this] { proc.randomizeSlot (selectedSlot); grid.repaint(); };
    undoBtn.onClick = [this] { proc.undo (selectedSlot); grid.repaint(); };
    clearPatBtn.onClick = [this] { proc.clearSlot (selectedSlot); grid.repaint(); };
    styleBox.onChange = [this] { for (auto& s : strips) s->repaint(); };

    // sound
    for (auto* k : { &pitch, &fine, &start, &attack, &decay, &sustain, &release, &gate, &cutoff, &reso, &drive, &crush, &vol, &pan })
        addAndMakeVisible (k);
    ftypeBox.addItemList ({ "LP", "HP", "BP" }, 1);
    addAndMakeVisible (ftypeBox);
    start.slider.onValueChange = [this] { wave.repaint(); };

    for (auto& v : seenVersion) v = -1;
    setSize (1100, 744);
    selectSlot (0);
    startTimerHz (30);
}

FGenEditor::~FGenEditor()
{
    stopTimer();
    slotSliderAtts.clear();
    slotButtonAtts.clear();
    slotComboAtts.clear();
    setLookAndFeel (nullptr);
}

void FGenEditor::attachSlotControls()
{
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    slotSliderAtts.clear();
    slotButtonAtts.clear();
    slotComboAtts.clear();

    const int s = selectedSlot;
    auto sa = [&] (Knob& k, const char* id) { slotSliderAtts.push_back (std::make_unique<SA> (proc.apvts, FGenProcessor::slotParam (s, id), k.slider)); };
    sa (pitch, "pitch"); sa (fine, "fine"); sa (start, "start");
    sa (attack, "attack"); sa (decay, "decay"); sa (sustain, "sustain"); sa (release, "release"); sa (gate, "gate");
    sa (cutoff, "cutoff"); sa (reso, "reso"); sa (drive, "drive"); sa (crush, "crush");
    sa (vol, "vol"); sa (pan, "pan");
    sa (density, "density"); sa (variation, "variation"); sa (melody, "melody");

    slotButtonAtts.push_back (std::make_unique<BA> (proc.apvts, FGenProcessor::slotParam (s, "reverse"), reverseBtn));
    slotButtonAtts.push_back (std::make_unique<BA> (proc.apvts, FGenProcessor::slotParam (s, "tune"), tuneBtn));

    slotComboAtts.push_back (std::make_unique<CA> (proc.apvts, FGenProcessor::slotParam (s, "root"), rootBox));
    slotComboAtts.push_back (std::make_unique<CA> (proc.apvts, FGenProcessor::slotParam (s, "style"), styleBox));
    slotComboAtts.push_back (std::make_unique<CA> (proc.apvts, FGenProcessor::slotParam (s, "ftype"), ftypeBox));
}

void FGenEditor::selectSlot (int s)
{
    selectedSlot = juce::jlimit (0, kNumSlots - 1, s);
    for (int i = 0; i < kNumSlots; ++i)
    {
        strips[(size_t) i]->selected = (i == selectedSlot);
        strips[(size_t) i]->repaint();
    }
    attachSlotControls();
    wave.setSlot (selectedSlot);
    grid.setSlot (selectedSlot);
    repaint();
}

void FGenEditor::refreshAll()
{
    wave.rebuild();
    for (auto& st : strips) st->repaint();
    grid.repaint();
}

void FGenEditor::loadFileInto (int slot, const juce::File& f)
{
    if (proc.loadSample (slot, f))
        selectSlot (slot);
    refreshAll();
}

void FGenEditor::openLoadDialog (int slot)
{
    chooser = std::make_unique<juce::FileChooser> ("Load one-shot into slot " + juce::String::charToString ((juce::juce_wchar) ('A' + slot)),
                                                   juce::File(), proc.formatManager.getWildcardForAllFormats());
    juce::Component::SafePointer<FGenEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, slot] (const juce::FileChooser& fc) {
                              if (safe == nullptr) return;
                              auto f = fc.getResult();
                              if (f.existsAsFile()) safe->loadFileInto (slot, f);
                          });
}

void FGenEditor::timerCallback()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& strip = *strips[(size_t) s];
        const float act = proc.consumeActivity (s);
        float newLed = juce::jmax (strip.led * 0.82f, act);
        if (std::abs (newLed - strip.led) > 0.01f) { strip.led = newLed; strip.repaint(); }

        const int v = proc.getSampleVersion (s);
        if (v != seenVersion[(size_t) s])
        {
            seenVersion[(size_t) s] = v;
            if (s == selectedSlot) wave.rebuild();
            strip.repaint();
        }
    }
    grid.repaint();
}

//==============================================================================
void FGenEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    auto rf = r.toFloat();
    g.setColour (C::panel);
    g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (juce::Colour (0xff15130f));
    g.drawRoundedRectangle (rf, 6.0f, 1.0f);

    // screws
    g.setColour (juce::Colour (0xff4a4238));
    for (auto pt : { rf.getTopLeft() + juce::Point<float> (7, 7), rf.getTopRight() + juce::Point<float> (-7, 7),
                     rf.getBottomLeft() + juce::Point<float> (7, -7), rf.getBottomRight() + juce::Point<float> (-7, -7) })
    {
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pt));
    }

    if (title.isNotEmpty())
    {
        g.setColour (C::creamDim);
        g.setFont (monoFont (11.0f, true));
        g.drawText (title, r.reduced (16, 4).removeFromTop (14), juce::Justification::centredLeft);
    }
}

void FGenEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::bg);

    // header strip: cream panel like old hardware
    auto h = headerArea.toFloat();
    g.setColour (C::cream);
    g.fillRoundedRectangle (h, 6.0f);
    // stripes
    const juce::Colour stripes[] = { C::orange, C::red, juce::Colour (0xffe8b04a), C::teal };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (juce::Rectangle<float> (h.getX() + 190.0f + i * 9.0f, h.getY(), 6.0f, h.getHeight()));
    }
    g.setColour (C::bg);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 38.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("f_gen", juce::Rectangle<float> (h.getX() + 18.0f, h.getY(), 170.0f, h.getHeight() - 8.0f), juce::Justification::centredLeft);
    g.setFont (monoFont (10.0f, true));
    g.drawText ("RHYTHM SAMPLER", juce::Rectangle<float> (h.getX() + 20.0f, h.getBottom() - 20.0f, 170.0f, 14.0f), juce::Justification::centredLeft);

    // labels for global combos
    g.setFont (monoFont (10.0f, true));
    g.setColour (C::bg.withAlpha (0.75f));
    auto lab = [&] (juce::Component& c, const char* t) {
        g.drawText (t, c.getBounds().translated (0, -13).withHeight (12), juce::Justification::centredLeft);
    };
    lab (keyBox, "KEY"); lab (scaleBox, "SCALE"); lab (barsBox, "LOOP"); lab (modeBox, "PLAY");

    drawPanel (g, sampleArea, "");
    drawPanel (g, gridArea, "PATTERN  " + juce::String::charToString ((juce::juce_wchar) ('A' + selectedSlot))
                               + "   click = on/off   drag = paint   wheel = velocity   right-click = note / ratchet");
    drawPanel (g, genArea, "GENERATOR");
    drawPanel (g, soundArea, "SOUND " + juce::String::charToString ((juce::juce_wchar) ('A' + selectedSlot)));

    // sub-section captions in the sound panel
    g.setColour (C::creamDim.withAlpha (0.6f));
    g.setFont (monoFont (9.0f, true));
    auto cap = [&] (Knob& first, Knob& last, const char* t) {
        auto r = first.getBounds().getUnion (last.getBounds());
        g.drawHorizontalLine (r.getY() - 4, (float) r.getX() + 4.0f, (float) r.getRight() - 4.0f);
        g.drawText (t, r.getX(), r.getY() - 16, r.getWidth(), 11, juce::Justification::centred);
    };
    cap (pitch, start, "PITCH");
    cap (attack, gate, "ENVELOPE");
    cap (cutoff, crush, "COLOUR");
    cap (vol, pan, "OUT");
}

void FGenEditor::resized()
{
    auto r = getLocalBounds().reduced (12);

    headerArea = r.removeFromTop (64);
    {
        auto h = headerArea.reduced (14, 0).withTrimmedLeft (240);
        auto row = h.withSizeKeepingCentre (h.getWidth(), 28).translated (0, 6);
        keyBox.setBounds (row.removeFromLeft (70)); row.removeFromLeft (10);
        scaleBox.setBounds (row.removeFromLeft (130)); row.removeFromLeft (10);
        barsBox.setBounds (row.removeFromLeft (100)); row.removeFromLeft (10);
        modeBox.setBounds (row.removeFromLeft (140)); row.removeFromLeft (14);
        randAll.setBounds (headerArea.reduced (12).removeFromRight (160));
        auto knobs = headerArea.withTrimmedLeft (row.getX() - headerArea.getX()).withTrimmedRight (180);
        swing.setBounds (knobs.removeFromLeft (64).reduced (0, 2));
        master.setBounds (knobs.removeFromLeft (64).reduced (0, 2));
    }
    r.removeFromTop (12);

    auto left = r.removeFromLeft (232);
    const int stripH = (left.getHeight() - 3 * 8) / 4;
    for (int s = 0; s < kNumSlots; ++s)
    {
        strips[(size_t) s]->setBounds (left.removeFromTop (stripH));
        left.removeFromTop (8);
    }
    r.removeFromLeft (12);

    sampleArea = r.removeFromTop (128);
    {
        auto a = sampleArea.reduced (10);
        auto ctrls = a.removeFromRight (230);
        wave.setBounds (a.withTrimmedRight (10));
        auto row1 = ctrls.removeFromTop (30);
        loadBtn.setBounds (row1.removeFromLeft (74)); row1.removeFromLeft (4);
        previewBtn.setBounds (row1.removeFromLeft (84)); row1.removeFromLeft (4);
        clearBtn.setBounds (row1);
        ctrls.removeFromTop (8);
        auto row2 = ctrls.removeFromTop (26);
        tuneBtn.setBounds (row2.removeFromLeft (110));
        reverseBtn.setBounds (row2);
        ctrls.removeFromTop (8);
        rootBox.setBounds (ctrls.removeFromTop (28));
    }
    r.removeFromTop (10);

    soundArea = r.removeFromBottom (112);
    r.removeFromBottom (10);
    genArea = r.removeFromBottom (92);
    r.removeFromBottom (10);
    gridArea = r;
    grid.setBounds (gridArea.reduced (14).withTrimmedTop (14));

    {
        auto g = genArea.reduced (14, 8).withTrimmedTop (14);
        styleBox.setBounds (g.removeFromLeft (180).withSizeKeepingCentre (180, 30));
        g.removeFromLeft (16);
        for (auto* k : { &density, &variation, &melody })
            k->setBounds (g.removeFromLeft (76));
        g.removeFromLeft (16);
        randBtn.setBounds (g.removeFromLeft (170).withSizeKeepingCentre (170, 44));
        g.removeFromLeft (10);
        undoBtn.setBounds (g.removeFromLeft (80).withSizeKeepingCentre (80, 32));
        g.removeFromLeft (8);
        clearPatBtn.setBounds (g.removeFromLeft (80).withSizeKeepingCentre (80, 32));
    }

    {
        auto s = soundArea.reduced (12, 8).withTrimmedTop (24);
        const int kw = 52;
        auto place = [&] (Knob& k) { k.setBounds (s.removeFromLeft (kw)); };
        place (pitch); place (fine); place (start);
        s.removeFromLeft (10);
        place (attack); place (decay); place (sustain); place (release); place (gate);
        s.removeFromLeft (10);
        place (cutoff); place (reso);
        ftypeBox.setBounds (s.removeFromLeft (46).withSizeKeepingCentre (46, 24).translated (0, -6));
        place (drive); place (crush);
        s.removeFromLeft (10);
        place (vol); place (pan);
    }
}
