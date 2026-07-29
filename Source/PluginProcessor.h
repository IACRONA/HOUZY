#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// HOUZY — master bus glue compressor + parallel "OTT-hardness" + hard clip.
//
// Design goals:
//  - ONE main "Amount" knob: scales compression strength AND parallel-mix AND
//    clipper drive, with built-in auto make-up gain so perceived loudness stays
//    roughly constant — you hear glue/density increase, not a volume jump.
//  - A simple separate Output "Gain" trim.
//  - A brickwall hard clipper at -0.1 dBFS (4x oversampled) so nothing ever
//    crosses 0 dBFS.
//
// Why single-band + parallel instead of true multiband (OTT)?
//  Real OTT changes timbre because it compresses bands independently. To get
//  OTT-like *density/hardness* while keeping the tonal balance ("not change the
//  sound"), we use a single-band glue compressor blended in parallel with the
//  dry signal. That fattens transients (kick/bass) and glues the mix without
//  pumping the spectral balance around.
//==============================================================================
class HouseCompAudioProcessor : public juce::AudioProcessor
{
public:
    HouseCompAudioProcessor();
    ~HouseCompAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    // Flush every recursive state (filters, envelopes, delay lines). The host calls
    // this on transport jumps; it also lets a NaN-poisoned state recover.
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HOUZY"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Exposed for the editor's gain-reduction meter (atomic, dB, negative).
    std::atomic<float> currentGainReductionDb { 0.0f };

    // Honest per-stage metering: how much EACH stage is pulling down right now
    // (dB, <=0), plus a 0..1 "clip activity" (how hard the clipper is slicing).
    std::atomic<float> compGrDb    { 0.0f };  // compressor reduction (dB, <=0)
    std::atomic<float> limiterGrDb { 0.0f };  // limiter reduction   (dB, <=0)
    std::atomic<float> clipActivity{ 0.0f };  // 0 = no clip, 1 = clipping hard
    // MASK mode only: 0..1, how far the cancellation pulse is tilted away from a plain
    // broadband slice (0 = behaving like a normal clipper, 1 = error fully relocated).
    std::atomic<float> maskTilt    { 0.0f };
    // FUTURE + AUTO release: which note value the engine picked, or -1 when AUTO is off.
    // Lets the UI show what it chose instead of leaving the knob looking stuck.
    std::atomic<int>   autoRelNote { -1 };

    // Output meters (dB) for the UI: short-term RMS and BS.1770 loudness.
    std::atomic<float> meterRmsDb  { -100.0f };
    std::atomic<float> meterLufsDb { -100.0f };   // MOMENTARY (400 ms) — the main readout
    std::atomic<float> meterLufsShortDb { -100.0f };  // SHORT-TERM (3 s)
    std::atomic<float> meterLufsIntDb   { -100.0f };  // INTEGRATED (gated) — for mastering

    // A/B loudness-match trim (dB) set by the editor. Applied at the very output so
    // the A and B snapshots can be auditioned at matched perceived loudness.
    std::atomic<float> abMatchTrimDb { 0.0f };

    // Rolling history of gain reduction (dB, <=0) for the scrolling graph.
    // Lock-free: the audio thread writes at historyWritePos, the UI reads.
    // grHistory = TOTAL (comp+limiter), grHistoryComp = compressor-only, so the
    // graph can shade the compressor vs limiter contributions separately.
    static constexpr int kHistorySize = 512;
    std::array<std::atomic<float>, kHistorySize> grHistory {};
    std::array<std::atomic<float>, kHistorySize> grHistoryComp {};
    std::array<std::atomic<float>, kHistorySize> clipHistory {};  // 0..1 clip activity
    std::atomic<int> historyWritePos { 0 };

    // Rolling INPUT waveform (downsampled) for the scope display, so you see the
    // actual signal going in, not just the reduction envelope.
    static constexpr int kScopeSize = 512;
    std::array<std::atomic<float>, kScopeSize> scopeHistory {};
    std::atomic<int> scopeWritePos { 0 };

    juce::AudioProcessorValueTreeState apvts;

    // Mirror of PartialEngine::kOrder, exposed so the .cpp can static_assert it against
    // cfg::futureFftOrder (Config.h is not visible from this header).
    static constexpr int kFutureOrderCheck = 11;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ========================================================================
    //  CLEAN 3-STAGE ENGINE
    //    Stage 1  Compressor  — evens the MACRO dynamics (sections: verse/chorus)
    //                            toward a very slow auto level. Stereo-linked,
    //                            single band, reduction capped so it never floors.
    //    Stage 2  Limiter     — fast peak control, presses transients to ceiling.
    //    Stage 3  Clipper      — per-sample slice at 0 dBFS => the FLAT, even tops.
    //  AMOUNT drives all three harder at once.
    // ========================================================================

    // --- Stage 1: single-band, stereo-linked auto-leveling compressor -------
    struct Compressor
    {
        void prepare (double sr);
        // detector: stereo-linked level (linear). Returns linear gain; reports the
        // applied gain change (dB, <=0) via outGrDb.
        float process (float detector, float amount01, float ratio,
                       float attackDbPerMs, float releaseDbPerMs, float maxReductionDb,
                       float& outGrDb, float threshScale = 1.0f);

