// Offline test tool: root detection on synthetic one-shots, render a loop, snapshot the UI.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 124.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setIsPlaying (true);
        return p;
    }
};

static juce::File writeWav (const juce::File& dir, const juce::String& name, const std::function<float (double)>& fn, double secs)
{
    const double sr = 44100.0;
    juce::AudioBuffer<float> b (1, (int) (sr * secs));
    for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, fn (i / sr));
    auto f = dir.getChildFile (name + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 1, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    return f;
}

static float saw (double f, double t) { double ph = f * t; return (float) (2.0 * (ph - std::floor (ph + 0.5))); }
static double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto dir = juce::File (argc > 1 ? argv[1] : "/tmp/fgen_test");
    dir.createDirectory();
    juce::Random rnd (1);

    struct Case { juce::String name; std::function<float (double)> fn; juce::String expect; };
    std::vector<Case> cases {
        { "stab_Am", [] (double t) { double e = std::exp (-t * 3.0); return (float) (e * 0.25 * (saw (mtof (57), t) + saw (mtof (60), t) + saw (mtof (64), t))); }, "A" },
        { "stab_Fmaj7", [] (double t) { double e = std::exp (-t * 3.0); return (float) (e * 0.2 * (saw (mtof (53), t) + saw (mtof (57), t) + saw (mtof (60), t) + saw (mtof (64), t))); }, "F" },
        { "stab_Dm7", [] (double t) { double e = std::exp (-t * 4.0); return (float) (e * 0.2 * (saw (mtof (50), t) + saw (mtof (53), t) + saw (mtof (57), t) + saw (mtof (60), t))); }, "D" },
        { "note_C", [] (double t) { double e = std::exp (-t * 2.0); return (float) (e * 0.6 * std::sin (2 * juce::MathConstants<double>::pi * mtof (48) * t)); }, "C" },
        { "bass_G", [] (double t) { double e = std::exp (-t * 2.0); return (float) (e * 0.5 * saw (mtof (43), t)); }, "G" },
        { "hat_noise", [&rnd] (double t) { return (float) (std::exp (-t * 40.0) * (rnd.nextFloat() * 2 - 1) * 0.5); }, "perc" },
    };

    int fails = 0;
    std::vector<juce::File> files;
    for (auto& c : cases)
    {
        auto f = writeWav (dir, c.name, c.fn, 1.0);
        files.push_back (f);
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
        r->read (&b, 0, b.getNumSamples(), 0, true, true);
        auto res = fgen::RootDetector::detect (b, r->sampleRate);
        const bool ok = c.expect == "perc" ? ! res.pitched : (res.pitched && fgen::noteNames()[res.rootPc] == c.expect);
        if (! ok) ++fails;
        std::cout << (ok ? "OK   " : "FAIL ") << c.name << " -> " << res.description << " (expect " << c.expect << ")\n";
    }

    // pattern generator sanity
    juce::Random rng (42);
    for (int st = 0; st < 6; ++st)
    {
        fgen::GenSettings g; g.style = (fgen::Style) st; g.bars = 4; g.melody = 0.5f;
        auto p = fgen::Generator::generate (g, rng);
        juce::String line;
        for (int i = 0; i < 32; ++i) line << (p[(size_t) i].on ? (p[(size_t) i].degree != 0 ? "o" : "x") : ".");
        std::cout << juce::String (fgen::styleNames()[st]).paddedRight (' ', 16) << line << "\n";
    }

    // render 4 bars
    FGenProcessor proc;
    proc.loadSample (0, files[0]);
    proc.loadSample (1, files[5]);
    proc.loadSample (2, files[3]);
    proc.loadSample (3, files[4]);
    const double sr = 44100.0; const int bs = 512;
    proc.setRateAndBufferSizeDetails (sr, bs);
    proc.apvts.getParameter ("s1_out")->setValueNotifyingHost (1.0f); // slot B -> own out
    proc.prepareToPlay (sr, bs);
    FakePlayHead ph; proc.setPlayHead (&ph);
    const int total = (int) (sr * 60.0 / ph.bpm * 16.0);
    const int nch = proc.getTotalNumOutputChannels();
    juce::AudioBuffer<float> out (2, total), block (nch, bs);
    float auxPeak = 0.0f, mainPeakB = 0.0f;
    juce::MidiBuffer midi;
    float peak = 0.0f;
    for (int pos = 0; pos < total; pos += bs)
    {
        const int n = juce::jmin (bs, total - pos);
        juce::AudioBuffer<float> blk (block.getArrayOfWritePointers(), nch, n);
        proc.processBlock (blk, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, blk, ch, 0, n);
        if (nch >= 6) auxPeak = juce::jmax (auxPeak, blk.getMagnitude (4, 0, n));
        ph.ppq += n / (sr * 60.0 / ph.bpm);
    }
    peak = out.getMagnitude (0, total);
    std::cout << "render peak " << peak << "  channels " << nch << "  slot B own-out peak " << auxPeak << "\n";
    if (nch != 10 || auxPeak < 0.001f) ++fails;
    if (peak < 0.01f || ! std::isfinite (peak)) ++fails;
    {
        auto f = dir.getChildFile ("render.wav"); f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (out, 0, total);
    }

    // state round trip
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    FGenProcessor proc2; proc2.setStateInformation (mb.getData(), (int) mb.getSize());
    bool same = proc2.getSample (0) != nullptr;
    for (int i = 0; i < 64; ++i) same &= proc2.getStep (0, i).pack() == proc.getStep (0, i).pack();
    std::cout << "state roundtrip " << (same ? "OK" : "FAIL") << "\n";
    if (! same) ++fails;

    // UI snapshot
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ph.ppq = 2.0;
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = dir.getChildFile ("ui.png"); f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "ui snapshot " << f.getFullPathName() << "\n";
    }

    std::cout << (fails == 0 ? "ALL OK" : "FAILURES: " + juce::String (fails)) << "\n";
    return fails == 0 ? 0 : 1;
}
