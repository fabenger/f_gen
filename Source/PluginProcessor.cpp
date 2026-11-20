#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace fgen;

namespace
{
juce::StringArray rootChoices()
{
    juce::StringArray a { "Auto" };
    a.addArray (noteNames());
    return a;
}

float dbToGain (float db) { return db <= -59.0f ? 0.0f : juce::Decibels::decibelsToGain (db); }
} // namespace

//==============================================================================
FGenProcessor::FGenProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Slot A", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Slot B", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Slot C", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Slot D", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    for (int s = 0; s < kNumSlots; ++s)
    {
        sampleVersion[(size_t) s].store (0);
        previewRequest[(size_t) s].store (false);
        slotActivity[(size_t) s].store (0.0f);
    }

    // start every slot with a groove so a freshly loaded sample plays right away
    for (int s = 0; s < kNumSlots; ++s)
        randomizeSlot (s);
    for (auto& u : undoStack) u.clear();

    startTimer (1000);
}

FGenProcessor::~FGenProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout FGenProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; });
    auto secs = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return v < 1.0f ? String (juce::roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", noteNames(), 9)); // A
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "bars", 1 }, "Bars", StringArray { "2", "4", "8" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (50.0f, 75.0f, 0.1f), 54.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Host Sync", "MIDI Trigger" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -3.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    static const int defaultStyles[] = { (int) Style::House, (int) Style::Deep, (int) Style::Minimal, (int) Style::Euclid };

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto id = [s] (const char* n) { return ParameterID { slotParam (s, n), 1 }; };
        const String p = String::charToString ((juce_wchar) ('A' + s)) + " ";

        layout.add (std::make_unique<AudioParameterInt>   (id ("pitch"), p + "Pitch", -24, 24, 0));
        layout.add (std::make_unique<AudioParameterFloat> (id ("fine"), p + "Fine", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                                           AudioParameterFloatAttributes().withLabel ("ct")));
        layout.add (std::make_unique<AudioParameterFloat> (id ("start"), p + "Start", NormalisableRange<float> (0.0f, 0.95f, 0.001f), 0.0f, pct));
        layout.add (std::make_unique<AudioParameterBool>  (id ("reverse"), p + "Reverse", false));

        layout.add (std::make_unique<AudioParameterFloat> (id ("attack"), p + "Attack", NormalisableRange<float> (0.0005f, 2.0f, 0.0001f, 0.3f), 0.002f, secs));
        layout.add (std::make_unique<AudioParameterFloat> (id ("decay"), p + "Decay", NormalisableRange<float> (0.01f, 3.0f, 0.001f, 0.35f), 0.4f, secs));
        layout.add (std::make_unique<AudioParameterFloat> (id ("sustain"), p + "Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (id ("release"), p + "Release", NormalisableRange<float> (0.005f, 4.0f, 0.001f, 0.35f), 0.15f, secs));
        layout.add (std::make_unique<AudioParameterFloat> (id ("gate"), p + "Gate", NormalisableRange<float> (0.1f, 8.0f, 0.01f, 0.5f), 1.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " st"; })));

        layout.add (std::make_unique<AudioParameterFloat> (id ("cutoff"), p + "Cutoff", NormalisableRange<float> (30.0f, 20000.0f, 1.0f, 0.25f), 20000.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
        layout.add (std::make_unique<AudioParameterFloat> (id ("reso"), p + "Resonance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.1f, pct));
        layout.add (std::make_unique<AudioParameterChoice>(id ("ftype"), p + "Filter Type", StringArray { "LP", "HP", "BP" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (id ("drive"), p + "Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (id ("crush"), p + "Crush", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (id ("vol"), p + "Volume", NormalisableRange<float> (-60.0f, 6.0f, 0.1f, 2.0f), -6.0f,
                                                           AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<AudioParameterFloat> (id ("pan"), p + "Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<AudioParameterBool>  (id ("tune"), p + "Key Tune", true));
        layout.add (std::make_unique<AudioParameterChoice>(id ("root"), p + "Root", rootChoices(), 0));

        layout.add (std::make_unique<AudioParameterChoice>(id ("style"), p + "Style", styleNames(), defaultStyles[s]));
        layout.add (std::make_unique<AudioParameterFloat> (id ("density"), p + "Density", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (id ("variation"), p + "Variation", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, pct));
        layout.add (std::make_unique<AudioParameterFloat> (id ("melody"), p + "Melody", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));

        layout.add (std::make_unique<AudioParameterChoice>(id ("out"), p + "Output", StringArray { "Main", "Own Out" }, 0));
        layout.add (std::make_unique<AudioParameterBool>  (id ("mute"), p + "Mute", false));
        layout.add (std::make_unique<AudioParameterBool>  (id ("lock"), p + "Lock", false));
    }

    return layout;
}

bool FGenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& aux = layouts.outputBuses.getReference (i);
        if (! aux.isDisabled() && aux != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void FGenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    slotBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
    events.reserve (512);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    for (auto& d : dsp)
    {
        d.filter.prepare (spec);
        d.filter.reset();
        for (auto& v : d.voices)
        {
            v.adsr.setSampleRate (sampleRate);
            v.active = false;
        }
        d.gainL.reset (sampleRate, 0.02);
        d.gainR.reset (sampleRate, 0.02);
    }
    internalPpq = 0.0;
    heldNotes = 0;
}

int FGenProcessor::getBars() const
{
    const int idx = (int) apvts.getRawParameterValue ("bars")->load();
    return idx == 0 ? 2 : (idx == 1 ? 4 : 8);
}

int FGenProcessor::getEffectiveRoot (int slot) const
{
    const int rootChoice = (int) apvts.getRawParameterValue (slotParam (slot, "root"))->load();
    if (rootChoice > 0) return rootChoice - 1;
    auto s = getSample (slot);
    return s != nullptr ? s->root.rootPc : 0;
}

//==============================================================================
void FGenProcessor::collectEvents (double a, double b, double samplesPerPpq, int offsetBase, int segLen, std::vector<Event>& out)
{
    const int totalSteps = getBars() * kStepsPerBar;
    const double stepLen = 0.25;
    const double swingOff = (apvts.getRawParameterValue ("swing")->load() / 100.0 - 0.5) * 0.5;

    const int nFirst = juce::jmax (0, (int) std::floor (a / stepLen) - 1);
    const int nLast  = (int) std::floor (b / stepLen) + 1;

    if (b > 0.0)
        playStep.store (((int) std::floor (juce::jmax (0.0, a) / stepLen)) % totalSteps);

    for (int n = nFirst; n <= nLast; ++n)
    {
        const double t0 = n * stepLen + ((n & 1) ? swingOff : 0.0);
        const int idx = n % totalSteps;

        for (int s = 0; s < kNumSlots; ++s)
        {
            if (apvts.getRawParameterValue (slotParam (s, "mute"))->load() > 0.5f) continue;
            const Step st = patterns[(size_t) s].get (idx);
            if (! st.on) continue;

            for (int r = 0; r < st.ratchet; ++r)
            {
                const double t = t0 + r * (stepLen / st.ratchet);
                if (t >= a && t < b)
                {
                    const int off = juce::jlimit (offsetBase, offsetBase + segLen - 1, offsetBase + (int) ((t - a) * samplesPerPpq));
                    out.push_back ({ off, s, idx, r, st.ratchet, false });
                }
            }
        }
    }
}

void FGenProcessor::triggerVoice (int slot, const SampleData& sd, const Event& e, double samplesPerStep)
{
    auto& d = dsp[(size_t) slot];
    auto param = [this, slot] (const char* n) { return apvts.getRawParameterValue (slotParam (slot, n))->load(); };

    // pick voice
    Voice* v = nullptr;
    for (auto& cand : d.voices) if (! cand.active) { v = &cand; break; }
    if (v == nullptr)
    {
        v = &d.voices[0];
        for (auto& cand : d.voices) if (cand.age > v->age) v = &cand;
    }

    Step st;
    if (! e.isPreview) st = patterns[(size_t) slot].get (e.step);

    double semis = param ("pitch") + param ("fine") / 100.0;
    if (param ("tune") > 0.5f)
    {
        const int key = (int) apvts.getRawParameterValue ("key")->load();
        const int scale = (int) apvts.getRawParameterValue ("scale")->load();
        int t = ((key - getEffectiveRootFast (slot, sd)) % 12 + 12) % 12;
        if (t > 6) t -= 12;
        semis += t + degreeToSemitones (st.degree, scale);
    }
    if ((int) apvts.getRawParameterValue ("mode")->load() == 1)
        semis += lastNote - 60;

    const int len = sd.buffer.getNumSamples();
    v->active = true;
    v->reverse = param ("reverse") > 0.5f;
    v->pos = param ("start") * (len - 1);
    v->inc = std::pow (2.0, semis / 12.0) * sd.sampleRate / currentSampleRate;
    v->gain = e.isPreview ? 0.9f : juce::jlimit (0.0f, 1.0f, st.vel / 127.0f);
    v->age = 0;

    const double gateSteps = e.isPreview ? 2.0 : param ("gate") / (double) juce::jmax (1, e.ratchetCount);
    v->gateLeft = juce::jmax (1, (int) (gateSteps * samplesPerStep));

    juce::ADSR::Parameters ap;
    ap.attack = param ("attack");
    ap.decay = param ("decay");
    ap.sustain = param ("sustain");
    ap.release = param ("release");
    v->adsr.setParameters (ap);
    v->adsr.reset();
    v->adsr.noteOn();

    for (auto& other : d.voices) if (&other != v) ++other.age;
    slotActivity[(size_t) slot].store (v->gain);
}

static inline float cubicRead (const float* d, int len, double pos) noexcept
{
    const int i1 = (int) pos;
    const float f = (float) (pos - i1);
    const int i0 = juce::jmax (0, i1 - 1), i2 = juce::jmin (len - 1, i1 + 1), i3 = juce::jmin (len - 1, i1 + 2);
    const float y0 = d[i0], y1 = d[juce::jlimit (0, len - 1, i1)], y2 = d[i2], y3 = d[i3];
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void FGenProcessor::renderSlot (int slot, const SampleData* sd, float* outL, float* outR, int start, int num)
{
    if (sd == nullptr || num <= 0) return;
    const int len = sd->buffer.getNumSamples();
    const float* srcL = sd->buffer.getReadPointer (0);
    const float* srcR = sd->buffer.getReadPointer (1);

    for (auto& v : dsp[(size_t) slot].voices)
    {
        if (! v.active) continue;
        for (int i = start; i < start + num; ++i)
        {
            if (v.pos >= len - 1 || ! v.adsr.isActive()) { v.active = false; break; }
            const double rp = v.reverse ? (len - 1) - v.pos : v.pos;
            const float env = v.adsr.getNextSample() * v.gain;
            outL[i] += cubicRead (srcL, len, rp) * env;
            outR[i] += cubicRead (srcR, len, rp) * env;
            v.pos += v.inc;
            if (v.gateLeft > 0 && --v.gateLeft == 0) v.adsr.noteOff();
        }
    }
}

// cheap root lookup on the audio thread (no ref-counting)
int FGenProcessor::getEffectiveRootFast (int slot, const SampleData& sd) const
{
    const int rootChoice = (int) apvts.getRawParameterValue (slotParam (slot, "root"))->load();
    return rootChoice > 0 ? rootChoice - 1 : sd.root.rootPc;
}

void FGenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    if (numSamples == 0) return;

    if (slotBuffer.getNumSamples() < numSamples)
        slotBuffer.setSize (2, numSamples, false, false, true);

    double bpm = 120.0, ppq = 0.0;
    bool hasPpq = false, playing = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPpq = true; }
            playing = pos->getIsPlaying();
        }
    }
    const double samplesPerPpq = currentSampleRate * 60.0 / bpm;
    const double samplesPerStep = samplesPerPpq * 0.25;

    events.clear();
    const int mode = (int) apvts.getRawParameterValue ("mode")->load();

    if (mode == 0)
    {
        const bool running = playing && hasPpq;
        clockRunning.store (running);
        if (running)
            collectEvents (ppq, ppq + numSamples / samplesPerPpq, samplesPerPpq, 0, numSamples, events);
        else
            playStep.store (-1);
    }
    else
    {
        int segStart = 0;
        auto runSegment = [&] (int segEnd)
        {
            const int segLen = segEnd - segStart;
            if (segLen <= 0) return;
            if (heldNotes > 0)
            {
                collectEvents (internalPpq, internalPpq + segLen / samplesPerPpq, samplesPerPpq, segStart, segLen, events);
                internalPpq += segLen / samplesPerPpq;
            }
        };

        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const int pos = juce::jlimit (0, numSamples, meta.samplePosition);
            runSegment (pos);
            segStart = pos;

            if (msg.isNoteOn())
            {
                if (heldNotes == 0) internalPpq = 0.0;
                ++heldNotes;
                lastNote = msg.getNoteNumber();
            }
            else if (msg.isNoteOff())
            {
                heldNotes = juce::jmax (0, heldNotes - 1);
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                heldNotes = 0;
            }
        }
        runSegment (numSamples);
        clockRunning.store (heldNotes > 0);
        if (heldNotes == 0) playStep.store (-1);
    }

    for (int s = 0; s < kNumSlots; ++s)
        if (previewRequest[(size_t) s].exchange (false))
            events.push_back ({ 0, s, 0, 0, 1, true });

    std::stable_sort (events.begin(), events.end(), [] (const Event& x, const Event& y) { return x.offset < y.offset; });

    std::array<SampleData::Ptr, kNumSlots> local;
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        for (int s = 0; s < kNumSlots; ++s) local[(size_t) s] = samples[(size_t) s];
    }

    auto mainBus = getBusBuffer (buffer, false, 0);
    const float master = dbToGain (apvts.getRawParameterValue ("master")->load());

    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& d = dsp[(size_t) s];
        const auto* sd = local[(size_t) s].get();

        const int ver = sampleVersion[(size_t) s].load();
        if (d.sampleVersionSeen != ver)
        {
            for (auto& v : d.voices) v.active = false;
            d.sampleVersionSeen = ver;
        }
        if (sd == nullptr) continue;

        slotBuffer.clear();
        auto* bl = slotBuffer.getWritePointer (0);
        auto* br = slotBuffer.getWritePointer (1);

        int cursor = 0;
        for (const auto& e : events)
        {
            if (e.slot != s) continue;
            renderSlot (s, sd, bl, br, cursor, e.offset - cursor);
            cursor = e.offset;
            triggerVoice (s, *sd, e, samplesPerStep);
        }
        renderSlot (s, sd, bl, br, cursor, numSamples - cursor);

        bool anyActive = false;
        for (auto& v : d.voices) anyActive |= v.active;

        auto param = [this, s] (const char* n) { return apvts.getRawParameterValue (slotParam (s, n))->load(); };

        // filter
        const int ftype = (int) param ("ftype");
        const float cutoff = juce::jmin (param ("cutoff"), (float) currentSampleRate * 0.45f);
        const float reso = param ("reso");
        const bool filterNeutral = ftype == 0 && cutoff >= 19000.0f && reso < 0.15f;
        if (! filterNeutral)
        {
            d.filter.setType (ftype == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                                         : (ftype == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                                                       : juce::dsp::StateVariableTPTFilterType::bandpass));
            d.filter.setCutoffFrequency (cutoff);
            d.filter.setResonance (0.707f + reso * reso * 9.0f);
            juce::dsp::AudioBlock<float> block (slotBuffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
            d.filter.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // saturation
        const float drive = param ("drive");
        if (drive > 0.001f)
        {
            const float g = 1.0f + drive * 24.0f;
            const float norm = 1.0f / (1.0f + 0.4f * (g - 1.0f));
            for (int i = 0; i < numSamples; ++i)
            {
                // slight asymmetry for a warmer, tape/tube-ish colour
                bl[i] = (std::tanh (g * bl[i] + 0.1f * drive) - std::tanh (0.1f * drive)) * norm;
                br[i] = (std::tanh (g * br[i] + 0.1f * drive) - std::tanh (0.1f * drive)) * norm;
            }
        }

        // bit crush + sample-rate reduction
        const float crush = param ("crush");
        if (crush > 0.001f)
        {
            const float bits = 16.0f - crush * 12.0f;
            const float q = std::pow (2.0f, bits - 1.0f);
            const float hold = 1.0f + crush * crush * 15.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                d.crushCounter += 1.0f;
                if (d.crushCounter >= hold)
                {
                    d.crushCounter -= hold;
                    d.crushHoldL = std::round (bl[i] * q) / q;
                    d.crushHoldR = std::round (br[i] * q) / q;
                }
                bl[i] = d.crushHoldL;
                br[i] = d.crushHoldR;
            }
        }

        // volume + pan (equal power)
        const float vol = dbToGain (param ("vol")) * master;
        const float pan = param ("pan");
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        d.gainL.setTargetValue (vol * std::cos (angle) * juce::MathConstants<float>::sqrt2);
        d.gainR.setTargetValue (vol * std::sin (angle) * juce::MathConstants<float>::sqrt2);

        // route: own output bus if selected and available, otherwise main
        float* outL = mainBus.getWritePointer (0);
        float* outR = mainBus.getNumChannels() > 1 ? mainBus.getWritePointer (1) : nullptr;
        if ((int) param ("out") == 1 && s + 1 < getBusCount (false))
        {
            if (auto* bus = getBus (false, s + 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() >= 2)
            {
                auto aux = getBusBuffer (buffer, false, s + 1);
                outL = aux.getWritePointer (0);
                outR = aux.getWritePointer (1);
            }
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float gl = d.gainL.getNextValue(), gr = d.gainR.getNextValue();
            if (outR != nullptr)
            {
                outL[i] += bl[i] * gl;
                outR[i] += br[i] * gr;
            }
            else
            {
                outL[i] += 0.5f * (bl[i] * gl + br[i] * gr);
            }
        }
        juce::ignoreUnused (anyActive);
    }

    midi.clear();
}

//==============================================================================
bool FGenProcessor::loadSample (int slot, const juce::File& file, bool autoTune)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 30.0);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len <= 0) return false;

    SampleData::Ptr sd = new SampleData();
    sd->buffer.setSize (2, len);
    reader->read (&sd->buffer, 0, len, 0, true, true);
    if (reader->numChannels == 1)
        sd->buffer.copyFrom (1, 0, sd->buffer, 0, 0, len);
    sd->sampleRate = reader->sampleRate;
    sd->path = file.getFullPathName();
    sd->name = file.getFileNameWithoutExtension();
    sd->root = RootDetector::detect (sd->buffer, sd->sampleRate);

    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        if (samples[(size_t) slot] != nullptr) releasePool.add (samples[(size_t) slot]);
        samples[(size_t) slot] = sd;
    }
    sampleVersion[(size_t) slot].fetch_add (1);

    if (autoTune)
        if (auto* p = apvts.getParameter (slotParam (slot, "tune")))
            p->setValueNotifyingHost (sd->root.pitched ? 1.0f : 0.0f);

    return true;
}

SampleData::Ptr FGenProcessor::getSample (int slot) const
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return samples[(size_t) slot];
}