        double sampleRate { 44100.0 };
        float rmsSq  { 1.0e-6f };   // running mean-square of the input (energy)
        float peakEnv { 0.0f };     // fast peak follower (for crest factor)
        float autoLvlDb { -18.0f }; // very slow auto level => the threshold ref
        float gainDb { 0.0f };      // current applied gain (dB, <=0)
        // Smoothing coefficients — depend ONLY on sampleRate, so computed once in
        // prepare() instead of via std::exp on every sample (up to 20 exp/frame).
        float rmsCoeff { 0.0f }, pkCoeff { 0.0f }, slowCoeff { 0.0f }, fastCoeff { 0.0f };

        // --- MODERN engine extras -------------------------------------------
        // Sidechain HPF (detector only, never the audio): stops the kick's sub
        // energy from dragging the whole band down on every beat.
        float scHpfX1 { 0.0f }, scHpfY1 { 0.0f }, scHpfCoeff { 0.0f };
        // Cascaded gain smoother: makes the gain envelope's derivative continuous
        // (the linear slew has sharp corners = broadband grit on bass).
        float gainSm1 { 0.0f }, gainSm2 { 0.0f }, gainSmCoeff { 0.0f };
        bool  modern { false };     // set per block by the processor
        bool  useScHpf { true };    // false for the 4-band comps (see processBlock)
    };
    Compressor comp;

    // --- "All Mix Comp" 4-band engine ---------------------------------------
    // 4 crossover bands (0-200, 200-1500, 1500-7000, 7000-20000 Hz), each with
    // its own stereo-linked compressor driven by its own knob. Used only when the
    // All Mix Comp toggle is on; otherwise the single-band 'comp' above runs.
    static constexpr int kBands = 4;
    Compressor bandComp[kBands];
    // Cascaded Linkwitz-Riley crossovers, per channel: lr[0]@200, lr[1]@1500,
    // lr[2]@7000. Splitting low-first then recombining gives 4 bands.
    juce::dsp::LinkwitzRileyFilter<float> xover[3][2]; // [splitPoint][channel]
    // Phase-compensation allpasses: already-separated lower bands are passed
    // through allpass copies of the LATER split points so all bands share the
    // same phase and sum flat (no dip at the crossover frequencies).
    //   apLow[k]  = allpass @ split(k+1..) applied to the LOW band
    //   apMid[k]  = allpass @ split(k+2..) applied to the LO-MID band
    juce::dsp::LinkwitzRileyFilter<float> apLowB[2][2];  // LOW  needs AP@1500,@7000
    juce::dsp::LinkwitzRileyFilter<float> apMidB[1][2];  // LOMID needs AP@7000

    // --- Stage 2: fast stereo-linked peak limiter ---------------------------
    //  + TRUE PEAK: estimates the inter-sample peak (2x halfway point) so peaks
    //    hidden between samples don't slip through and distort later.
    //  + LOOKAHEAD: a short delay line lets the gain start ducking BEFORE the peak
    //    arrives, so loud transients are caught smoothly (no clicks). Adds latency.
    struct PeakLimiter
    {
        void prepare (double sr, int maxBlock);
        void setLookahead (bool on);
        void flush();   // clear delay lines + envelope (on toggle changes)
        void process (float& l, float& r, float ceilingLin, bool trueePeak);
        int  latencySamples() const;
        int  maxLatencySamples() const { return laSamples; }

        double sampleRate { 44100.0 };
        // Two-stage release: fast + slow envelopes, output = max (less low-freq
        // ripple => cleaner bass under heavy limiting). Plus a short hold.
        float envFast { 1.0f }, envSlow { 1.0f };
        float relFastCoeff { 0.0f }, relSlowCoeff { 0.0f };
        int   holdSamples { 0 }, holdCounter { 0 };
        float prevL { 0.0f }, prevR { 0.0f };   // 1-sample history (inter-sample peak)
        float prev2L { 0.0f }, prev2R { 0.0f }; // 2-sample history (Catmull-Rom)
        float lastGain { 1.0f };                // gain applied on the last sample (for metering)

        // SMART mode: frequency-dependent release (low band slower, high faster).
        bool  smartMode { false };
        void  setSmart (bool on) { smartMode = on; }

        // CASCADE mode (Sanfilippo-style): instead of the 2-stage max-release, the
        // gain is smoothed by a SERIES of one-pole filters. Cascading makes the gain
        // curve's derivative continuous => far fewer harmonics when the gain
        // multiplies the signal (cleaner bass under heavy limiting). Off = current.
        bool  cascadeMode { false };
        void  setCascade (bool on) { cascadeMode = on; }
        static constexpr int kCascadeStages = 4;   // 4 one-poles in series
        float casc[kCascadeStages] { 1.0f, 1.0f, 1.0f, 1.0f };
        float cascAtkCoeff { 0.0f }, cascRelCoeff { 0.0f };

