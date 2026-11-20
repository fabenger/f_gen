#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

class FGenEditor;

//==============================================================================
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& labelText);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    juce::Colour labelColour { fgen::Colours::ink };
private:
    juce::String text;
};

//==============================================================================
class SlotStrip : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    SlotStrip (FGenEditor& e, FGenProcessor& p, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

    bool selected = false;
    float led = 0.0f;
private:
    FGenEditor& editor;
    FGenProcessor& proc;
    int slot;
    bool dragOver = false;
    juce::ToggleButton mute { "MUTE" }, lock { "LOCK" };
    juce::TextButton rand { "RND" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt, lockAtt;
};

//==============================================================================
class WaveDisplay : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    WaveDisplay (FGenEditor& e, FGenProcessor& p) : editor (e), proc (p) {}
    void paint (juce::Graphics&) override;
    void setSlot (int s) { slot = s; rebuild(); }
    void rebuild();
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
private:
    FGenEditor& editor;
    FGenProcessor& proc;
    int slot = 0;
    std::vector<float> peaks;
};

//==============================================================================
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (FGenProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setSlot (int s) { slot = s; repaint(); }
private:
    int stepAt (juce::Point<int> p) const;
    juce::Rectangle<float> cellBounds (int step) const;
    void showStepMenu (int step);
    FGenProcessor& proc;
    int slot = 0;
    bool paintOn = true;
    int lastPainted = -1;
};

//==============================================================================
class FGenEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FGenEditor (FGenProcessor&);
    ~FGenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void selectSlot (int s);
    int getSelectedSlot() const { return selectedSlot; }
    void loadFileInto (int slot, const juce::File& f);
    void openLoadDialog (int slot);
    void refreshAll();

private:
    void timerCallback() override;
    void attachSlotControls();
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    FGenProcessor& proc;
    fgen::FGenLookAndFeel lnf;
    int selectedSlot = 0;

    // global
    juce::ComboBox keyBox, scaleBox, barsBox, modeBox;
    Knob swing { "SWING" }, master { "MASTER" };
    juce::TextButton randAll { "RANDOMIZE ALL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, barsAtt, modeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAtt, masterAtt;

    std::vector<std::unique_ptr<SlotStrip>> strips;

    // sample section
    WaveDisplay wave;
    juce::TextButton loadBtn { "LOAD" }, previewBtn { "PREVIEW" }, clearBtn { "EJECT" };
    juce::ToggleButton reverseBtn { "REVERSE" }, tuneBtn { "KEY TUNE" };
    juce::ComboBox rootBox;

    // grid
    StepGrid grid;

    // generator
    juce::ComboBox styleBox;
    Knob density { "DENSITY" }, variation { "VARIATION" }, melody { "MELODY" };
    juce::TextButton randBtn { "RANDOMIZE" }, undoBtn { "UNDO" }, clearPatBtn { "CLEAR" };

    // sound
    Knob pitch { "PITCH" }, fine { "FINE" }, start { "START" };
    Knob attack { "ATTACK" }, decay { "DECAY" }, sustain { "SUSTAIN" }, release { "RELEASE" }, gate { "GATE" };
    Knob cutoff { "CUTOFF" }, reso { "RESO" }, drive { "DRIVE" }, crush { "CRUSH" };
    Knob vol { "VOLUME" }, pan { "PAN" };
    juce::ComboBox ftypeBox, outBox;
    juce::Image avatar;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> slotSliderAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> slotButtonAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> slotComboAtts;

    std::array<int, fgen::kNumSlots> seenVersion {};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    // layout rects for painting
    juce::Rectangle<int> sampleArea, gridArea, genArea, soundArea, headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FGenEditor)
};