void FGenProcessor::clearSample (int slot)
{
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        if (samples[(size_t) slot] != nullptr) releasePool.add (samples[(size_t) slot]);
        samples[(size_t) slot] = nullptr;
    }
    sampleVersion[(size_t) slot].fetch_add (1);
}

void FGenProcessor::timerCallback()
{
    for (int i = releasePool.size(); --i >= 0;)
        if (releasePool.getObjectPointerUnchecked (i)->getReferenceCount() == 1)
            releasePool.remove (i);
}

//==============================================================================
void FGenProcessor::pushUndo (int slot)
{
    auto& u = undoStack[(size_t) slot];
    u.push_back (patterns[(size_t) slot].read());
    if (u.size() > 32) u.erase (u.begin());
}

bool FGenProcessor::undo (int slot)
{
    auto& u = undoStack[(size_t) slot];
    if (u.empty()) return false;
    patterns[(size_t) slot].write (u.back());
    u.pop_back();
    return true;
}

void FGenProcessor::randomizeSlot (int slot)
{
    auto param = [this, slot] (const char* n) { return apvts.getRawParameterValue (slotParam (slot, n))->load(); };

    GenSettings g;
    g.style = (Style) (int) param ("style");
    g.density = param ("density");
    g.variation = param ("variation");
    g.melody = param ("melody");
    g.bars = getBars();
    auto sd = getSample (slot);
    g.percussive = param ("tune") < 0.5f || (sd != nullptr && ! sd->root.pitched);

    pushUndo (slot);
    patterns[(size_t) slot].write (Generator::generate (g, rng));
}