        // Sliding-window MINIMUM in O(1) amortised, via a monotonic deque.
        //
        // This replaces a full linear rescan of the lookahead window on EVERY sample.
        // With IRC IV that was 7 limiter instances x 96 samples ~= 32 million
        // comparisons per second — the hottest loop in the whole plugin. The result is
        // identical (it is the same minimum over the same window), it just stops
        // re-examining values that have not changed.
        //
        // Invariant: values in the deque are strictly increasing front-to-back, so the
        // front is always the window minimum. A new value evicts every value at the
        // back it is smaller than, because those can never be the minimum again while
        // the newer, smaller one is still in the window.
        struct MinWindow
        {
            void prepare (int windowLen)
            {
                len = juce::jmax (1, windowLen);
                val.assign ((size_t) len + 1, 1.0f);   // +1 slack slot: head==tail means empty
                pos.assign ((size_t) len + 1, 0);
                reset();
            }
            void reset() { head = tail = 0; t = 0; }

            // Push the newest value; returns the minimum over the last `len` values.
            inline float push (float v)
            {
                while (tail != head && val[(size_t) prev (tail)] >= v)
                    tail = prev (tail);

                val[(size_t) tail] = v;
                pos[(size_t) tail] = t;
                tail = next (tail);

                while (pos[(size_t) head] + len <= t)
                    head = next (head);

                ++t;
                return val[(size_t) head];
            }

        private:
            int cap()        const { return (int) val.size(); }
            int next (int i) const { return (i + 1 >= cap()) ? 0 : i + 1; }
            int prev (int i) const { return (i - 1 < 0) ? cap() - 1 : i - 1; }

            std::vector<float>     val;
            std::vector<long long> pos;
            long long t { 0 };
            int len { 1 }, head { 0 }, tail { 0 };
        };

        // Lookahead delay line (stereo) + gain-smoothing.
        bool  lookaheadOn { true };
        int   laSamples { 0 };
        std::vector<float> dlL, dlR;
        int   dlPos { 0 };
        MinWindow gEnvMin;                    // smoothed-gain window minimum (CLASSIC)

        // --- MODERN engine ---------------------------------------------------
        // In MODERN the min-scan runs on the RAW target gain and the result is then
        // smoothed, instead of min-scanning an already-smoothed envelope. Min-of-
        // smoothed leaves rectangular corners in the gain (a sinc-like burst =
        // broadband distortion, worst on bass); smooth-of-min is the correct order.
        bool  modern { false };
        MinWindow tgtMin;                     // raw target-gain window minimum (MODERN)
        float mSm1 { 1.0f }, mSm2 { 1.0f };   // 2-stage smoother after the min
        float mSmCoeff { 0.0f };
        // Switching engine changes the cascade time-constant correction, so the
        // coefficients must be re-derived (prepare() runs before `modern` is known).
        void setModern (bool on)
        {
            if (on == modern) return;
            modern = on;
            const float atkMs = 1.5f, relMs = 90.0f;
            const float corr = on ? (1.0f / std::sqrt ((float) kCascadeStages))
                                  : (1.0f / (float) kCascadeStages);
            cascAtkCoeff = std::exp (-1.0f / (0.001f * atkMs * corr * (float) sampleRate));
            cascRelCoeff = std::exp (-1.0f / (0.001f * relMs * corr * (float) sampleRate));
        }
    };
    PeakLimiter limiter;

    // --- MASK mode: peak-cancellation clipper (PC-CFR) -----------------------
    // A normal clipper SLICES the top off the waveform. A slice is a sharp event, so
    // its error is BROADBAND — every kick peak subtracts a little from the hats, the
    // vocal, the reverb tails riding on top. That is why hard-clipped house goes dull
    // and sandy on the highs when you push it. Oversampling does not help: it makes
    // that error CLEAN (alias-free) but does not change WHERE it lands in the spectrum.
    //
    // So instead of slicing, we SUBTRACT a short band-limited pulse centred exactly on
    // the peak. The pulse's spectrum is chosen in advance, so the distortion appears
    // only in the bands we picked — it is never generated broadband in the first place.
    // Which bands? The ones that are loud right now, because the kick that makes the
    // peaks is also the thing that masks the distortion. (This is Peak Cancellation
    // CFR from LTE transmitters, with the kernel designed by a masking model.)
    //
    // The pulse is built as p = qL*pLow + qM*pMid + qH*pHigh, normalised so p[centre]
    // is exactly 1 (the peak lands precisely on the ceiling). The three band pulses sum
    // to a unit impulse by construction, so EQUAL weights give p == delta == today's
    // hard clip: the degenerate case is exact, not approximate.
    struct MaskClipper
    {
        void prepare (double sr, int maxBlock);
        void flush();
        // Cancels peaks above ceilingLin in place. amount01 scales the correction
        // (0 = pure delay, i.e. identical to not having the stage at all).
        void processBlock (juce::AudioBuffer<float>& buffer, float ceilingLin,
                           float amount01, int iters);
        int  maxChunk() const { return juce::jmax (1, bufLen - 2 * kHalf); }
        int  latencySamples() const { return 2 * kHalf; }
        // 0..1, how far the pulse is tilted away from a plain slice (for the meter).
        float lastTilt() const { return tilt; }

    private:
        void designKernels();                                    // once, in prepare
        void updateWeights (const float* l, const float* r, int n);   // masking -> qSm
        void rebuildComposite();                                 // qSm -> pComp
        void processChunk (float* l, float* r, int n, float ceilingLin,
                           float amount01, int iters);

