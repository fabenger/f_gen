#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace fgen
{
constexpr int kNumSlots     = 4;
constexpr int kStepsPerBar  = 16;
constexpr int kMaxBars      = 8;
constexpr int kMaxSteps     = kStepsPerBar * kMaxBars;

//==============================================================================
struct Step
{
    bool on      = false;
    int  vel     = 100;   // 1..127
    int  degree  = 0;     // scale degree offset, -8..7
    int  ratchet = 1;     // 1..3 hits per step

    uint32_t pack() const noexcept
    {
        return (uint32_t) (on ? 1 : 0)
             | ((uint32_t) juce::jlimit (0, 127, vel) << 1)
             | ((uint32_t) (juce::jlimit (-8, 7, degree) + 8) << 8)
             | ((uint32_t) (juce::jlimit (1, 3, ratchet) - 1) << 12);
    }

    static Step unpack (uint32_t v) noexcept
    {
        Step s;
        s.on      = (v & 1u) != 0;
        s.vel     = (int) ((v >> 1) & 127u);
        s.degree  = (int) ((v >> 8) & 15u) - 8;
        s.ratchet = (int) ((v >> 12) & 3u) + 1;
        if (s.vel == 0) s.vel = 100;
        return s;
    }
};

using PatternData = std::array<Step, kMaxSteps>;

/** Lock-free pattern storage shared between UI and audio thread. */
struct AtomicPattern
{
    std::array<std::atomic<uint32_t>, kMaxSteps> steps;

    AtomicPattern() { for (auto& s : steps) s.store (Step().pack()); }

    Step get (int i) const noexcept       { return Step::unpack (steps[(size_t) i].load (std::memory_order_relaxed)); }
    void set (int i, const Step& s)       { steps[(size_t) i].store (s.pack(), std::memory_order_relaxed); }

    PatternData read() const
    {
        PatternData d;
        for (int i = 0; i < kMaxSteps; ++i) d[(size_t) i] = get (i);
        return d;
    }

    void write (const PatternData& d)
    {
        for (int i = 0; i < kMaxSteps; ++i) set (i, d[(size_t) i]);
    }

    juce::String toString() const
    {
        juce::StringArray a;
        for (auto& s : steps) a.add (juce::String ((int) s.load()));
        return a.joinIntoString (",");
    }

    void fromString (const juce::String& str)
    {
        juce::StringArray a;
        a.addTokens (str, ",", "");
        for (int i = 0; i < kMaxSteps; ++i)
            set (i, i < a.size() ? Step::unpack ((uint32_t) a[i].getIntValue()) : Step());
    }
};

//==============================================================================
// Musical helpers
inline const juce::StringArray& noteNames()
{
    static juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline const juce::StringArray& scaleNames()
{
    static juce::StringArray n { "Minor", "Major", "Dorian", "Phrygian", "Minor Pent", "Major Pent", "Harm Minor", "Mixolydian" };
    return n;
}

inline const std::vector<int>& scaleIntervals (int scaleIndex)
{
    static const std::vector<std::vector<int>> scales {
        { 0, 2, 3, 5, 7, 8, 10 },   // minor
        { 0, 2, 4, 5, 7, 9, 11 },   // major
        { 0, 2, 3, 5, 7, 9, 10 },   // dorian
        { 0, 1, 3, 5, 7, 8, 10 },   // phrygian
        { 0, 3, 5, 7, 10 },         // minor pentatonic
        { 0, 2, 4, 7, 9 },          // major pentatonic
        { 0, 2, 3, 5, 7, 8, 11 },   // harmonic minor
        { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian
    };
    return scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, scaleIndex)];
}

inline int degreeToSemitones (int degree, int scaleIndex)
{
    const auto& sc = scaleIntervals (scaleIndex);
    const int n = (int) sc.size();
    int oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
    int idx = degree - oct * n;
    return sc[(size_t) idx] + 12 * oct;
}

//==============================================================================
// Rhythm generator
enum class Style { House = 0, Deep, Minimal, Dub, Euclid, Random };

inline const juce::StringArray& styleNames()
{
    static juce::StringArray n { "House", "Deep / UG", "Minimal Detroit", "Dub Chords", "Euclid", "Wild" };
    return n;
}

struct GenSettings
{
    Style style      = Style::House;
    float density    = 0.5f;
    float variation  = 0.4f;
    float melody     = 0.0f;
    int   bars       = 4;
    bool  percussive = false;
};

class Generator
{
public:
    static PatternData generate (const GenSettings& g, juce::Random& rng)
    {
        const int total = juce::jlimit (1, kMaxBars, g.bars) * kStepsPerBar;
        std::vector<Step> motif;
        bool polyMeter = false;
        int cycle = 16;

        switch (g.style)
        {
            case Style::House:   motif = fromTemplateOrWeights (houseTemplates(), houseWeights(), g, rng); break;
            case Style::Deep:    motif = fromTemplateOrWeights (deepTemplates(),  deepWeights(),  g, rng); break;
            case Style::Dub:     motif = fromTemplateOrWeights (dubTemplates(),   dubWeights(),   g, rng); break;
            case Style::Minimal: motif = minimalMotif (g, rng, polyMeter, cycle); break;
            case Style::Euclid:  motif = euclidMotif (g, rng); break;
            case Style::Random:  motif = wildMotif (g, rng); break;
        }

        // ensure something plays
        int hits = 0;
        for (auto& s : motif) hits += s.on ? 1 : 0;
        if (hits == 0 && ! motif.empty())
            motif[(size_t) (2 % (int) motif.size())].on = true;

        applyVelocities (motif, g, rng, polyMeter ? cycle : 16);
        applyMelody (motif, g, rng);

        // chord / bar movement
        std::array<int, kMaxBars> barShift {};
        if (! g.percussive && g.melody > 0.35f && rng.nextFloat() < g.melody)
        {
            static const int progs[][4] = { { 0, 0, 0, 0 }, { 0, 0, -2, -2 }, { 0, 3, 0, 4 }, { 0, -1, -2, -3 },
                                            { 0, 0, 3, 3 }, { 0, 5, 3, 4 }, { 0, 2, 0, -2 } };
            const auto& p = progs[rng.nextInt (7)];
            const int barsPerChord = g.bars >= 8 ? 2 : 1;
            for (int b = 0; b < kMaxBars; ++b)
                barShift[(size_t) b] = p[(b / barsPerChord) % 4];
        }

        PatternData out {};
        for (int i = 0; i < total; ++i)
        {
            Step s = motif[(size_t) (i % (int) motif.size())];
            s.degree = juce::jlimit (-8, 7, s.degree + barShift[(size_t) (i / kStepsPerBar)]);
            out[(size_t) i] = s;
        }

        // variations per bar (keep first bar / motif intact)
        const int motifLen = (int) motif.size();
        for (int i = juce::jmax (motifLen, kStepsPerBar); i < total; ++i)
        {
            if (rng.nextFloat() < g.variation * 0.12f)
            {
                auto& s = out[(size_t) i];
                if (s.on)
                {
                    // move or drop
                    if (rng.nextBool() && i + 1 < total && ! out[(size_t) i + 1].on)
                        std::swap (out[(size_t) i], out[(size_t) i + 1]);
                    else
                        s.on = false;
                }
                else if (rng.nextFloat() < 0.5f)
                {
                    s.on = true;
                    s.vel = 60 + rng.nextInt (40);
                }
            }
        }

        // fills at the end of each 4-bar phrase / the loop
        for (int b = 0; b < g.bars; ++b)
        {
            const bool phraseEnd = (b % 4 == 3) || (b == g.bars - 1);
            if (! phraseEnd || rng.nextFloat() > g.variation) continue;

            for (int st = 12; st < 16; ++st)
            {
                auto& s = out[(size_t) (b * kStepsPerBar + st)];
                if (rng.nextFloat() < 0.35f + 0.3f * g.density)
                {
                    s.on = true;
                    s.vel = 70 + rng.nextInt (50);
                    if (g.percussive && rng.nextFloat() < 0.3f) s.ratchet = 2 + rng.nextInt (2);
                    if (! g.percussive && g.melody > 0.0f && rng.nextFloat() < g.melody)
                        s.degree = juce::jlimit (-8, 7, s.degree + rng.nextInt (5) - 1);
                }
            }
        }

        // tile to the full buffer so changing the bar count stays musical
        for (int i = total; i < kMaxSteps; ++i)
            out[(size_t) i] = out[(size_t) (i % total)];

        return out;
    }

private:
    using Template = const char*;

    static std::vector<Template> houseTemplates()
    {
        return { "..x...x...x...x.",   // classic offbeat stabs
                 "..x..x....x..x..",
                 "...x..x...x...x.",
                 "x..x..x...x..x..",
                 "..x.x...x..x..x.",
                 "..x...x..x....x.",
                 "..x...x...x...x...x...x...x..xx." };
    }
    static std::array<float, 16> houseWeights() { return { .15f, .2f, .85f, .3f, .1f, .2f, .85f, .3f, .15f, .2f, .85f, .3f, .1f, .25f, .85f, .35f }; }

    static std::vector<Template> deepTemplates()
    {
        return { "x..x..x.........",
                 "...x..x....x..x.",
                 "..x..x..x..x..x.",
                 ".x..x..x..x...x.",
                 "x..x...x..x..x..",
                 "...x...x..x.....",
                 "..x..x.....x..x.x..x..x...x....." };
    }
    static std::array<float, 16> deepWeights() { return { .2f, .15f, .4f, .6f, .1f, .2f, .5f, .4f, .1f, .25f, .45f, .55f, .1f, .35f, .45f, .5f }; }

    static std::vector<Template> dubTemplates()
    {
        return { "...x.......x....",
                 "..x........x..x.",
                 "x.....x.........",
                 "...x......x.....",
                 "..x.....x..x....",
                 "......x.......x.",
                 "...x.......x.......x......x...x." };
    }
    static std::array<float, 16> dubWeights() { return { .1f, .05f, .3f, .4f, .05f, .05f, .3f, .15f, .05f, .05f, .3f, .4f, .05f, .1f, .3f, .2f }; }

    static std::vector<Step> parse (Template t)
    {
        std::vector<Step> v;
        for (const char* c = t; *c != 0; ++c)
        {
            Step s;
            s.on = (*c == 'x');
            v.push_back (s);
        }
        return v;
    }

    static std::vector<Step> fromWeights (const std::array<float, 16>& w, const GenSettings& g, juce::Random& rng, int len)
    {
        std::vector<Step> v ((size_t) len);
        const float scale = 0.35f + 1.3f * g.density;
        for (int i = 0; i < len; ++i)
            v[(size_t) i].on = rng.nextFloat() < juce::jlimit (0.0f, 0.97f, w[(size_t) (i % 16)] * scale);
        return v;
    }

    static std::vector<Step> fromTemplateOrWeights (const std::vector<Template>& templates, const std::array<float, 16>& w,
                                                    const GenSettings& g, juce::Random& rng)
    {
        if (rng.nextFloat() < 0.55f)
        {
            auto v = parse (templates[(size_t) rng.nextInt ((int) templates.size())]);
            // density: thin out or add
            for (size_t i = 0; i < v.size(); ++i)
            {
                const float wi = w[i % 16];
                if (v[i].on && rng.nextFloat() < (0.5f - g.density) * 0.5f) v[i].on = false;
                else if (! v[i].on && rng.nextFloat() < (g.density - 0.5f) * wi) v[i].on = true;
            }
            return v;
        }
        return fromWeights (w, g, rng, rng.nextFloat() < 0.4f ? 32 : 16);
    }

    static std::array<bool, 64> euclid (int k, int n, int rot)
    {
        std::array<bool, 64> r {};
        for (int i = 0; i < n; ++i)
            r[(size_t) i] = (((i + rot) * k) % n) < k;
        return r;
    }

    static std::vector<Step> minimalMotif (const GenSettings& g, juce::Random& rng, bool& polyMeter, int& cycle)
    {
        if (rng.nextFloat() < 0.6f)
        {
            // polymetric cycle running over the bar line -> evolving minimal loops
            static const int cycles[] = { 3, 5, 6, 7, 10, 12 };
            cycle = cycles[rng.nextInt (6)];
            polyMeter = true;
            const int k = juce::jlimit (1, cycle - 1, 1 + (int) std::round (g.density * (cycle / 2.5f)) + rng.nextInt (2) - (cycle <= 5 ? 1 : 0));
            auto e = euclid (k, cycle, rng.nextInt (cycle));
            // write a full 2-bar motif so the cycle keeps rotating against the bar
            const int len = 32;
            std::vector<Step> v ((size_t) len);
            for (int i = 0; i < len; ++i) v[(size_t) i].on = e[(size_t) (i % cycle)];
            // the polymeter does not line up with 32, make it evolve across all bars instead
            std::vector<Step> full ((size_t) kMaxSteps);
            for (int i = 0; i < kMaxSteps; ++i) full[(size_t) i].on = e[(size_t) (i % cycle)];
            juce::ignoreUnused (v);
            return trimToBars (full, g.bars);
        }

        const int k = juce::jlimit (2, 9, 3 + (int) std::round (g.density * 4.0f) + rng.nextInt (2));
        auto e = euclid (k, 16, rng.nextInt (16));
        std::vector<Step> v (16);
        for (int i = 0; i < 16; ++i) v[(size_t) i].on = e[(size_t) i];
        return v;
    }

    static std::vector<Step> trimToBars (const std::vector<Step>& full, int bars)
    {
        return std::vector<Step> (full.begin(), full.begin() + juce::jlimit (1, kMaxBars, bars) * kStepsPerBar);
    }

    static std::vector<Step> euclidMotif (const GenSettings& g, juce::Random& rng)
    {
        const int k = juce::jlimit (1, 15, 2 + (int) std::round (g.density * 9.0f) + rng.nextInt (3) - 1);
        auto e = euclid (k, 16, rng.nextInt (16));
        std::vector<Step> v (16);
        for (int i = 0; i < 16; ++i) v[(size_t) i].on = e[(size_t) i];
        return v;
    }

    static std::vector<Step> wildMotif (const GenSettings& g, juce::Random& rng)
    {
        const int len = rng.nextBool() ? 32 : 16;
        std::vector<Step> v ((size_t) len);
        for (auto& s : v)
        {
            s.on = rng.nextFloat() < 0.15f + 0.6f * g.density;
            if (s.on && rng.nextFloat() < 0.15f * g.variation) s.ratchet = 2 + rng.nextInt (2);
        }
        return v;
    }

    static void applyVelocities (std::vector<Step>& v, const GenSettings& g, juce::Random& rng, int accentCycle)
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            auto& s = v[i];
            const int pos = (int) (i % 16);
            int base;
            if (pos % 4 == 2)       base = 112;   // offbeat accent
            else if (pos % 4 == 0)  base = 100;
            else                    base = 78;    // ghosty 16ths
            if (accentCycle != 16 && (int) (i % (size_t) accentCycle) == 0) base = 115;
            base += (int) ((rng.nextFloat() * 2.0f - 1.0f) * 14.0f * g.variation);
            s.vel = juce::jlimit (20, 127, base);
        }
    }

    static void applyMelody (std::vector<Step>& v, const GenSettings& g, juce::Random& rng)
    {
        if (g.percussive || g.melody <= 0.0f) return;
        static const int degrees[] = { 0, 0, 4, 2, -3, 3, 5, 7, -2, 1 };
        for (auto& s : v)
            if (s.on && rng.nextFloat() < g.melody * 0.6f)
                s.degree = degrees[rng.nextInt (juce::jlimit (3, 10, 3 + (int) (g.melody * 7.0f)))];
    }
};

} // namespace fgen