void FGenProcessor::randomizeAll()
{
    for (int s = 0; s < kNumSlots; ++s)
        if (apvts.getRawParameterValue (slotParam (s, "lock"))->load() < 0.5f)
            randomizeSlot (s);
}

void FGenProcessor::clearSlot (int slot)
{
    pushUndo (slot);
    patterns[(size_t) slot].write (PatternData {});
}

//==============================================================================
void FGenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* slotsXml = xml->createNewChildElement ("FGEN_SLOTS");
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto* e = slotsXml->createNewChildElement ("SLOT");
        e->setAttribute ("index", s);
        auto sd = getSample (s);
        e->setAttribute ("path", sd != nullptr ? sd->path : juce::String());
        e->setAttribute ("pattern", patterns[(size_t) s].toString());
    }
    copyXmlToBinary (*xml, destData);
}

void FGenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr) return;

    std::unique_ptr<juce::XmlElement> slotsXml;
    if (auto* sx = xml->getChildByName ("FGEN_SLOTS"))
    {
        slotsXml = std::make_unique<juce::XmlElement> (*sx);
        xml->removeChildElement (sx, true);
    }

    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (slotsXml != nullptr)
    {
        for (auto* e : slotsXml->getChildWithTagNameIterator ("SLOT"))
        {
            const int s = e->getIntAttribute ("index", -1);
            if (s < 0 || s >= kNumSlots) continue;
            patterns[(size_t) s].fromString (e->getStringAttribute ("pattern"));
            const juce::File f (e->getStringAttribute ("path"));
            if (f.existsAsFile()) loadSample (s, f, false);
            else clearSample (s);
        }
    }
}

juce::AudioProcessorEditor* FGenProcessor::createEditor() { return new FGenEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FGenProcessor(); }