        double sampleRate { 48000.0 };
        int kLen { 0 };     // pulse length K (even)
        int kHalf { 0 };    // K/2 — also the one-sided reach of the pulse
        int bufLen { 0 };   // 2*kHalf + maxBlock

        std::vector<float> pLow, pMid, pHigh, pComp;   // band pulses + the composite
        std::vector<float> work[2];                    // lookahead work buffer per channel

        // Masking detector: five PARALLEL lowpasses on the mono sum. Bands come out by
        // telescoping subtraction, so they sum back to the input EXACTLY — no allpass
        // compensation bank needed (unlike the cascaded split the limiter uses).
        juce::dsp::LinkwitzRileyFilter<float> detLp[5];
        float lvl[6] {};                        // per-band level follower
        float qSm[3] { 1.0f, 1.0f, 1.0f };      // smoothed pulse weights
        float atkC { 0.0f }, relC { 0.0f };
        float tilt { 0.0f };
        float nsE1[2] {}, nsE2[2] {};           // noise-shaping error feedback (opt-in)
    };
    MaskClipper maskClip;

    // --- MASK mode: crest-factor phase rotator -------------------------------
    // A house kick's peak is high because the waveform is ASYMMETRIC, not because it
    // carries that much energy. An allpass changes phase but not magnitude — so it
    // changes the peak WITHOUT changing anything you can hear. Slowly hunting for the
    // phase that flattens the crest buys 1-3 dB of headroom at zero distortion cost,
    // before the clipper does any work at all.
    //
    // Safety by construction: there is only ever ONE allpass in the audio path, so
    // there is no crossfade between two filters and therefore no comb filtering. The
    // search runs on three MEASUREMENT-ONLY probes; audio never passes through them.
    // The frequency glides at cfg::rotGlideRate (a few % per second) — far slower than
    // any audible phaser sweep.
    struct CrestRotator
    {
        void prepare (double sr);
        void flush();
        void processBlock (juce::AudioBuffer<float>& buffer);
        float currentHz() const { return fHz; }

    private:
        void updateCoeffs();
        void decideDirection();

        // 2nd-order allpass (RBJ), transposed direct form II.
        struct AP2
        {
            float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
            float z1 { 0.0f }, z2 { 0.0f };
            inline float process (float x)
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void reset() { z1 = z2 = 0.0f; }
            void set (double f0, double q, double sr);
        };

        double sampleRate { 48000.0 };
        float  fHz { 60.0f };
        AP2    audio[2];    // the ONLY filters the signal passes through
        AP2    probe[3];    // measurement only: fHz/spread, fHz, fHz*spread

        double pk[3] {}, sq[3] {};   // per-probe peak and energy over the window
        int    winPos { 0 }, winLen { 0 };
        int    dir { 0 };            // -1 / 0 / +1, from the last completed window
    };
    CrestRotator crestRot;

    // --- PHASE mode: loudness from crest reduction, not from compression -----
    // A waveform's PEAK is set by how the tones' phases line up, not by how much
    // energy it carries. Spread those phases and the peak drops while the loudness
    // stays exactly where it was — loudness is energy, and energy does not depend on
    // phase. The ear is very nearly deaf to it.
    //
    // So every dB taken off here is a dB of headroom for FREE: no compression, no
    // limiting, no distortion. And it happens BEFORE the limiter, so the limiter has
    // markedly less to do — which is the whole complaint about where loudness
    // currently comes from.
    //
    // Built from cascaded all-pass sections, log-spaced across the low end. All-pass
    // means the magnitude response is flat BY CONSTRUCTION: the tone colour cannot
    // change, only the timing of the phases. The sections together form a smooth
    // group-delay ramp (a "chirp"), which is what disperses a kick's impulse instead
    // of letting all its partials pile up into one spike.
    struct CrestDisperser
    {
        void prepare (double sr);
        void flush();
        void processBlock (juce::AudioBuffer<float>& buffer);
        float crestReductionDb() const { return reductionDb; }

    private:
        struct AP2
        {
            float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
            float z1 { 0.0f }, z2 { 0.0f };
            inline float process (float x)
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void reset() { z1 = z2 = 0.0f; }
            void set (double f0, double q, double sr);
        };

        void placeSections (double centreHz);
        void updatePlacement();

        static constexpr int kMaxSections = 12;
        AP2   ap[kMaxSections][2];
        int   sections { 0 };
        double sampleRate { 48000.0 };
        double centreHz { 80.0 };          // where the cascade currently sits

        // ADAPTIVE PLACEMENT. Six DETECTOR bands — the audio never passes through them,
        // they only measure. That is the borrowed idea from IRC ("look per band")
        // without its mechanism (splitting the signal), because splitting would bring
        // back exactly the crossover phase problems an all-pass cascade avoids.
        // The cascade stays ONE continuous filter and simply slides to wherever the
        // crest factor actually is.
        static constexpr int kDetBands = 6;
        juce::dsp::LinkwitzRileyFilter<float> detLp[kDetBands - 1];
        double bandPk[kDetBands] {}, bandSq[kDetBands] {};
        int    searchPos { 0 }, searchLen { 1 };

