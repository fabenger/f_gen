#pragma once

#include <JuceHeader.h>
#include "Pattern.h"
#include "RootDetector.h"

namespace fgen
{
struct SampleData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;
    juce::AudioBuffer<float> buffer;   // always 2 channels
    double sampleRate = 44100.0;
    juce::String path, name;
    RootResult root;
};

struct Voice
{
    bool active = false;
    double pos = 0.0, inc = 1.0;
    float gain = 1.0f;
    int gateLeft = 0;
    bool reverse = false;
    int age = 0;
    juce::ADSR adsr;
};

struct SlotDsp
{
    static constexpr int kVoices = 8;
    std::array<Voice, kVoices> voices;
    juce::dsp::StateVariableTPTFilter<float> filter;
    float crushHoldL = 0.0f, crushHoldR = 0.0f;
    float crushCounter = 0.0f;
    juce::SmoothedValue<float> gainL, gainR;
    int sampleVersionSeen = -1;
};
} // namespace fgen

//==============================================================================
class FGenProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    FGenProcessor();
    ~FGenProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "f_gen"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::String slotParam (int slot, const char* name) { return "s" + juce::String (slot) + "_" + name; }

    bool loadSample (int slot, const juce::File& file, bool autoTune = true);
    void clearSample (int slot);
    fgen::SampleData::Ptr getSample (int slot) const;
    int getSampleVersion (int slot) const { return sampleVersion[(size_t) slot].load(); }

    void randomizeSlot (int slot);
    void randomizeAll();
    void clearSlot (int slot);
    bool undo (int slot);
    void pushUndo (int slot);

    fgen::Step getStep (int slot, int idx) const   { return patterns[(size_t) slot].get (idx); }
    void setStep (int slot, int idx, const fgen::Step& s) { patterns[(size_t) slot].set (idx, s); }

    void preview (int slot) { previewRequest[(size_t) slot].store (true); }

    int getBars() const;
    int getPlayStep() const       { return playStep.load(); }
    bool isClockRunning() const   { return clockRunning.load(); }
    float consumeActivity (int slot) { return slotActivity[(size_t) slot].exchange (0.0f); }
    int getEffectiveRoot (int slot) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;

    struct Event { int offset; int slot; int step; int ratchet; int ratchetCount; bool isPreview; };
    void collectEvents (double ppqA, double ppqB, double samplesPerPpq, int offsetBase, int numSamplesInSeg,
                        std::vector<Event>& out);
    void triggerVoice (int slot, const fgen::SampleData& sd, const Event& e, double samplesPerStep);
    int getEffectiveRootFast (int slot, const fgen::SampleData& sd) const;
    void renderSlot (int slot, const fgen::SampleData* sd, float* outL, float* outR, int start, int num);

    std::array<fgen::AtomicPattern, fgen::kNumSlots> patterns;
    std::array<std::vector<fgen::PatternData>, fgen::kNumSlots> undoStack;

    std::array<fgen::SampleData::Ptr, fgen::kNumSlots> samples;
    juce::ReferenceCountedArray<fgen::SampleData> releasePool;
    juce::SpinLock sampleLock;
    std::array<std::atomic<int>, fgen::kNumSlots> sampleVersion {};

    std::array<fgen::SlotDsp, fgen::kNumSlots> dsp;
    std::array<std::atomic<bool>, fgen::kNumSlots> previewRequest {};
    std::array<std::atomic<float>, fgen::kNumSlots> slotActivity {};

    std::atomic<int> playStep { -1 };
    std::atomic<bool> clockRunning { false };

    // MIDI-trigger mode clock
    double internalPpq = 0.0;
    int heldNotes = 0;
    int lastNote = 60;

    double currentSampleRate = 44100.0;
    std::vector<Event> events;
    juce::AudioBuffer<float> slotBuffer;
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FGenProcessor)
};
