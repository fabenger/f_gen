#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace fgen
{
/** Detects the root note (pitch class) of a one-shot: single notes and chord stabs.
    Uses a chromagram built from FFT frames plus chord template matching and a bass bonus. */
struct RootResult
{
    int rootPc       = 0;      // 0 = C ... 11 = B
    bool pitched     = false;
    juce::String description;  // e.g. "Am", "C", "noise"
};

class RootDetector
{
public:
    static RootResult detect (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        RootResult res;
        const int numSamples = buffer.getNumSamples();
        if (numSamples < 512 || sampleRate <= 0)
        {
            res.description = "-";
            return res;
        }

        // mono mix
        std::vector<float> mono ((size_t) numSamples, 0.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i) mono[(size_t) i] += d[i];
        }

        constexpr int order = 13, fftSize = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false);

        std::vector<float> spectrumSum ((size_t) fftSize / 2, 0.0f);
        std::vector<float> frame ((size_t) fftSize * 2, 0.0f);

        const int hop = fftSize / 4;
        const int maxAnalyse = juce::jmin (numSamples, (int) (sampleRate * 3.0));
        const int skip = juce::jmin ((int) (sampleRate * 0.015), maxAnalyse / 8); // skip the very first click
        int frames = 0;

        for (int start = skip; start < juce::jmax (skip + 1, maxAnalyse - fftSize / 2); start += hop)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            float energy = 0.0f;
            for (int i = 0; i < fftSize; ++i)
            {
                const int idx = start + i;
                const float v = idx < numSamples ? mono[(size_t) idx] : 0.0f;
                frame[(size_t) i] = v;
                energy += v * v;
            }
            if (energy < 1.0e-6f) { if (frames > 0) break; else continue; }

            window.multiplyWithWindowingTable (frame.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (frame.data());
            for (int k = 0; k < fftSize / 2; ++k) spectrumSum[(size_t) k] += frame[(size_t) k];
            ++frames;
            if (frames > 24) break;
        }

        if (frames == 0)
        {
            res.description = "silent";
            return res;
        }

        const double binHz = sampleRate / fftSize;

        // spectral flatness 60 Hz .. 5 kHz
        double logSum = 0.0, linSum = 0.0;
        int n = 0;
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const double f = k * binHz;
            if (f < 60.0 || f > 5000.0) continue;
            const double m = spectrumSum[(size_t) k] + 1.0e-9;
            logSum += std::log (m);
            linSum += m;
            ++n;
        }
        const double flatness = n > 0 ? std::exp (logSum / n) / (linSum / n) : 1.0;

        // peak picking -> chroma (only spectral peaks count, which suppresses noise)
        std::array<double, 12> chroma {};
        double bassMag = 0.0;
        int bassPc = -1;
        float maxMag = 0.0f;
        for (auto v : spectrumSum) maxMag = juce::jmax (maxMag, v);

        for (int k = 2; k < fftSize / 2 - 2; ++k)
        {
            const double f = k * binHz;
            if (f < 40.0 || f > 5000.0) continue;
            const float m = spectrumSum[(size_t) k];
            if (m < maxMag * 0.02f) continue;
            if (! (m > spectrumSum[(size_t) k - 1] && m >= spectrumSum[(size_t) k + 1])) continue;

            // parabolic interpolation for precise frequency
            const float a = spectrumSum[(size_t) k - 1], b = m, c = spectrumSum[(size_t) k + 1];
            const float denom = a - 2.0f * b + c;
            const double delta = std::abs (denom) > 1.0e-12f ? 0.5 * (a - c) / denom : 0.0;
            const double freq = (k + delta) * binHz;

            const double midi = 69.0 + 12.0 * std::log2 (freq / 440.0);
            const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
            const double weight = m / (1.0 + freq / 1500.0);
            chroma[(size_t) pc] += weight;

            if (freq < 400.0 && m > bassMag * 1.0 && m > maxMag * 0.15f && bassPc < 0)
            {
                // first strong low peak = bass note
                bassMag = m;
                bassPc = pc;
            }
        }

        double chromaTotal = 0.0;
        for (auto c : chroma) chromaTotal += c;
        if (chromaTotal <= 0.0)
        {
            res.description = "noise";
            return res;
        }

        double norm = 0.0;
        for (auto c : chroma) norm += c * c;
        norm = std::sqrt (norm);

        struct Tmpl { const char* suffix; std::vector<std::pair<int, double>> notes; };
        static const std::vector<Tmpl> templates {
            { "",     { { 0, 1.0 }, { 7, 0.5 }, { 4, 0.25 } } },              // single note with harmonics
            { "",     { { 0, 1.0 }, { 4, 0.8 }, { 7, 0.8 } } },               // major
            { "m",    { { 0, 1.0 }, { 3, 0.8 }, { 7, 0.8 } } },               // minor
            { "7",    { { 0, 1.0 }, { 4, 0.7 }, { 7, 0.7 }, { 10, 0.6 } } },
            { "m7",   { { 0, 1.0 }, { 3, 0.7 }, { 7, 0.7 }, { 10, 0.6 } } },
            { "maj7", { { 0, 1.0 }, { 4, 0.7 }, { 7, 0.7 }, { 11, 0.6 } } },
            { "m9",   { { 0, 1.0 }, { 3, 0.7 }, { 7, 0.6 }, { 10, 0.6 }, { 2, 0.5 } } },
            { "sus",  { { 0, 1.0 }, { 5, 0.7 }, { 7, 0.8 } } },
        };

        double bestScore = -1.0;
        int bestRoot = 0;
        const char* bestSuffix = "";
        for (int r = 0; r < 12; ++r)
        {
            for (auto& t : templates)
            {
                double dot = 0.0, tn = 0.0;
                for (auto& [iv, w] : t.notes)
                {
                    dot += chroma[(size_t) ((r + iv) % 12)] * w;
                    tn += w * w;
                }
                double score = dot / (norm * std::sqrt (tn));
                if (r == bassPc) score += 0.12;
                // the root itself must carry real energy
                score *= 0.6 + 0.4 * (chroma[(size_t) r] / (chromaTotal / 3.0 + 1.0e-12) > 1.0 ? 1.0 : chroma[(size_t) r] / (chromaTotal / 3.0));
                if (score > bestScore)
                {
                    bestScore = score;
                    bestRoot = r;
                    bestSuffix = t.suffix;
                }
            }
        }

        res.rootPc = bestRoot;
        res.pitched = flatness < 0.25 && bestScore > 0.55;
        res.description = res.pitched ? noteName (bestRoot) + bestSuffix : juce::String ("perc (") + noteName (bestRoot) + ")";
        return res;
    }

    static juce::String noteName (int pc)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[(pc % 12 + 12) % 12];
    }
};
} // namespace fgen