        // Crest measured before and after, so the panel can show what was gained.
        double pkIn { 0.0 }, sqIn { 0.0 }, pkOut { 0.0 }, sqOut { 0.0 };
        int    winPos { 0 }, winLen { 1 };
        float  reductionDb { 0.0f };
    };
    CrestDisperser disperser;

    // --- FUTURE engine: partial-based dynamics -------------------------------
    // Every classic compressor is y = g*x — ONE number scaling the whole signal. All
    // three long-standing complaints come from that single fact:
    //   * the detector must collapse the whole spectrum into one number, so a kick and
    //     a vocal at the same amplitude are indistinguishable to it;
    //   * one envelope has to serve both the transient and the body, so attack speed
    //     and punch are forced to trade against each other;
    //   * multiplying IS modulation, so moving the gain puts sidebands around the very
    //     bass the compressor was asked to even out.
    //
    // The way out is to stop scaling the signal and start editing its DESCRIPTION:
    // take the sound apart into individual tones and change the loudness of each one.
    // There is then no shared gain, so there is nothing left to pump, and changing a
    // tone's amplitude directly is not a multiplication of the signal — no sidebands.
    //
    // THIS VERSION ONLY TAKES THE SIGNAL APART AND PUTS IT BACK TOGETHER. No dynamics
    // yet. That is deliberate: if the round trip is not transparent there is no point
    // building anything on top of it, and this is the cheapest possible way to find out.
    //
    // Overlap-add STFT (Hann analysis + Hann synthesis, 75% overlap). Reconstruction is
    // exact because the squared window sums to a constant at that hop — measured in
    // prepare() rather than assumed, along with the FFT's own round-trip scaling.
    struct PartialEngine
    {
        void prepare (double sr, int maxBlock);
        void flush();
        void processBlock (juce::AudioBuffer<float>& buffer);
        // A full window, MEASURED with an impulse rather than reasoned about (the
        // obvious guess, fftSize - hop, is wrong and would have put the plugin out of
        // time with the session by a whole hop).
        int  latencySamples() const { return fftSize; }

        void  setAmount (float a01)  { amount01 = juce::jlimit (0.0f, 1.0f, a01); }
        void  setUpward (float u01)  { upward01 = juce::jlimit (0.0f, 1.0f, u01); }
        void  setBond   (bool on)    { bondOn = on; }
        // ALL MIX in FUTURE: four knobs set how hard the tones in each frequency range
        // are compressed. No crossovers are involved — the spectrum is already split, so
        // the bands are just a per-tone weighting of AMOUNT, blended smoothly across the
        // boundaries. That is a multiband with none of a multiband's seams.
        void  setBandAmounts (const float* amt4, float mix01);
        // How long the material's tones actually take to fade, in seconds. AUTO turns
        // this into the nearest note value — the plugin can measure decay, which is a
        // fact about the audio, unlike attack shape which is a matter of taste.
        float measuredDecaySec() const { return decaySec; }
        // atk01 = 0..1 from the ATTACK knob (meaning CYCLES of each tone's own wave).
        // relBeats = the release length in BEATS, chosen as a note value. Both are
        // unrelated to milliseconds in FUTURE — see setTiming().
        void  setTiming (float atk01, float relBeats, double bpm, bool smartRelease);
        float lastReductionDb() const { return maxRedDb; }   // >= 0, for the GR meter

        // FUTURE 2: detect onsets and decays on the ear's critical band instead of the
        // tone's own bin, and guard each tone's attack for a number of ITS OWN cycles
        // rather than a fixed number of frames. Set from the engine switch.
        bool future2 { false };
        // Clears ONLY the state whose meaning differs between the two modes, so switching
        // between them is clean. A full flush would empty the STFT buffers as well and
        // punch a hole in the audio; those hold plain samples and are valid either way.
        void resetEventState();

    private:
        void analyseFrame (int ch);      // window + FFT into spec[ch]
        void synthFrame (int ch);        // IFFT + window + overlap-add
        void applyDynamics();            // BOTH channels together — see the note below

        // The order is fixed at compile time (cfg::futureFftOrder), but Config.h is not
        // visible from this header, so it is mirrored here and cross-checked with a
        // static_assert in the .cpp — a mismatch cannot slip through silently.
        static constexpr int kOrder = 11;          // 2048
        juce::dsp::FFT fft { kOrder };
        double sampleRate { 48000.0 };
        int fftSize { 1 << kOrder };
        int hop     { 1 };

        std::vector<float> window;      // Hann, used for BOTH analysis and synthesis
        std::vector<float> inFifo[2];   // newest fftSize input samples
        std::vector<float> outAcc[2];   // overlap-add accumulator
        std::vector<float> spec[2];     // 2*fftSize per channel — both must exist at
                                        // once, because the gain decision is stereo-linked
        int   pos { 0 };
        float outScale { 1.0f };        // COLA + FFT round-trip normalisation combined

        // --- per-tone dynamics ------------------------------------------------
        // State is indexed by BIN rather than by a tracked partial identity. Tracking
        // partials across frames (birth / continuation / death) is the fragile part of
        // sinusoidal modelling — it drops notes, swaps neighbours, and every failure is
        // audible. Bin-indexed state gives the same four things without any of that:
        // no shared gain, per-tone independence, a frequency-aware detector, and
        // per-tone transient protection.
        // STEREO-LINKED, deliberately: ONE gain per tone, shared by both channels. Left
        // and right deciding independently would let a tone be compressed in one channel
        // and not the other, which walks the stereo image around. Everything else in this
        // plugin is stereo-linked for the same reason.
        int numBins { 1 };
        std::vector<float> binWeight;     // perceptual weight per bin (frequency-aware)
        std::vector<float> binGain;       // smoothed gain per tone (shared L/R)
        std::vector<float> binPrevMag;    // last frame's BAND level in dB (onset detection)
        std::vector<int>   binAge;        // frames since this tone's onset
        std::vector<int>   binAgeLimit;   // how long THIS onset stays protected
        std::vector<float> binMag;        // this frame's magnitude
        std::vector<float> binWDb;        // ...weighted, in dB
        std::vector<float> binSpread;     // masking shadow cast by everything else
        std::vector<float> gainTmp;       // scratch for the frequency-domain smoothing

        // Masking spread slopes, per BIN but derived from a per-Bark slope, so the
        // shadow widens with frequency the way hearing actually does.
        std::vector<float> upSlope, downSlope;

        // Half-width of the ear's critical band at each bin, in bins. Used for BOTH the
        // level measurement and the gain smoothing, so the engine works at the resolution
        // the ear actually has instead of a fixed number of bins.
        std::vector<int>   bandHalf;
        std::vector<float> prefix;      // running energy sum, for O(1) band totals

        // Tonality: how predictably a bin's phase advances from frame to frame. A real
        // tone rotates by a fixed amount per hop; noise does not. Stored as the previous
        // complex value plus the expected rotation, so it can be measured with a few
        // multiplies instead of an atan2 per bin.
        std::vector<float> binPrevRe, binPrevIm;
        std::vector<float> rotRe, rotIm;
        std::vector<float> binTonal;
        float tonalC { 0.0f };

        // Attack/release coefficients are PER TONE, because in FUTURE they are set in
        // periods of each tone's own wave rather than in milliseconds. Rebuilt only
        // when the knobs actually move.
        std::vector<float> binAtkC, binRelC;
        std::vector<float> binRelFastC;   // SMART: the quicker coefficient for a tone
                                          // that has already died away
        // Running peak each tone is measured against — SMART uses it to tell whether a
        // tone has died away, AUTO to time how long it rings. Two versions, because the
        // engine switch offers both:
        //   FUTURE   binPeakMag  — the tone's own FFT bin, linear magnitude
        //   FUTURE 2 binPeakWDb  — the ear's critical band around it, in dB
        // Per bin, a Rayleigh-distributed noise magnitude sits 6 dB under its own peak
        // about one frame in twenty, so SMART fires its fast release on hats and air with
        // nothing having happened. The band average cancels that jitter. It also
        // compresses harder, which is why it is a mode and not a silent replacement.
        std::vector<float> binPeakMag;
        std::vector<float> binPeakWDb;
        float peakDropDb { 0.0f };        // how far the dB peak falls per frame

        // Has the level memory been seeded from a frame that actually had signal? Until
        // it has, the threshold would sit far below the music and everything would be
        // slammed at once — heard as a dip at the start of a track.
        bool lvlMemSeeded { false };
        // FUTURE 2: attack protection expressed in CYCLES of each tone's own wave, so a
        // 50 Hz fundamental is guarded for as many oscillations as a hi-hat is. Both are
        // floored at the frame-count constants, so nothing above ~100 Hz changes.
        std::vector<float> binAgeBase;   // base window, in frames, per bin
        std::vector<int>   binAgeMax;    // its ceiling, in frames, per bin
        float timingAtk01 { -1.0f }, timingRel01 { -1.0f };
        double timingBpm { -1.0 };
        // Rolling estimate of how long tones take to fall away, fed by counting how many
        // frames pass between a tone's onset and it dropping below the release threshold.
        float decaySec { 0.25f };
        std::vector<int> binSinceOnset;

        // Loudest INPUT tone (per bin), measured in analyseFrame before any gain is
        // applied. FUTURE weights AUTO's decay votes against it; FUTURE 2 uses the
        // loudest critical BAND instead, so it does not need this.
        float inputLoudest { 1.0e-9f };
        bool  smartRel { false };
        void rebuildTiming();
        // Picks this tone's envelope coefficient: attack when the gain is falling,
        // release when it is rising — and in SMART, the fast release once the tone
        // has dropped away from its own peak.
        float envCoeff (int b, float target, float current) const;

        // --- BOND: tones that always sound together share a gain ---------------
        // The engine otherwise treats every tone as independent, but music does not
        // work that way — a kick and its sub are ONE source. Compressed separately
        // their balance shifts from hit to hit, which the ear hears as the low end
        // wandering. BOND measures which tones consistently start in the SAME frame
        // and locks their gains together.
        //
        // What makes it different from multiband: the grouping comes from the material,
        // not from fixed crossover points. Multiband decides "what belongs together" by
        // frequency, always the same way; this decides by co-occurrence in time, which
        // is how music is actually built.
        //
        // Bonding the WRONG things would bring back exactly the pumping this engine
        // exists to remove, so a pair must clear four separate guards — see Config.h.
        static constexpr int kMaxAnchors = 8;
        bool  bondOn { false };
        int   anchorBin[kMaxAnchors] {};        // which bin each anchor currently is
        int   anchorCount { 0 };
        bool  anchorOnset[kMaxAnchors] {};      // did it fire this frame?
        std::vector<char> binOnset;             // captured BEFORE binPrevMag is
                                                // overwritten, or every onset reads false
        float bondScore[kMaxAnchors][kMaxAnchors] {};   // agreement, 0..1
        int   bondEvents[kMaxAnchors][kMaxAnchors] {};  // how many onsets seen together
        void  updateBonds();
        void  applyBonds();

        // Which bin the harmonic linking currently calls the fundamental. Held between
        // frames with hysteresis: picked fresh every frame it would jump between the
        // kick and whatever bass note is playing, and every jump moves the whole set of
        // linked harmonics — a discontinuity in the gain.
        int prevF0 { 0 };

        float amount01 { 0.0f };
        float upward01 { 0.0f };
        // Per-tone AMOUNT scale from the ALL MIX band knobs. All 1.0 when ALL MIX is off,
        // so the global amount applies everywhere exactly as before.
        std::vector<float> binAmtScale;
        float lvlMemDb { -60.0f };        // slow level memory the threshold hangs off
        float atkC { 0.0f }, relC { 0.0f }, memC { 0.0f };   // per-FRAME coefficients
        // Decay of the per-tone peak follower. Separate from memC on purpose: memC sets
        // the THRESHOLD, this one decides when SMART calls a tone dead and which note
        // AUTO picks. Sharing one number tied two unrelated behaviours together.
        float peakC { 0.0f };
        float maxRedDb { 0.0f };
    };
    PartialEngine partialEng;

    // --- Constant-latency compensation ---------------------------------------
    // FUTURE needs a whole analysis window; CLASSIC and MODERN need nothing. Reporting
    // a DIFFERENT latency per engine meant that switching engines mid-playback forced
    // the host to re-align the track, which is heard as the sound dropping out — the
    // real cause of the dip, and something no amount of buffer priming can fix.
    //
    // So the plugin now always reports the SAME latency and simply delays the other two
    // engines to match. The host never re-syncs, both paths run in the same timeframe,
    // and switching is seamless. The cost is that CLASSIC and MODERN also carry the
    // window's delay — irrelevant for a mastering plugin, which is what this is.
    std::vector<float> engDelayL, engDelayR;
    int engDelayPos { 0 }, engDelayLen { 0 };

    // --- Spectral (multiband) limiter: IRC III / IRC IV style ----------------
    // Each band has its own limiter, so a peak in one band ducks ONLY that band ->
    // the mix stays open and pushes louder without global pumping. 3 bands = the
    // original IRC-III mode; 6 bands + psychoacoustic weighting = the IRC-IV mode
    // (finer splitting + limiting harder where the ear masks it, softer where the
    // ear is sensitive). The number of ACTIVE bands is chosen per-block.
    static constexpr int kSpecBands = 6;               // max bands (IRC IV)
    PeakLimiter specLim[kSpecBands];
    // 5 cascaded LP split points give up to 6 bands. Split set is chosen by mode.
    juce::dsp::LinkwitzRileyFilter<float> specXover[kSpecBands - 1][2];
    // MODERN engine only: phase compensation for the spectral split. A band peeled
    // off at split i must still pass the ALLPASS of every LATER split, otherwise the
    // bands don't sum flat and the crossover frequencies (90/200 Hz — the kick's
    // fundamental!) get ±3 dB notches even with no limiting. specAp[b][s][ch] is the
    // allpass of split s applied to band b. Mirrors what the 4-band comp already does.
    juce::dsp::LinkwitzRileyFilter<float> specAp[kSpecBands - 1][kSpecBands - 1][2];

    // Output-meter running states (audio thread).
    float meterRmsSq  { 1.0e-9f };  // ~300 ms RMS energy of the output

    // ---- BS.1770 LUFS metering ---------------------------------------------
    // A transposed-direct-form-II biquad (double precision for accuracy). The
    // K-weighting is TWO of these in series per channel: a high-shelf then a
    // high-pass (RLB). Coefficients are computed for the actual sample rate.
    struct Biquad
    {
        double b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
        double z1{0}, z2{0};
        inline double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0.0; }
    };
    Biquad kShelfL, kShelfR, kHpfL, kHpfR;   // K-weighting stages per channel
    void designKWeighting (double sr);        // set coeffs for the sample rate

    // --- ITU-R BS.1770-4 loudness meter ------------------------------------
    // The spec is built on 100 ms blocks of K-weighted mean-square power:
    //   MOMENTARY  = mean of the last 4 blocks   (400 ms), rectangular window
    //   SHORT-TERM = mean of the last 30 blocks  (3 s)
    //   INTEGRATED = gated mean of ALL blocks: absolute gate at -70 LUFS, then a
    //                relative gate 10 LU below the ungated mean (this is the number
    //                streaming platforms normalise to).
    // The old code used a one-pole (exponential) average, whose effective window is
    // ~2x too long and never forgets — it read differently from any compliant meter.
    static constexpr int kLufsBlockMs   = 100;
    static constexpr int kMomentaryBlk  = 4;    // 400 ms
    static constexpr int kShortTermBlk  = 30;   // 3 s
    int    lufsBlockLen  { 4800 };  // samples per 100 ms block (set in prepare)
    int    lufsBlockPos  { 0 };     // sample counter inside the current block
    double lufsBlockAcc  { 0.0 };   // K-weighted energy accumulated in this block
    std::vector<double> lufsRecent; // ring of the last 30 block powers (short-term)
    int    lufsRecentPos { 0 };
    int    lufsRecentFilled { 0 };
    // Gated integration: running sums over every block that passed the absolute gate.
    double lufsIntSum    { 0.0 };   // sum of block powers above the absolute gate
    int    lufsIntCount  { 0 };
    std::vector<float> lufsGateBlocks;  // blocks kept for the relative-gate pass
    void   pushLufsBlock (double meanSquare);   // called once per completed block

    // LUFS-GAIN mode: a slowly-moving make-up gain (linear) that pushes the measured
    // loudness toward a target. Updated once per block; smoothed so it never pumps.
    // prevLufsGain lets us reset to 0 dB (soft ramp) the moment the mode turns on.
    float lufsGainDb { 0.0f };
    bool  prevLufsGain { false };

    // ADAA clipper state: previous input sample per channel (for antiderivative
    // anti-aliasing — the "clean" hard-clip used by pro clippers). DOUBLE precision:
    // float32 cancellation at high oversampling caused audible hiss (see clipmath).
    double adaaPrevX[2] { 0.0, 0.0 };
    // DC-blocker state (post-clip high-pass, removes tiny DC from asymmetry).
    float dcX1[2] { 0.0f, 0.0f }, dcY1[2] { 0.0f, 0.0f };
    // Post-downsample CATCH gain (per channel) + its release coefficient. The OS
    // clipper leaves a hair of headroom (cfg::clipDownGuard) so the only thing that
    // can arrive above the ceiling here is downsampler ripple; this dips the gain
    // smoothly to remove it instead of hard-clamping (a clamp at base rate is an
    // un-oversampled clip = aliasing, exactly what the 16x exists to avoid).
    float dipG[2] { 1.0f, 1.0f };
    // Initialised to a WORKING value, not 0. prepareToPlay computes the real one, but
    // a host may call reset() first — and with a coefficient of zero this gain, once it
    // dips, never recovers, which silences the plugin permanently.
    float dipRelCoeff { 0.0606f };   // ~16-sample release

    // Oversampled hard clipper. One Oversampling object per selectable factor
    // (1x..64x = index 0..6); the Oversample parameter picks which one is used,
    // so switching is just an index change — no realtime allocation.
    static constexpr int kNumOsFactors = 7; // 1,2,4,8,16,32,64
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling[kNumOsFactors];
    // ADAA state for the 2nd-order scheme (needs the previous TWO input samples).
    double adaaPrevX2[2] { 0.0, 0.0 };

    // INPUT low-band splitter (@120 Hz): when INPUT is pushed above 0, the sub/low
    // band is lifted 1.5 dB LESS than the rest, so pushing level doesn't over-drive
    // or smear the bass. One LR lowpass per channel isolates the low band to trim.
    juce::dsp::LinkwitzRileyFilter<float> inLowXover[2];

    // Smoothed parameter followers (avoid zipper noise on the single knob).
    juce::LinearSmoothedValue<float> amountSmoothed;
    juce::LinearSmoothedValue<float> inGainSmoothed;
    juce::LinearSmoothedValue<float> outGainSmoothed;  // OUTPUT trim (attenuate only)

    double currentSampleRate { 44100.0 };

    // Previous toggle states — to report PDC only on change and flush on switch.
    int  prevReportedLatency { -1 };
    bool prevLookahead { true };
    bool prevSpecLimit { true };
    bool prevAllMix    { false };
    bool prevIrc4      { false };
    bool prevModernEngine { false };   // ENGINE toggle edge-detect (flush allpasses)
    int  prevClipMode  { 1 };          // CLIP|HI-Q|MASK edge-detect (flush the mask stage)
    int  prevEngineIdx { 0 };          // CLASSIC|MODERN|FUTURE edge-detect
    int  prevLimMode   { 1 };          // SMART|IRC IV|PHASE edge-detect
    // Bypass crossfade (0 = processed, 1 = dry) to avoid clicks on toggle.
    float bypassFade { 0.0f };
    // All-Mix crossfade (0 = single-band, 1 = 4-band) — click-free mode switch.
    float allMixFade { 0.0f };

    // Dry delay line for MIX (aligns the dry copy to the processed latency so the
    // blend doesn't comb-filter). Sized to the max lookahead latency.
    std::vector<float> dryDelayL, dryDelayR;
    int dryDelayPos { 0 }, dryDelayLen { 0 };
    juce::AudioBuffer<float> dryCopy;   // reused per block (sized in prepareToPlay) —
                                        // avoids a heap alloc on the audio thread.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HouseCompAudioProcessor)
};
