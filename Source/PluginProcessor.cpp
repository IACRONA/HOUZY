#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Config.h"
#include <cmath>   // std::isfinite for the input NaN/Inf guard

//==============================================================================
namespace ParamID
{
    constexpr auto amount   = "amount";    // main knob 0..100 %
    constexpr auto inGain   = "ingain";    // INPUT gain (pre-comp) dB -> drives into comp
    // OUTPUT trim: ATTENUATION ONLY (-24..0 dB). It can never boost, so it can
    // never create peaks after the clipper — it only turns the finished, already
    // peak-safe signal down.
    constexpr auto outGain  = "outgain";   // OUTPUT trim dB (<= 0, post-clip)
    constexpr auto autoGain = "autogain";  // auto make-up on/off (default on)
    // NOTE: no OUTPUT gain. Raising level AFTER the comp would create peaks the
    // comp can't catch (only the clipper would, = distortion). All level lift
    // happens at INPUT, so new peaks go THROUGH the compressor.
    constexpr auto clip     = "clip";      // hard-clip on/off
    constexpr auto ceiling  = "ceiling";   // clip ceiling dBFS (default -0.1)
    constexpr auto atkSlew  = "atkslew";   // attack slew speed (dB/ms)
    constexpr auto relSlew  = "relslew";   // release slew speed (dB/ms)
    constexpr auto osFactor = "osfactor";  // clipper oversampling: index into OS list
    constexpr auto bypass   = "bypass";    // true = pass input through untouched
    constexpr auto lookahead= "lookahead"; // limiter lookahead on/off (adds latency)
    constexpr auto specLimit= "speclimit"; // spectral (multiband) limiter on/off

    // --- New optional modes (defaults keep the CURRENT sound) ---------------
    constexpr auto character= "character"; // -1..+1 tone/feel; 0 = current sound
    constexpr auto smart    = "smart";     // psychoacoustic limiter release (off=current)
    constexpr auto lufsGain = "lufsgain";  // auto-gain by measured LUFS (off=static make-up)

    // --- Clipper character (2026 upgrades). Default = hard/OFF = today's sound. ---
    constexpr auto clipShape= "clipshape"; // 0=hard brickwall .. 1=soft warm
    constexpr auto clipHiQ  = "cliphiq";    // LEGACY (superseded by clipMode) — kept in the
                                            // layout so old presets still load; on load its
                                            // value seeds clipMode, then it is ignored.

    // CLIPPER MODE:
    //   0 (retired) = the old plain ADAA1 CLIP. Kept as an index so presets that stored
    //                 it still load; it is no longer offered on the panel and now
    //                 behaves as HI-Q.
    //   1 HI-Q      = ADAA2, the clean conventional clipper.
    //   2 ACR       = PC-CFR + ADAA2. Peaks are cancelled by subtracting a band-limited
    //                 pulse whose spectrum is placed under the masking curve, instead of
    //                 being sliced broadband. Same flat tops, but the kick stops chopping
    //                 the highs on every beat.   <- DEFAULT
    constexpr auto clipMode = "clipmode";

    // Cascade limiter (Sanfilippo-style low-distortion smoothing). Off = current.
    constexpr auto cascade  = "cascade";

    // IRC IV: 6-band spectral limiter with psychoacoustic weighting (vs the 3-band
    // IRC-III default). Off = current 3-band spectral. Needs SPECTRAL on.
    constexpr auto irc4     = "irc4";       // LEGACY bool, superseded by limMode below

    // LIMITER MODE — how peaks are handled:
    //   0 SMART  = 3-band spectral limiter
    //   1 IRC IV = 6-band spectral limiter        <- default
    //   2 PHASE  = IRC IV plus crest dispersion. Peaks are lowered by spreading the
    //              tones' PHASES instead of by turning the gain down, so the loudness
    //              INPUT and AMOUNT ask for costs no compression at all.
    constexpr auto limMode  = "limmode";

    // SWAP: order of the final two stages. false = LIMITER -> CLIPPER (default,
    // clipper last = flat tops). true = CLIPPER -> LIMITER (limiter last = smoother).
    constexpr auto clipOrder= "cliporder";

    // 4-band "All Mix Comp" mode: when on, the single AMOUNT is frozen and these
    // four per-band amounts drive compression of each frequency range instead.
    constexpr auto allMix   = "allmix";    // toggle: 4-band mode on/off
    constexpr auto compLow   = "complow";   // 0..200 Hz   compression amount %
    constexpr auto compLoMid = "complomid"; // 200..1500   compression amount %
    constexpr auto compHiMid = "comphimid"; // 1500..7000  compression amount %
    constexpr auto compHigh  = "comphigh";  // 7000..20000 compression amount %

    // ENGINE: CLASSIC = the original sound, untouched. MODERN = the same chain with
    // the sonic corrections applied (flat-summing spectral bands, detector HPF,
    // smoothed gain envelope, soft reduction cap, cleaner limiter smoothing).
    constexpr auto engine   = "engine";     // LEGACY bool (superseded by engineMode);
                                            // kept so old presets load, then seeds engineMode.

    // ENGINE MODE — the three engines:
    //   0 CLASSIC = the original sound, untouched   <- default
    //   1 MODERN  = the same chain with the sonic corrections applied
    //   2 FUTURE  = partial-based engine. The signal is taken apart into its
    //               individual tones and put back together, so dynamics can later be
    //               applied per tone instead of through one shared gain. Right now it
    //               only decomposes and reassembles (no compression yet) — that is the
    //               transparency test the whole approach stands or falls on.
    constexpr auto engineMode = "enginemode";

    // FUTURE only: lift QUIET tones (reverb tails, air, detail that normally sinks).
    // 0 = off. Safe per-tone in a way broadband upward compression is not — with no
    // shared gain it can only lift tones that are actually there.
    constexpr auto upComp = "upcomp";

    // FUTURE only: how the release decides when to let go.
    //   false BEAT  = exactly the beat fraction the RELEASE knob asks for.
    //   true  SMART = that fraction is a MAXIMUM; a tone that has already died away is
    //                 released early instead of holding an empty pause.
    constexpr auto relSmart = "relsmart";

    // FUTURE only: BOND ties together tones that consistently start in the same frame
    // (a kick and its sub are one source), so their balance stops shifting hit to hit.
    // false = FREE (every tone independent, as before).
    constexpr auto bond = "bond";

    // FUTURE only: the RELEASE length as a NOTE VALUE. A separate parameter from
    // relSlew so MODERN keeps its dB/ms knob untouched and old presets still load.
    constexpr auto relNote = "relnote";

    // FUTURE only: let the engine pick the release note itself, from how fast the
    // material's tones actually decay. Only RELEASE gets this — ATTACK is already
    // automatic per tone (it scales with each tone's own frequency), so its knob only
    // sets taste, and no automatic value can decide taste for you.
    constexpr auto relAuto = "relauto";

    // FUTURE only: ATTACK expressed in CYCLES of each tone's own wave. A separate
    // parameter from atkSlew so MODERN keeps its dB/ms knob and old presets still load,
    // and so the number on screen means what the caption says.
    constexpr auto atkCycles = "atkcycles";
}

//==============================================================================
// RELEASE note values for FUTURE, longest first. Straight, then TRIPLET (T) and
// DOTTED (.) so swung and half-step feels are reachable without hunting for them.
// The knob CLICKS between these — you land on the grid instead of guessing at it.
namespace relnote
{
    struct Entry { const char* name; float beats; };

    // 1 beat = a quarter note. Triplet = 2/3 of the straight value, dotted = 1.5x.
    static const Entry table[] =
    {
        { "1/1",   4.0f      }, { "1/2.",  3.0f      }, { "1/2",   2.0f      },
        { "1/2T",  4.0f/3.0f }, { "1/4.",  1.5f      }, { "1/4",   1.0f      },
        { "1/4T",  2.0f/3.0f }, { "1/8.",  0.75f     }, { "1/8",   0.5f      },
        { "1/8T",  1.0f/3.0f }, { "1/16.", 0.375f    }, { "1/16",  0.25f     },
        { "1/16T", 1.0f/6.0f }, { "1/32",  0.125f    }
    };
    static constexpr int count = (int) (sizeof (table) / sizeof (table[0]));

    inline juce::StringArray names()
    {
        juce::StringArray s;
        for (int i = 0; i < count; ++i) s.add (table[i].name);
        return s;
    }
    inline float beatsAt (int i) { return table[juce::jlimit (0, count - 1, i)].beats; }
}

//==============================================================================
// STAGE 1 — single-band, stereo-linked auto-leveling compressor.
//
//  Goal: even out the MACRO dynamics (verse vs chorus) toward a stable auto
//  level, WITHOUT flooring loud sections and WITHOUT pumping per-hit. The actual
//  "flat tops" come from the clipper later — this stage just feeds it an even,
//  predictable signal.
//
//  Logic:
//   - RMS detector (cfg::compRmsMs) = how loud we are now.
//   - autoLvlDb = a VERY slow running level (cfg::levelMemoryMs) => the threshold
//     reference. It barely moves, so the compression is steady (GR doesn't gulp).
//   - threshold = autoLvlDb - cfg::thresholdOffsetDb (sits just under the level so
//     the body of the track is gently compressed).
//   - reduction is capped at maxReductionDb, so even if a chorus jumps in before
//     the slow level catches up, we never crush it to the floor.
//   - slew (attack/release in dB/ms) smooths the gain so there are no clicks.
//==============================================================================
void HouseCompAudioProcessor::Compressor::prepare (double sr)
{
    // Guard against a host calling prepareToPlay(0, 0) during scanning: a zero rate
    // would make the exp() coefficients NaN and poison the detector permanently.
    sampleRate = juce::jmax (8000.0, sr);
    rmsSq     = 1.0e-6f;
    peakEnv   = 0.0f;
    autoLvlDb = -18.0f;
    gainDb    = 0.0f;
    // One-time smoothing coefficients (were std::exp per sample — same values).
    rmsCoeff  = std::exp (-1.0f / (0.001f * cfg::compRmsMs     * (float) sampleRate));
    pkCoeff   = std::exp (-1.0f / (0.030f                      * (float) sampleRate));
    slowCoeff = std::exp (-1.0f / (0.001f * cfg::levelMemoryMs * (float) sampleRate));
    fastCoeff = std::exp (-1.0f / (0.001f * cfg::memoryRiseMs  * (float) sampleRate));

    // MODERN: 1-pole sidechain HPF for the detector (cfg::scHpfHz).
    {
        const float fc = juce::jlimit (10.0f, (float) sampleRate * 0.45f, cfg::scHpfHz);
        scHpfCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);
        scHpfX1 = scHpfY1 = 0.0f;
    }
    // MODERN: gain-envelope smoother. Short (sub-ms) so it rounds the slew corners
    // WITHOUT changing the attack/release times the user set.
    gainSmCoeff = std::exp (-1.0f / (0.001f * cfg::gainSmoothMs * (float) sampleRate));
    gainSm1 = gainSm2 = 0.0f;
}

float HouseCompAudioProcessor::Compressor::process (float detector, float amount01,
                                                    float ratio, float attackDbPerMs,
                                                    float releaseDbPerMs,
                                                    float maxReductionDb, float& outGrDb,
                                                    float threshScale)
{
    // MODERN: sidechain HPF on the DETECTOR ONLY (audio is untouched). Without it
    // the kick's sub energy dominates the RMS and pulls the whole band down on
    // every beat — the classic "bass over-compressed, highs untouched" problem.
    if (modern && useScHpf)
    {
        const float x = detector;
        scHpfY1  = scHpfCoeff * (scHpfY1 + x - scHpfX1);   // 1-pole highpass
        scHpfX1  = x;
        detector = std::abs (scHpfY1);
    }

    // RMS level (energy). Coeff cached in prepare() (was std::exp per sample).
    rmsSq = rmsCoeff * rmsSq + (1.0f - rmsCoeff) * (detector * detector);
    const float rms = std::sqrt (rmsSq);
    const float envDb = juce::Decibels::gainToDecibels (rms + 1.0e-9f);

    // CREST FACTOR: fast peak follower vs RMS. crest = peak/RMS. Punchy/transient
    // material has a HIGH crest; steady material LOW. We use it to ADAPT the
    // attack/release so the comp reacts musically on its own.
    // Slow decay (~30 ms) so crest reflects the SECTION's punch, not the audio
    // waveform itself — a 0.5 ms decay dipped every bass cycle and modulated the
    // gain at audio rate (extra harmonics on the low end).
    peakEnv = juce::jmax (detector, pkCoeff * peakEnv);    // ~30ms, coeff cached in prepare()
    const float crest = (rms > 1.0e-6f) ? (peakEnv / rms) : 1.0f;
    // crest ~1 (steady) .. ~4+ (very transient). Map to 0..1.
    const float crest01 = juce::jlimit (0.0f, 1.0f, (crest - 1.0f) / 3.0f);

    // Adapt times: punchy (high crest) -> SOFTER attack (let transient through) &
    // SLOWER release (don't pump). Steady -> snappier both ways (tighter glue).
    const float atkAdapt = juce::jmap (crest01, 0.0f, 1.0f, 1.0f, 0.5f);  // up to 2x softer attack
    const float relAdapt = juce::jmap (crest01, 0.0f, 1.0f, 1.0f, 0.6f);  // up to ~1.7x slower release

    // TRANSIENT-ADAPTIVE auto level (the stable threshold reference). Smart:
    //   - when the signal sits NEAR the remembered level -> memory is SLOW
    //     (levelMemoryMs) so the threshold stays put and the bass stays even;
    //   - when the level SUDDENLY jumps (> memoryBigJumpDb, e.g. a section change,
    //     a drop, or switching AUTO<->LUFS) -> memory goes FAST (memoryRiseMs) so it
    //     catches up in tens of ms instead of several seconds.
    // This removes the slow "catch-up" lag without sacrificing steady-state calm.
    // slowCoeff / fastCoeff cached in prepare() (were std::exp per sample).
    const float jump = std::abs (envDb - autoLvlDb);                 // how far off we are
    const float fastAmt = juce::jlimit (0.0f, 1.0f,
                              (jump - cfg::memoryBigJumpDb) / cfg::memoryBigJumpDb);
    // Blend slow->fast the bigger the jump; small wobble stays slow (even bass).
    const float lvlCoeff = slowCoeff + (fastCoeff - slowCoeff) * fastAmt;
    autoLvlDb = lvlCoeff * autoLvlDb + (1.0f - lvlCoeff) * envDb;

    // CHARACTER scales how deep below the auto-level the threshold sits (>1 = jams
    // more of the body into compression, <1 = only the loud peaks).
    const float thresholdDb = autoLvlDb - cfg::thresholdOffsetDb * threshScale;
    const float over = envDb - thresholdDb;             // dB above threshold

    // SOFT KNEE: instead of a hard corner at the threshold, the amount of "excess"
    // that gets compressed ramps in smoothly across a knee width. This makes the
    // GR fade in gently (no micro-stepping on the body of the track), same depth.
    const float knee = cfg::compKneeDb;
    float effOver;                                       // effective dB over the curve
    if (over <= -knee * 0.5f)
        effOver = 0.0f;                                 // fully below knee -> no comp
    else if (over >= knee * 0.5f)
        effOver = over;                                 // fully above knee -> linear
    else
    {
        // quadratic transition in [-knee/2, +knee/2]
        const float x = over + knee * 0.5f;             // 0..knee
        effOver = (x * x) / (2.0f * knee);
    }

    float desiredGrDb = 0.0f;
    if (effOver > 0.0f)
    {
        const float removed = effOver - (effOver / ratio);
        if (modern && maxReductionDb > 0.0f)
        {
            // MODERN: SOFT cap. The hard jmin() put a corner in the transfer curve —
            // above it the ratio jumped to infinity, so the GR flat-topped. tanh()
            // approaches the same ceiling smoothly (identical below the knee).
            desiredGrDb = -maxReductionDb * std::tanh ((removed * amount01) / maxReductionDb);
        }
        else
        {
            desiredGrDb = -juce::jmin (removed * amount01, maxReductionDb);
        }
    }

    // Slew the gain toward the target (dB per sample), times adapted by crest.
    const float msPerSample = 1000.0f / (float) sampleRate;
    const float atkStep = attackDbPerMs  * atkAdapt * msPerSample;
    const float relStep = releaseDbPerMs * relAdapt * msPerSample;
    const float diff = desiredGrDb - gainDb;
    if (diff < 0.0f) gainDb += juce::jmax (diff, -atkStep);  // attack (more reduction)
    else             gainDb += juce::jmin (diff,  relStep);  // release (back toward 0)

    // MODERN: run the slewed gain through TWO one-pole stages. The slew itself sets
    // the speed (unchanged); this only rounds the sharp corners where the envelope
    // hits its target or flips attack<->release. A discontinuous derivative on a
    // multiplicative gain is a broadband click — this removes it, so the low end
    // stays clean while the compression amount/timing is identical.
    float outDb = gainDb;
    if (modern)
    {
        gainSm1 = gainSmCoeff * gainSm1 + (1.0f - gainSmCoeff) * gainDb;
        gainSm2 = gainSmCoeff * gainSm2 + (1.0f - gainSmCoeff) * gainSm1;
        outDb   = gainSm2;
    }

    outGrDb = outDb;
    return juce::Decibels::decibelsToGain (outDb);
}

//==============================================================================
// PeakLimiter — stereo-linked limiter with TRUE-PEAK detection and LOOKAHEAD.
//==============================================================================
void HouseCompAudioProcessor::PeakLimiter::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    // Two release stages: a fast one (grabs quick recovery) and a slow one
    // (keeps steady on sustained bass). Output gain = max(fast, slow) so the
    // gain doesn't ripple down with every bass cycle => cleaner low end.
    relFastCoeff = std::exp (-1.0f / (0.001f * cfg::limiterRelFastMs * (float) sampleRate));
    relSlowCoeff = std::exp (-1.0f / (0.001f * cfg::limiterRelSlowMs * (float) sampleRate));
    holdSamples  = juce::jmax (0, (int) (0.001f * cfg::limiterHoldMs * (float) sampleRate));

    // CASCADE mode coeffs. Each of the N one-poles uses a corrected cutoff so the
    // SERIES still gives the intended attack/release time (Zavalishin cutoff
    // correction: split the time across the stages). Attack short, release medium.
    {
        const float atkMs = 1.5f, relMs = 90.0f;
        // CLASSIC used 1/N, which makes the cascade ~2x FASTER than the label says
        // (N one-poles scale as sqrt(N), not N) — the limiter released in ~45 ms,
        // fast enough to ripple on a 50 Hz kick. MODERN uses the correct 1/sqrt(N).
        const float corrClassic = 1.0f / (float) juce::jmax (1, (int) kCascadeStages);
        const float corrModern  = 1.0f / std::sqrt ((float) juce::jmax (1, (int) kCascadeStages));
        const float corr = modern ? corrModern : corrClassic;
        cascAtkCoeff = std::exp (-1.0f / (0.001f * atkMs * corr * (float) sampleRate));
        cascRelCoeff = std::exp (-1.0f / (0.001f * relMs * corr * (float) sampleRate));
        for (auto& c : casc) c = 1.0f;
    }

    // Lookahead time from Config (e.g. 2 ms).
    laSamples = juce::jmax (1, (int) (0.001f * cfg::lookaheadMs * (float) sampleRate));
    dlL.assign ((size_t) laSamples, 0.0f);
    dlR.assign ((size_t) laSamples, 0.0f);
    gEnvMin.prepare (laSamples);
    // MODERN: window minimum of the RAW target gain + its smoother.
    tgtMin.prepare (laSamples);
    mSmCoeff = std::exp (-1.0f / (0.001f * juce::jmax (0.05f, cfg::lookaheadMs * 0.5f)
                                        * (float) sampleRate));
    flush();
    juce::ignoreUnused (maxBlock);
}

void HouseCompAudioProcessor::PeakLimiter::flush()
{
    envFast = envSlow = 1.0f;
    holdCounter = 0;
    prevL = prevR = 0.0f;
    prev2L = prev2R = 0.0f;      // was missed: stale history gave one bad TP estimate
    dlPos = 0;
    std::fill (dlL.begin(), dlL.end(), 0.0f);
    std::fill (dlR.begin(), dlR.end(), 0.0f);
    // An empty window and a window pre-filled with 1.0 give the same minimum, because
    // the gain can never exceed 1.0 — so this stays equivalent to the old fill.
    gEnvMin.reset();
    tgtMin.reset();
    mSm1 = mSm2 = 1.0f;
    for (auto& c : casc) c = 1.0f;
}

void HouseCompAudioProcessor::PeakLimiter::setLookahead (bool on) { lookaheadOn = on; }

int HouseCompAudioProcessor::PeakLimiter::latencySamples() const
{
    return lookaheadOn ? laSamples : 0;
}

void HouseCompAudioProcessor::PeakLimiter::process (float& l, float& r,
                                                    float ceilingLin, bool truePeak)
{
    // --- TRUE PEAK: 4-point (Catmull-Rom) inter-sample estimate at the halfway
    // point. Honestly catches peaks that hide between samples (esp. highs), with
    // no fudge factor. Uses a 2-sample history per channel.
    float peakL = std::abs (l), peakR = std::abs (r);
    if (truePeak)
    {
        // midpoint of a cubic through [prev2, prev1, cur, cur] ~ Catmull-Rom @0.5
        auto midPoint = [] (float x0, float x1, float x2)
        {
            // p(0.5) for Catmull-Rom with points (x0,x1,x2,x2)
            return 0.5f * (x1 + x2) + 0.125f * (x2 - x0);
        };
        peakL = juce::jmax (peakL, std::abs (midPoint (prev2L, prevL, l)));
        peakR = juce::jmax (peakR, std::abs (midPoint (prev2R, prevR, r)));
    }
    prev2L = prevL; prev2R = prevR;
    prevL = l; prevR = r;
    const float peak = juce::jmax (peakL, peakR);

    // Target gain to bring the (true) peak to the ceiling.
    float targetGain = 1.0f;
    if (peak > ceilingLin) targetGain = ceilingLin / peak;

    float gainNow;
    if (cascadeMode)
    {
        // --- CASCADE ballistics (Sanfilippo-style low-distortion smoothing) -------
        // The target gain passes through a SERIES of one-pole smoothers. Because
        // each stage smooths the previous stage's output, the final gain curve has
        // continuous derivatives (no attack "corner") => multiplying it onto the
        // audio adds far fewer harmonics than a single/parallel release. Attack uses
        // the fast coeff, release the slow one; a short hold still applies.
        float atk = cascAtkCoeff, rel = cascRelCoeff;
        if (smartMode) rel *= 0.85f;        // SMART: recover a touch faster
        // First stage tracks the target with attack/release asymmetry + hold.
        float in0 = targetGain;
        if (in0 < casc[0])
        {
            casc[0] = atk * casc[0] + (1.0f - atk) * in0;   // attack (down)
            holdCounter = holdSamples;
        }
        else if (holdCounter > 0) --holdCounter;
        else casc[0] = rel * casc[0] + (1.0f - rel) * in0;  // release (up)
        // Remaining stages: each smooths the previous (always same coeff => smooth).
        for (int s = 1; s < kCascadeStages; ++s)
        {
            const float k = (casc[s - 1] < casc[s]) ? atk : rel;
            casc[s] = k * casc[s] + (1.0f - k) * casc[s - 1];
        }
        gainNow = casc[kCascadeStages - 1];
    }
    else
    {
        // --- Default ballistics: instant attack, HOLD, then two-stage release. ---
        // SMART mode makes the release frequency-aware so highs breathe while lows
        // stay solid (a psychoacoustic release).
        const float smartFast = smartMode ? (relFastCoeff * 0.5f) : relFastCoeff;
        const float smartSlow = smartMode ? (relSlowCoeff * 0.85f) : relSlowCoeff;
        if (targetGain < envFast)
        {
            envFast = envSlow = targetGain;
            holdCounter = holdSamples;
        }
        else
        {
            if (holdCounter > 0)
                --holdCounter;
            else
            {
                envFast = smartFast * envFast + (1.0f - smartFast) * targetGain;
                envSlow = smartSlow * envSlow + (1.0f - smartSlow) * targetGain;
            }
        }
        // Max of the two = smooth but doesn't ripple with the bass waveform.
        gainNow = juce::jmax (envFast, envSlow);
    }

    if (! lookaheadOn)
    {
        lastGain = gainNow;
        l *= gainNow; r *= gainNow;
        return;
    }

    // --- LOOKAHEAD: delay audio; apply the MIN gain across the window so the
    // ducking starts BEFORE the peak arrives. ---
    float gMin;
    if (modern)
    {
        // MODERN order: min-scan the RAW target, THEN smooth. CLASSIC min-scanned an
        // already-smoothed envelope, which leaves rectangular corners in the gain
        // (a gain "notch" whose spectrum is a sinc => broadband grit, worst on bass).
        const float tMin = tgtMin.push (targetGain);
        // Two-stage smoother sized to the lookahead, so the gain reaches its minimum
        // exactly as the peak arrives, with a continuous derivative.
        mSm1 = mSmCoeff * mSm1 + (1.0f - mSmCoeff) * tMin;
        mSm2 = mSmCoeff * mSm2 + (1.0f - mSmCoeff) * mSm1;
        // Never let the smoothing overshoot above the instantaneous requirement.
        gMin = juce::jmin (mSm2, juce::jmin (gainNow, tMin));
        gEnvMin.push (gainNow);               // keep the CLASSIC window coherent
    }
    else
    {
        gMin = juce::jmin (gainNow, gEnvMin.push (gainNow));
    }

    const float outL = dlL[(size_t) dlPos];
    const float outR = dlR[(size_t) dlPos];
    dlL[(size_t) dlPos] = l;
    dlR[(size_t) dlPos] = r;
    dlPos = (dlPos + 1) % laSamples;

    lastGain = gMin;
    l = outL * gMin;
    r = outR * gMin;
}

//==============================================================================
HouseCompAudioProcessor::HouseCompAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout HouseCompAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::amount, 1 }, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f,   // default favourite
        AudioParameterFloatAttributes().withLabel ("%")));

    // INPUT gain: all level lift happens here, BEFORE the compressor, so any
    // new peaks are caught by the comp instead of slipping past it. Push this up
    // to drive a -6 dB signal toward 0; the comp + clipper keep the peak in line.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::inGain, 1 }, "Input",
        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,   // neutral on load
        AudioParameterFloatAttributes().withLabel ("dB")));

    // OUTPUT trim — ATTENUATION ONLY (max is 0 dB). Turning the finished signal
    // down can never introduce peaks; boosting after the clipper could, so the
    // range simply doesn't allow it.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::outGain, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 0.0f, 0.1f), 0.0f,     // 0 = unity
        AudioParameterFloatAttributes().withLabel ("dB")));

    // Auto gain: STATIC make-up re-leveling. Mutually exclusive with LUFS GAIN
    // (segmented switch). OFF by default — LUFS GAIN is the default make-up mode.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::autoGain, 1 }, "Auto Gain", false));

    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::clip, 1 }, "Clip", true));

    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::ceiling, 1 }, "Ceiling",
        NormalisableRange<float> (-3.0f, 0.0f, 0.01f), cfg::defaultCeilingDb,
        AudioParameterFloatAttributes().withLabel ("dBFS")));

    // ATTACK / RELEASE now control the gain SLEW SPEED (dB per ms): how fast the
    // compressor is allowed to pull down (attack) and recover (release). Higher =
    // faster/snappier; lower = smoother (less pumping, more even). This is the
    // single ballistics control — no separate "auto" switches.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::atkSlew, 1 }, "Attack",
        // MODERN / CLASSIC only — FUTURE has its own ATTACK knob in wave cycles.
        NormalisableRange<float> (0.5f, 24.0f, 0.1f), 20.0f,   // default favourite
        AudioParameterFloatAttributes().withLabel ("dB/ms")));
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::relSlew, 1 }, "Release",
        NormalisableRange<float> (0.2f, 12.0f, 0.1f), 7.0f,    // default favourite
        AudioParameterFloatAttributes().withLabel ("dB/ms")));

    // --- All Mix Comp: 4-band mode -----------------------------------------
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::allMix, 1 }, "All Mix Comp", false));

    auto bandAmt = [] (const char* id, const char* name)
    {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name,
            // Matches the AMOUNT default, so switching ALL MIX on does not change the
            // amount of compression until you actually move a band knob.
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f,
            AudioParameterFloatAttributes().withLabel ("%"));
    };
    layout.add (bandAmt (ParamID::compLow,   "Low"));
    layout.add (bandAmt (ParamID::compLoMid, "Lo-Mid"));
    layout.add (bandAmt (ParamID::compHiMid, "Hi-Mid"));
    layout.add (bandAmt (ParamID::compHigh,  "High"));

    // Clipper oversampling: 1x..64x. Higher = cleaner clipping (less aliasing)
    // but more CPU/latency.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { ParamID::osFactor, 1 }, "Oversample",
        juce::StringArray { "1x", "2x", "4x", "8x", "16x", "32x", "64x" }, 4));  // default 16x

    // Bypass: pass the input straight through (no processing). OFF by default.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::bypass, 1 }, "Bypass", false));

    // Lookahead: limiter peeks ahead to catch peaks smoothly (adds latency).
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::lookahead, 1 }, "Lookahead", true));

    // Spectral limiter: limit each band independently (IRC-style) so a peak in one
    // band doesn't duck the whole mix -> louder & cleaner. ON by default.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::specLimit, 1 }, "Spectral Limit", true));

    // --- New optional modes (defaults = the CURRENT sound) ------------------
    // CHARACTER: one bipolar knob for feel. 0 (centre) = exactly today's engine.
    // Left = softer/slower (more open), right = denser/harder (more glued).
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::character, 1 }, "Character",
        NormalisableRange<float> (-1.0f, 1.0f, 0.01f), -0.40f));   // default: softer side

    // SMART: psychoacoustic limiter release. OFF by default (IRC IV is the default
    // spectral-release mode; SMART and IRC IV are mutually exclusive in the UI).
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::smart, 1 }, "Smart", false));

    // LUFS GAIN: auto-level to a target loudness from the measured LUFS, instead of
    // the fixed static make-up. ON by default (the default make-up mode).
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::lufsGain, 1 }, "LUFS Gain", true));

    // CLIP SHAPE: 0 = hard brickwall (today's exact sound) .. 1 = soft/warm cubic.
    // Default 0 keeps the current sound; a knob lets you dial in warmth.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::clipShape, 1 }, "Clip Shape",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // ENGINE: LEGACY bool, superseded by engineMode. Kept in the layout so presets
    // from older versions still load; setStateInformation uses it to seed engineMode.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::engine, 1 }, "Engine", false));

    // ENGINE MODE. CLASSIC (value 0) is retired from the panel but kept in the
    // parameter so older presets still load and still sound the way they were saved.
    // The panel offers MODERN, FUTURE and FUTURE 2; FUTURE is the default.
    //
    // FUTURE reads each tone's onset and decay from the ear's CRITICAL BAND. It was
    // trialled beside the older per-bin version as "FUTURE 2" and won on listening, so it
    // is simply what FUTURE means now. Value 3 is kept as an alias of 2 so any project
    // saved during that trial still loads and sounds the same; it is off the panel.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { ParamID::engineMode, 1 }, "Engine Mode",
        juce::StringArray { "CLASSIC", "MODERN", "FUTURE", "FUTURE 2" }, 2));   // default FUTURE

    // RELEASE TYPE (FUTURE only): BEAT = exactly the beat fraction asked for,
    // SMART = that fraction as a maximum, letting a tone that already died away go early.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::relSmart, 1 }, "Release Smart", true));   // default SMART

    // RELEASE as a note value (FUTURE only). A choice, not a float, so the knob clicks
    // from one musical value to the next instead of stopping between them. Default 1/8.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { ParamID::relNote, 1 }, "Release Note",
        relnote::names(), 13));   // default 1/32

    // ATTACK in CYCLES (FUTURE only). Right = faster, matching the dB/ms knob's feel:
    // fewer cycles = the gain moves within a smaller part of the tone's own wave.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::atkCycles, 1 }, "Attack Cycles",
        NormalisableRange<float> (0.5f, 4.0f, 0.05f), 2.3f,
        AudioParameterFloatAttributes().withLabel ("cyc")));

    // AUTO release (FUTURE only): the engine picks the note value from the measured
    // decay. Off by default so the knob stays in charge until you hand it over.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::relAuto, 1 }, "Release Auto", true));   // default ON

    // BOND (FUTURE only). Default OFF, so the engine keeps behaving exactly as before
    // until it is switched on for testing.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::bond, 1 }, "Bond", false));

    // UPWARD (FUTURE only): lifts quiet tones. Default 0 = off, so it never changes
    // the sound until it is asked to.
    layout.add (std::make_unique<AudioParameterFloat>(
        ParameterID { ParamID::upComp, 1 }, "Upward",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // CLIP HI-Q: LEGACY. Superseded by clipMode, but kept in the layout so presets
    // saved by older versions still load without the host complaining. Its value is
    // used once, in setStateInformation, to seed clipMode when that's absent.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::clipHiQ, 1 }, "Clip Hi-Q", true));

    // CLIPPER MODE. Clipping itself is always active — you pick the flavour, never
    // on/off. Index 0 is the retired plain CLIP: the slot stays so old presets keep
    // loading, but the panel only offers HI-Q and ACR, and 0 is treated as HI-Q.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { ParamID::clipMode, 1 }, "Clip Mode",
        juce::StringArray { "CLIP", "HI-Q", "ACR" }, 2));   // default ACR

    // CASCADE: low-distortion cascaded-smoother limiter (Sanfilippo 2022). Off = the
    // current 2-stage limiter (keeps today's sound). On = cleaner bass when limiting hard.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::cascade, 1 }, "Cascade", true));

    // IRC IV: 6-band psychoacoustic spectral limiter (Ozone-Maximizer-like). Off =
    // the current 3-band spectral. Louder & cleaner at the limit. Needs SPECTRAL on.
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::irc4, 1 }, "IRC IV", true));

    // LIMITER MODE: SMART | IRC IV | PHASE. Default IRC IV — exactly today's sound.
    layout.add (std::make_unique<AudioParameterChoice>(
        ParameterID { ParamID::limMode, 1 }, "Limiter Mode",
        juce::StringArray { "SMART", "IRC IV", "PHASE" }, 1));

    // SWAP order: false = LIMITER then CLIPPER (default, flat tops); true = CLIPPER
    // then LIMITER (clipper shaves transients, limiter smooths after = gentler).
    layout.add (std::make_unique<AudioParameterBool>(
        ParameterID { ParamID::clipOrder, 1 }, "Swap Clip/Lim", false));

    return layout;
}

//==============================================================================
void HouseCompAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Pre-size the reusable dry-copy buffer so processBlock never allocates.
    dryCopy.setSize (2, samplesPerBlock, false, false, true);

    comp.prepare (sampleRate);
    limiter.prepare (sampleRate, samplesPerBlock);

    // Spectral limiter: up to 6 bands. 5 cascaded LP split points span the range
    // (sub / low / lo-mid / mid / hi-mid / air). The 3-band (IRC III) mode reuses a
    // subset of these same filters, so no separate filter bank is needed.
    for (auto& sl : specLim) sl.prepare (sampleRate, samplesPerBlock);
    {
        juce::dsp::ProcessSpec sp { sampleRate, (juce::uint32) samplesPerBlock, 1 };
        // Chosen so the 3-band (IRC III) mode can reuse indices {1,3} = 200 Hz &
        // 3 kHz EXACTLY (unchanged default sound), while all five give 6 IRC-IV bands.
        const float ssp[kSpecBands - 1] = { 90.0f, 200.0f, 700.0f, 3000.0f, 9000.0f };
        // Clamp cutoffs safely below Nyquist (protects low sample rates).
        auto safeF = [sampleRate] (float f) { return juce::jmin (f, (float) sampleRate * 0.45f); };
        for (int s = 0; s < kSpecBands - 1; ++s)
            for (int ch = 0; ch < 2; ++ch)
            {
                specXover[s][ch].prepare (sp);
                specXover[s][ch].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
                specXover[s][ch].setCutoffFrequency (safeF (ssp[s]));
            }

        // MODERN engine: allpass bank for flat band summing. Band b gets the allpass
        // of every split s > b (the splits it was peeled off BEFORE).
        for (int b = 0; b < kSpecBands - 1; ++b)
            for (int s = 0; s < kSpecBands - 1; ++s)
                for (int ch = 0; ch < 2; ++ch)
                {
                    specAp[b][s][ch].prepare (sp);
                    specAp[b][s][ch].setType (juce::dsp::LinkwitzRileyFilterType::allpass);
                    specAp[b][s][ch].setCutoffFrequency (safeF (ssp[s]));
                }
    }

    // 4-band crossover prep (split points from Config: 160 / 1500 / 7000 Hz).
    for (auto& c : bandComp) c.prepare (sampleRate);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    const float splits[3] = { cfg::bandSplit1Hz, cfg::bandSplit2Hz, cfg::bandSplit3Hz };
    for (int s = 0; s < 3; ++s)
        for (int ch = 0; ch < 2; ++ch)
        {
            xover[s][ch].prepare (spec);
            xover[s][ch].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            xover[s][ch].setCutoffFrequency (splits[s]);
        }
    // Phase-comp allpasses: LOW band -> AP@split2, AP@split3; LO-MID -> AP@split3.
    for (int ch = 0; ch < 2; ++ch)
    {
        apLowB[0][ch].prepare (spec); apLowB[0][ch].setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        apLowB[0][ch].setCutoffFrequency (splits[1]);
        apLowB[1][ch].prepare (spec); apLowB[1][ch].setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        apLowB[1][ch].setCutoffFrequency (splits[2]);
        apMidB[0][ch].prepare (spec); apMidB[0][ch].setType (juce::dsp::LinkwitzRileyFilterType::allpass);
        apMidB[0][ch].setCutoffFrequency (splits[2]);
    }

    // Build one oversampler per factor (1x..64x => 2^0..2^6). The Oversample
    // parameter selects which one runs, so switching costs nothing at runtime.
    // Constructed ONCE (lazily); on later prepareToPlay calls we only re-init, never
    // re-allocate — rebuilding 7 oversamplers every prepare was slow and churned
    // memory (a hitch the host felt on buffer/rate changes and teardown).
    for (int i = 0; i < kNumOsFactors; ++i)
    {
        if (oversampling[i] == nullptr)
            oversampling[i] = std::make_unique<juce::dsp::Oversampling<float>>(
                2, (size_t) i,   // factor = 2^i
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        oversampling[i]->reset();
        oversampling[i]->initProcessing ((size_t) samplesPerBlock);
    }

    // INPUT low-band splitter @120 Hz (per channel) for the -1.5 dB low trim.
    {
        juce::dsp::ProcessSpec sp1 { sampleRate, (juce::uint32) samplesPerBlock, 1 };
        for (int ch = 0; ch < 2; ++ch)
        {
            inLowXover[ch].prepare (sp1);
            inLowXover[ch].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            inLowXover[ch].setCutoffFrequency (120.0f);
        }
    }

    adaaPrevX2[0] = adaaPrevX2[1] = 0.0f;
    adaaPrevX[0] = adaaPrevX[1] = 0.0f;
    dcX1[0] = dcX1[1] = dcY1[0] = dcY1[1] = 0.0f;

    maskClip.prepare (sampleRate, samplesPerBlock);
    crestRot.prepare (sampleRate);
    disperser.prepare (sampleRate);
    partialEng.prepare (sampleRate, samplesPerBlock);

    // Delay line that holds CLASSIC/MODERN back to FUTURE's timing, so the reported
    // latency is the same in every engine and switching never makes the host re-sync.
    engDelayLen = juce::jmax (1, partialEng.latencySamples());
    engDelayL.assign ((size_t) engDelayLen, 0.0f);
    engDelayR.assign ((size_t) engDelayLen, 0.0f);
    engDelayPos = 0;

    // Post-downsample catch: release over cfg::clipDipSamples. This is a ripple
    // catcher, so its time constant belongs in SAMPLES, not milliseconds — the
    // downsampler's overshoot is a fixed number of samples long at any rate.
    dipG[0] = dipG[1] = 1.0f;
    dipRelCoeff = 1.0f - std::exp (-1.0f / (float) juce::jmax (1, cfg::clipDipSamples));

    // Dry delay line for the BYPASS crossfade. Sized to the WHOLE processed-path delay
    // (lookahead + the analysis window every engine now carries), or the dry copy would
    // arrive early and comb-filter against the wet.
    dryDelayLen = juce::jmax (1, limiter.maxLatencySamples() + partialEng.latencySamples());
    dryDelayL.assign ((size_t) dryDelayLen, 0.0f);
    dryDelayR.assign ((size_t) dryDelayLen, 0.0f);
    dryDelayPos = 0;

    amountSmoothed.reset (sampleRate, 0.02);
    inGainSmoothed.reset (sampleRate, 0.02);
    amountSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamID::amount)->load());
    inGainSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamID::inGain)->load());
    outGainSmoothed.reset (sampleRate, 0.02);
    outGainSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (juce::jmin (0.0f,
            apvts.getRawParameterValue (ParamID::outGain)->load())));

    // BS.1770 LUFS metering: design the K-weighting filters for this sample rate,
    // and size the 400 ms momentary block. Reset the gated integration.
    designKWeighting (sampleRate);
    kShelfL.reset(); kShelfR.reset(); kHpfL.reset(); kHpfR.reset();
    // BS.1770 block accumulator: 100 ms of samples per block.
    lufsBlockLen = juce::jmax (1, (int) (sampleRate * (kLufsBlockMs / 1000.0)));
    lufsBlockPos = 0;
    lufsBlockAcc = 0.0;
    lufsRecent.assign ((size_t) kShortTermBlk, 0.0);
    lufsRecentPos = lufsRecentFilled = 0;
    lufsIntSum = 0.0; lufsIntCount = 0;
    lufsGateBlocks.clear();
    lufsGateBlocks.reserve (36000);   // pre-allocate: no RT allocation later
}

//==============================================================================
// Flush every recursive state. Called by the host on transport jumps, and it is the
// recovery path if a NaN ever slipped in before the input guard existed.
void HouseCompAudioProcessor::reset()
{
    comp.prepare (currentSampleRate);
    for (auto& bc : bandComp) bc.prepare (currentSampleRate);

    limiter.flush();
    for (auto& sl : specLim) sl.flush();

    for (auto& sx : xover)     for (auto& f : sx) f.reset();
    for (auto& sx : specXover) for (auto& f : sx) f.reset();
    for (auto& ap : apLowB)    for (auto& f : ap) f.reset();
    for (auto& ap : apMidB)    for (auto& f : ap) f.reset();
    for (auto& f : inLowXover) f.reset();

    for (auto& os : oversampling) if (os != nullptr) os->reset();

    adaaPrevX[0]  = adaaPrevX[1]  = 0.0;
    adaaPrevX2[0] = adaaPrevX2[1] = 0.0;
    dcX1[0] = dcX1[1] = dcY1[0] = dcY1[1] = 0.0f;
    dipG[0] = dipG[1] = 1.0f;
    maskClip.flush();
    crestRot.flush();
    disperser.flush();
    partialEng.flush();
    std::fill (engDelayL.begin(), engDelayL.end(), 0.0f);
    std::fill (engDelayR.begin(), engDelayR.end(), 0.0f);
    engDelayPos = 0;

    std::fill (dryDelayL.begin(), dryDelayL.end(), 0.0f);
    std::fill (dryDelayR.begin(), dryDelayR.end(), 0.0f);
    dryDelayPos = 0;

    kShelfL.reset(); kShelfR.reset(); kHpfL.reset(); kHpfR.reset();
    // Restart the loudness measurement (integrated is a running total, so a
    // transport jump must not carry the old programme into the new one).
    lufsBlockPos = 0;
    lufsBlockAcc = 0.0;
    std::fill (lufsRecent.begin(), lufsRecent.end(), 0.0);
    lufsRecentPos = lufsRecentFilled = 0;
    lufsIntSum = 0.0; lufsIntCount = 0;
    lufsGateBlocks.clear();
}

//==============================================================================
// One completed 100 ms block of K-weighted mean-square power. Updates momentary,
// short-term and the gated integrated loudness exactly as BS.1770-4 specifies.
void HouseCompAudioProcessor::pushLufsBlock (double meanSquare)
{
    // Ring buffer of recent block powers (holds enough for the 3 s short-term mean).
    if ((int) lufsRecent.size() != kShortTermBlk)
    {
        lufsRecent.assign ((size_t) kShortTermBlk, 0.0);
        lufsRecentPos = lufsRecentFilled = 0;
    }
    lufsRecent[(size_t) lufsRecentPos] = meanSquare;
    lufsRecentPos = (lufsRecentPos + 1) % kShortTermBlk;
    lufsRecentFilled = juce::jmin (kShortTermBlk, lufsRecentFilled + 1);

    auto meanOfLast = [this] (int count) -> double
    {
        const int n = juce::jmin (count, lufsRecentFilled);
        if (n <= 0) return 0.0;
        double sum = 0.0;
        for (int i = 1; i <= n; ++i)
        {
            int idx = lufsRecentPos - i;
            while (idx < 0) idx += kShortTermBlk;
            sum += lufsRecent[(size_t) idx];
        }
        return sum / (double) n;
    };
    auto toLufs = [] (double ms) { return -0.691 + 10.0 * std::log10 (ms + 1.0e-12); };

    // MOMENTARY = rectangular mean of the last 4 blocks (400 ms) — per spec.
    meterLufsDb.store ((float) juce::jlimit (-100.0, 6.0, toLufs (meanOfLast (kMomentaryBlk))));
    // SHORT-TERM = mean of the last 30 blocks (3 s).
    meterLufsShortDb.store ((float) juce::jlimit (-100.0, 6.0, toLufs (meanOfLast (kShortTermBlk))));

    // INTEGRATED: absolute gate at -70 LUFS, then a relative gate 10 LU below the
    // mean of the blocks that passed the absolute gate.
    const double blockLufs = toLufs (meanSquare);
    if (blockLufs > -70.0)
    {
        lufsIntSum += meanSquare;
        ++lufsIntCount;
        if (lufsGateBlocks.size() < 36000)          // ~1 hour cap, then stop growing
            lufsGateBlocks.push_back ((float) meanSquare);

        const double ungatedMean = lufsIntSum / (double) lufsIntCount;
        const double relGate     = toLufs (ungatedMean) - 10.0;   // -10 LU relative
        double gatedSum = 0.0; int gatedCount = 0;
        for (float ms : lufsGateBlocks)
            if (toLufs ((double) ms) > relGate) { gatedSum += (double) ms; ++gatedCount; }
        if (gatedCount > 0)
            meterLufsIntDb.store ((float) juce::jlimit (-100.0, 6.0,
                                    toLufs (gatedSum / (double) gatedCount)));
    }
}

//==============================================================================
// ITU-R BS.1770 K-weighting: a high-shelf (stage 1) followed by a high-pass /
// RLB weighting (stage 2). The reference coefficients are specified at 48 kHz;
// we re-derive them for the actual sample rate by matching the analog prototypes
// via the bilinear transform (standard approach, matches libebur128 to <0.1 dB).
void HouseCompAudioProcessor::designKWeighting (double sr)
{
    // --- Stage 1: high-shelf (+~4 dB, corner ~1681 Hz, Q ~0.707) ------------
    {
        const double f0 = 1681.974450955533;
        const double G  = 3.999843853973347;   // dB
        const double Q  = 0.7071752369554196;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / sr);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad& f = kShelfL; // compute once, copy to all four below
        f.b0 = (Vh + Vb * K / Q + K * K) / a0;
        f.b1 = 2.0 * (K * K - Vh) / a0;
        f.b2 = (Vh - Vb * K / Q + K * K) / a0;
        f.a1 = 2.0 * (K * K - 1.0) / a0;
        f.a2 = (1.0 - K / Q + K * K) / a0;
        f.z1 = f.z2 = 0.0;
        kShelfR = f;
    }
    // --- Stage 2: high-pass (RLB), corner ~38 Hz, Q ~0.5 -------------------
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / sr);
        const double a0 = 1.0 + K / Q + K * K;
        Biquad& f = kHpfL;
        f.b0 = 1.0 / a0;
        f.b1 = -2.0 / a0;
        f.b2 = 1.0 / a0;
        f.a1 = 2.0 * (K * K - 1.0) / a0;
        f.a2 = (1.0 - K / Q + K * K) / a0;
        f.z1 = f.z2 = 0.0;
        kHpfR = f;
    }
}

bool HouseCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono();
}

//==============================================================================
// Clipper math (2026-grade): a HARD clip plus its 1st AND 2nd antiderivatives, so
// we can run 2nd-order ADAA (much lower aliasing than ADAA1 at the same OS), and a
// HARDNESS morph that blends the hard clip with a smooth cubic soft-clip (warm at
// low hardness, brickwall at hardness=1).
//
//   f  : the clip curve            (value)
//   F1 : first antiderivative      (for ADAA1 / ADAA2)
//   F2 : second antiderivative     (for ADAA2)
//==============================================================================
namespace clipmath
{
    // NOTE: all of this runs in DOUBLE precision on purpose. ADAA divides tiny
    // differences of antiderivatives by tiny sample deltas; at high oversampling
    // (16x-64x) consecutive samples differ by ~1e-4..1e-5, and float32 catastrophic
    // cancellation turned those divided differences into broadband noise (the
    // "hiss like overdrive" bug). Doubles give ~9 extra digits => clean output.
    // The reference ADAA implementation (Chowdhury) also uses doubles.

    // --- HARD clip: clamp(x,-1,1). Unity below the corner, flat at +/-1. --------
    inline double fHard  (double x) { return x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x); }
    inline double F1Hard (double x) { return (std::abs (x) <= 1.0) ? (0.5 * x * x)
                                                                    : (std::abs (x) - 0.5); }
    // 2nd antiderivative of the hard clip: x^3/6 inside, smooth continuation outside.
    inline double F2Hard (double x)
    {
        const double a = std::abs (x);
        if (a <= 1.0) return x * x * x / 6.0;
        const double s = (x < 0.0) ? -1.0 : 1.0;
        return s * (0.5 * a * a - 0.5 * a + 1.0 / 6.0);
    }

    // --- SMOOTH cubic soft-clip (odd, C1): f(x)=x-x^3/3 inside, +/-2/3 beyond. ---
    inline double fSoft (double x)
    {
        const double a = x < -1.0 ? -1.0 : (x > 1.0 ? 1.0 : x);
        return a - a * a * a / 3.0;
    }
    inline double F1Soft (double x)
    {
        const double a = std::abs (x);
        if (a <= 1.0) return 0.5 * x * x - x * x * x * x / 12.0;
        return (2.0 / 3.0) * (a - 1.0) + (0.5 - 1.0 / 12.0); // linear beyond, C1
    }
    inline double F2Soft (double x)
    {
        const double a = std::abs (x);
        const double s = (x < 0.0) ? -1.0 : 1.0;
        if (a <= 1.0) return x * x * x / 6.0 - x * x * x * x * x / 60.0;
        const double edge = 1.0 / 6.0 - 1.0 / 60.0;
        return s * ((1.0 / 3.0) * (a - 1.0) * (a - 1.0) + (0.5 - 1.0 / 12.0) * (a - 1.0) + edge);
    }

    // --- HARDNESS morph: h=1 -> hard, h->0 -> smooth soft-clip. Linear blend of the
    //     curve AND its antiderivatives (so ADAA stays exact for the blended curve).
    inline double f  (double x, double h) { return h * fHard (x)  + (1.0 - h) * fSoft (x); }
    inline double F1 (double x, double h) { return h * F1Hard (x) + (1.0 - h) * F1Soft (x); }
    inline double F2 (double x, double h) { return h * F2Hard (x) + (1.0 - h) * F2Soft (x); }
}

//==============================================================================
//  MASK CLIPPER — Peak Cancellation (PC-CFR) with a masking-designed pulse.
//  See the long note on the struct in PluginProcessor.h for the why.
//==============================================================================

// Build the three fixed band pulses. Done once per prepare(); allocates, so it must
// never be called from the audio thread.
//
// The construction is deliberately subtractive:
//     pLow = lp(400)      pMid = lp(4000) - lp(400)      pHigh = delta - lp(4000)
// so pLow + pMid + pHigh == delta EXACTLY, whatever the filters actually do. That is
// what makes "equal weights == a plain hard clip" exact rather than approximate, and
// it is why no FFT and no phase-compensation bank is needed anywhere here.
void HouseCompAudioProcessor::MaskClipper::designKernels()
{
    kLen  = juce::jmax (16, cfg::maskKernelLen & ~1);   // force even so the centre is exact
    kHalf = kLen / 2;

    auto sincf = [] (double x)
    {
        constexpr double pi = juce::MathConstants<double>::pi;
        return (std::abs (x) < 1.0e-9) ? 1.0 : std::sin (pi * x) / (pi * x);
    };

    // Hann-windowed sinc lowpass, normalised to unity gain at DC.
    auto lowpass = [&] (double fc, std::vector<float>& out)
    {
        constexpr double pi = juce::MathConstants<double>::pi;
        const double ft = juce::jlimit (1.0e-4, 0.49, fc / sampleRate);

        std::vector<double> h ((size_t) kLen);
        double sum = 0.0;
        for (int k = 0; k < kLen; ++k)
        {
            const double x = (double) (k - kHalf);
            const double w = 0.5 - 0.5 * std::cos (2.0 * pi * (double) k / (double) (kLen - 1));
            h[(size_t) k] = 2.0 * ft * sincf (2.0 * ft * x) * w;
            sum += h[(size_t) k];
        }
        const double g = (std::abs (sum) > 1.0e-12) ? (1.0 / sum) : 1.0;

        out.resize ((size_t) kLen);
        for (int k = 0; k < kLen; ++k)
            out[(size_t) k] = (float) (h[(size_t) k] * g);
    };

    std::vector<float> h1, h2;
    lowpass (cfg::maskKernelLoHz, h1);
    lowpass (cfg::maskKernelHiHz, h2);

    pLow .assign ((size_t) kLen, 0.0f);
    pMid .assign ((size_t) kLen, 0.0f);
    pHigh.assign ((size_t) kLen, 0.0f);
    pComp.assign ((size_t) kLen, 0.0f);

    for (int k = 0; k < kLen; ++k)
    {
        const float delta = (k == kHalf) ? 1.0f : 0.0f;
        pLow [(size_t) k] = h1[(size_t) k];
        pMid [(size_t) k] = h2[(size_t) k] - h1[(size_t) k];
        pHigh[(size_t) k] = delta - h2[(size_t) k];
    }
}

void HouseCompAudioProcessor::MaskClipper::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    designKernels();

    bufLen = 2 * kHalf + juce::jmax (1, maxBlock);
    for (auto& w : work) w.assign ((size_t) bufLen, 0.0f);

    // Same split points as the spectral limiter, so the two stages agree about where
    // the bands are. PARALLEL here (each fed the raw mono sum), not cascaded.
    constexpr float splits[5] = { 90.0f, 200.0f, 700.0f, 3000.0f, 9000.0f };
    juce::dsp::ProcessSpec sp { sr, (juce::uint32) juce::jmax (1, maxBlock), 1 };
    for (int b = 0; b < 5; ++b)
    {
        detLp[b].prepare (sp);
        detLp[b].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        detLp[b].setCutoffFrequency (juce::jmin (splits[b], (float) (sr * 0.45)));
    }

    atkC = 1.0f - std::exp (-1.0f / (0.001f * cfg::maskLevelAtkMs * (float) sr));
    relC = 1.0f - std::exp (-1.0f / (0.001f * cfg::maskLevelRelMs * (float) sr));

    flush();
}

void HouseCompAudioProcessor::MaskClipper::flush()
{
    for (auto& w : work)  std::fill (w.begin(), w.end(), 0.0f);
    for (auto& f : detLp) f.reset();
    for (auto& v : lvl)   v = 0.0f;
    qSm[0] = qSm[1] = qSm[2] = 1.0f;
    tilt = 0.0f;
    nsE1[0] = nsE1[1] = nsE2[0] = nsE2[1] = 0.0f;

    // Equal weights => the composite pulse IS a unit impulse => a plain hard clip.
    std::fill (pComp.begin(), pComp.end(), 0.0f);
    if (kHalf >= 0 && kHalf < (int) pComp.size())
        pComp[(size_t) kHalf] = 1.0f;
}

// Measure what is currently masking what, and turn that into the three pulse weights.
void HouseCompAudioProcessor::MaskClipper::updateWeights (const float* l, const float* r, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (l[i] + r[i]);

        float lp[5], hp;
        for (int b = 0; b < 5; ++b)
            detLp[b].processSample (0, m, lp[b], hp);

        // Telescoping subtraction: these six sum back to m exactly.
        const float band[6] = { lp[0], lp[1] - lp[0], lp[2] - lp[1],
                                lp[3] - lp[2], lp[4] - lp[3], m - lp[4] };

        for (int b = 0; b < 6; ++b)
        {
            const float a = std::abs (band[b]);
            lvl[b] += (a - lvl[b]) * (a > lvl[b] ? atkC : relC);
        }
    }

    // SPREADING FUNCTION. A loud band masks its neighbours — strongly UPWARD in
    // frequency, weakly downward. This is the step that makes the stage psychoacoustic
    // rather than merely multiband: it is why a loud kick buys headroom for distortion
    // in the low mids too, not just in the sub.
    float s[6];
    for (int b = 0; b < 6; ++b)
        s[b] = juce::Decibels::gainToDecibels (lvl[b], -120.0f);

    for (int b = 1; b < 6; ++b) s[b] = juce::jmax (s[b], s[b - 1] - cfg::maskSpreadUpDb);
    for (int b = 4; b >= 0; --b) s[b] = juce::jmax (s[b], s[b + 1] - cfg::maskSpreadDownDb);

    // How much distortion each band can hide, folded from the 6 detector bands into
    // the 3 the (short, hence coarse) pulse can actually resolve.
    auto allow = [&] (int b) { return juce::Decibels::decibelsToGain (s[b] - cfg::maskDepthDb, 0.0f); };
    float q[3] = { juce::jmax (allow (0), allow (1)),
                   juce::jmax (allow (2), allow (3)),
                   juce::jmax (allow (4), allow (5)) };

    const float qMax = juce::jmax (q[0], juce::jmax (q[1], q[2]));
    if (qMax > 1.0e-12f)
    {
        // The floor matters: if a band were allowed to go to zero the pulse would have
        // to get very wide and tall to still cancel the peak, and a wide tall pulse
        // creates NEW peaks around the old one. Flooring bounds the pulse's spread.
        for (auto& v : q) v = juce::jlimit (cfg::maskWeightFloor, 1.0f, v / qMax);
    }
    else
    {
        q[0] = q[1] = q[2] = 1.0f;   // silence -> uniform -> pulse == delta -> plain clip
    }

    // Smooth per BLOCK (this runs once per block, so the coefficient tracks its length).
    const float a = 1.0f - std::exp (-(float) n
                        / (0.001f * cfg::maskWeightSmMs * (float) sampleRate));
    for (int i = 0; i < 3; ++i)
        qSm[i] += (q[i] - qSm[i]) * a;

    // Tilt = how far from uniform the weights are (0 = acting like a plain clipper).
    const float qLo = juce::jmin (qSm[0], juce::jmin (qSm[1], qSm[2]));
    const float qHi = juce::jmax (qSm[0], juce::jmax (qSm[1], qSm[2]));
    tilt = (qHi > 1.0e-9f) ? juce::jlimit (0.0f, 1.0f, 1.0f - qLo / qHi) : 0.0f;
}

// Weighted sum of the three band pulses, normalised so the centre tap is exactly 1 —
// that is what makes the cancelled peak land precisely on the ceiling.
void HouseCompAudioProcessor::MaskClipper::rebuildComposite()
{
    const size_t c = (size_t) kHalf;
    const float denom = qSm[0] * pLow[c] + qSm[1] * pMid[c] + qSm[2] * pHigh[c];
    // pLow[c]+pMid[c]+pHigh[c] == 1 and every weight is >= maskWeightFloor, so denom
    // can never approach zero; the guard is belt-and-braces.
    const float inv = (std::abs (denom) > 1.0e-6f) ? (1.0f / denom) : 1.0f;

    for (int k = 0; k < kLen; ++k)
        pComp[(size_t) k] = (qSm[0] * pLow[(size_t) k]
                           + qSm[1] * pMid[(size_t) k]
                           + qSm[2] * pHigh[(size_t) k]) * inv;
}

// Public entry point. Splits into chunks the work buffer can actually hold, so a host
// that sends a block larger than the one it promised in prepareToPlay still gets
// processed — silently doing nothing here would be worse than useless, because the
// latency would still be reported and the signal would just be delayed.
void HouseCompAudioProcessor::MaskClipper::processBlock (juce::AudioBuffer<float>& buffer,
                                                          float ceilingLin, float amount01,
                                                          int iters)
{
    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0 || kLen <= 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    const int chunk = maxChunk();
    for (int off = 0; off < n; off += chunk)
        processChunk (l + off, r != nullptr ? r + off : nullptr,
                      juce::jmin (chunk, n - off), ceilingLin, amount01, iters);
}

void HouseCompAudioProcessor::MaskClipper::processChunk (float* chL, float* chR, int n,
                                                          float ceilingLin, float amount01,
                                                          int iters)
{
    const int L = kHalf;
    if (n <= 0 || n > bufLen - 2 * L) return;

    updateWeights (chL, chR != nullptr ? chR : chL, n);
    rebuildComposite();

    // Buffer layout, length 2L + n:  [0, 2L) carried over, [2L, 2L+n) the new block.
    // Peaks are searched in [L, L+n) so a pulse centred on one always fits. Only
    // [0, n) is output; [n, n+2L) stays behind because a peak in the NEXT block can
    // still reach back into it. Hence the latency is 2L, not L.
    std::copy (chL, chL + n, work[0].begin() + 2 * L);
    if (chR != nullptr) std::copy (chR, chR + n, work[1].begin() + 2 * L);
    else                std::fill (work[1].begin() + 2 * L, work[1].begin() + 2 * L + n, 0.0f);

    float* wl = work[0].data();
    float* wr = work[1].data();
    const float* p = pComp.data();

    for (int it = 0; it < iters; ++it)
    {
        for (int i = L; i < L + n; ++i)
        {
            const float a = juce::jmax (std::abs (wl[i]), std::abs (wr[i]));
            if (a <= ceilingLin) continue;

            // Only act on the CREST. Cancelling at every sample of an overshoot would
            // stack pulses and over-correct; nearby peaks that this uncovers are picked
            // up by the next iteration, and whatever is left falls through to the
            // oversampled ADAA clipper downstream.
            const float aPrev = juce::jmax (std::abs (wl[i - 1]), std::abs (wr[i - 1]));
            const float aNext = juce::jmax (std::abs (wl[i + 1]), std::abs (wr[i + 1]));
            if (! (a > aPrev && a >= aNext)) continue;

            // BOTH channels are scaled by the SAME factor, so the louder one lands
            // exactly on the ceiling and the L/R ratio at the peak is untouched — the
            // stereo image cannot shift, which a per-channel subtraction would risk.
            const float frac = amount01 * (a - ceilingLin) / a;
            const float cl = wl[i] * frac;
            const float cr = wr[i] * frac;

            const int base = i - L;
            for (int k = 0; k < kLen; ++k)
            {
                wl[base + k] -= cl * p[k];
                wr[base + k] -= cr * p[k];
            }
        }
    }

    // ---- OPTIONAL residual polish: noise-shaped mop-up (off by default) ----
    // Only the region about to be output is touched — everything past it can still be
    // rewritten by the next block's pulses.
    //
    // Read the note in Config.h before enabling. Three honest costs: this loop runs at
    // BASE rate so it is not anti-aliased; noise shaping always raises TOTAL error
    // power (it relocates, it does not remove); and unlike dither — where the error is
    // near-white and signal-independent — clipping error is strongly correlated with
    // the signal, so the loop can ring. The clamp is the guard against that, and it is
    // why this runs last, on the small residual PC-CFR leaves behind.
    if (cfg::maskNoiseShape)
    {
        const float lim = cfg::maskNsClamp * ceilingLin;
        for (int ch = 0; ch < 2; ++ch)
        {
            float* w  = work[ch].data();
            float  e1 = nsE1[ch], e2 = nsE2[ch];
            for (int i = 0; i < n; ++i)
            {
                const float fb = juce::jlimit (-lim, lim, cfg::maskNsA1 * e1 + cfg::maskNsA2 * e2);
                const float x  = w[i] - fb;
                const float y  = juce::jlimit (-ceilingLin, ceilingLin, x);
                e2 = e1; e1 = x - y;
                w[i] = y;
            }
            nsE1[ch] = e1; nsE2[ch] = e2;
        }
    }

    std::copy (work[0].begin(), work[0].begin() + n, chL);
    if (chR != nullptr) std::copy (work[1].begin(), work[1].begin() + n, chR);

    for (auto& w : work)
        std::copy (w.begin() + n, w.begin() + n + 2 * L, w.begin());
}

//==============================================================================
//  CREST DISPERSER — the PHASE mode. See the long note in PluginProcessor.h.
//==============================================================================

// Same RBJ all-pass as the crest rotator: b0 == a2, b1 == a1, b2 == 1. That symmetry
// is what makes the magnitude response dead flat at every frequency, so no amount of
// dispersion can colour the sound — only the timing of the phases moves.
void HouseCompAudioProcessor::CrestDisperser::AP2::set (double f0, double q, double sr)
{
    const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / sr;
    const double cw    = std::cos (w0);
    const double sw    = std::sin (w0);
    const double alpha = sw / (2.0 * juce::jmax (0.05, q));
    const double a0    = 1.0 + alpha;

    b0 = (float) ((1.0 - alpha) / a0);
    b1 = (float) ((-2.0 * cw)   / a0);
    b2 = 1.0f;
    a1 = (float) ((-2.0 * cw)   / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

// LOG-spaced around a centre. Even spacing in log frequency is what turns a row of
// individual all-passes into one smooth group-delay ramp rather than a set of separate
// phase bumps — the ramp is what actually disperses an impulse.
void HouseCompAudioProcessor::CrestDisperser::placeSections (double c)
{
    const double spread = juce::jmax (1.2f, cfg::dispSpread);
    const double lo = juce::jlimit ((double) cfg::dispMinHz, (double) cfg::dispMaxHz, c / spread);
    const double hi = juce::jlimit (lo * 1.5, (double) cfg::dispMaxHz, c * spread);

    for (int s = 0; s < sections; ++s)
    {
        const double t = (sections > 1) ? (double) s / (double) (sections - 1) : 0.0;
        const double f = lo * std::pow (hi / lo, t);
        for (int ch = 0; ch < 2; ++ch)
            ap[s][ch].set (juce::jmin (f, sampleRate * 0.45), cfg::dispQ, sampleRate);
    }
}

void HouseCompAudioProcessor::CrestDisperser::prepare (double sr)
{
    sampleRate = sr;
    sections = juce::jlimit (1, (int) kMaxSections, cfg::dispSections);
    winLen    = juce::jmax (1, (int) (0.4 * sr));                  // crest readout window
    searchLen = juce::jmax (1, (int) (cfg::dispSearchSec * sr));   // placement window

    // Detector-only band split, same points the spectral limiter uses. Audio does NOT
    // pass through these — they exist purely to find where the crest factor lives.
    constexpr float splits[kDetBands - 1] = { 90.0f, 200.0f, 700.0f, 3000.0f, 9000.0f };
    juce::dsp::ProcessSpec sp { sr, 512, 1 };
    for (int b = 0; b < kDetBands - 1; ++b)
    {
        detLp[b].prepare (sp);
        detLp[b].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        detLp[b].setCutoffFrequency (juce::jmin (splits[b], (float) (sr * 0.45)));
    }

    centreHz = std::sqrt ((double) cfg::dispLoHz * (double) cfg::dispHiHz);
    placeSections (centreHz);
    flush();
}

// Decide where the cascade should sit: the frequency region whose crest factor is
// worst. Crest is peak/RMS, so it says "this band spikes" rather than "this band is
// loud" — which is exactly the thing dispersion can fix.
void HouseCompAudioProcessor::CrestDisperser::updatePlacement()
{
    // Rough centre of each detector band, and the ceiling above which a high crest is
    // ignored: hats spike constantly, and dragging the cascade up there would smear the
    // top end for no gain. A kick's transient is the START of its sound; a hat's
    // transient IS its sound.
    constexpr double centres[kDetBands] = { 45.0, 140.0, 420.0, 1600.0, 5500.0, 13000.0 };

    double wSum = 0.0, fSum = 0.0;
    for (int b = 0; b < kDetBands; ++b)
    {
        if (centres[b] > (double) cfg::dispHfCapHz) continue;

        const double rms = std::sqrt (bandSq[b] / (double) juce::jmax (1, searchPos));
        if (rms < 1.0e-6 || bandPk[b] < 1.0e-6) continue;

        const double crest = bandPk[b] / rms;
        // Weight by BOTH how spiky the band is and how much energy it carries: a spiky
        // but inaudible band should not steer the cascade.
        const double w = juce::jmax (0.0, crest - 2.0) * rms;
        wSum += w;
        fSum += w * std::log (centres[b]);
    }

    if (wSum > 1.0e-12)
    {
        const double want = juce::jlimit ((double) cfg::dispMinHz * 1.5,
                                          (double) cfg::dispMaxHz / 1.5,
                                          std::exp (fSum / wSum));

        // Glide. Moving the cascade quickly would read as a phaser sweep, so the centre
        // is rate-limited the same way the crest rotator's frequency is.
        const double maxStep = centreHz * (double) cfg::dispGlideRate * (double) cfg::dispSearchSec;
        centreHz = juce::jlimit (centreHz - maxStep, centreHz + maxStep, want);
        placeSections (centreHz);
    }

    for (int b = 0; b < kDetBands; ++b) { bandPk[b] = 0.0; bandSq[b] = 0.0; }
    searchPos = 0;
}

void HouseCompAudioProcessor::CrestDisperser::flush()
{
    for (int s = 0; s < kMaxSections; ++s)
        for (int ch = 0; ch < 2; ++ch)
            ap[s][ch].reset();

    for (auto& f : detLp) f.reset();
    for (int b = 0; b < kDetBands; ++b) { bandPk[b] = 0.0; bandSq[b] = 0.0; }
    searchPos = 0;

    pkIn = sqIn = pkOut = sqOut = 0.0;
    winPos = 0;
    reductionDb = 0.0f;
}

void HouseCompAudioProcessor::CrestDisperser::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0 || sections <= 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        float xl = l[i];
        float xr = (r != nullptr) ? r[i] : xl;
        if (! std::isfinite (xl)) xl = 0.0f;   // recursive filters: never let a NaN in
        if (! std::isfinite (xr)) xr = 0.0f;

        const double mIn = 0.5 * ((double) xl + (double) xr);
        pkIn = juce::jmax (pkIn, std::abs (mIn));
        sqIn += mIn * mIn;

        // Per-band crest measurement. DETECTORS ONLY — the audio path above is
        // untouched by these filters, so the band split cannot colour anything.
        if (cfg::dispAdaptive)
        {
            const float m = (float) mIn;
            float lp[kDetBands - 1], hp;
            for (int b = 0; b < kDetBands - 1; ++b)
                detLp[b].processSample (0, m, lp[b], hp);

            // Telescoping subtraction: these six sum back to m exactly.
            const float band[kDetBands] = { lp[0], lp[1] - lp[0], lp[2] - lp[1],
                                            lp[3] - lp[2], lp[4] - lp[3], m - lp[4] };
            for (int b = 0; b < kDetBands; ++b)
            {
                const double a = std::abs ((double) band[b]);
                bandPk[b] = juce::jmax (bandPk[b], a);
                bandSq[b] += a * a;
            }
        }

        // IDENTICAL cascade on both channels. Anything else would move the phases of
        // L and R apart and walk the stereo image around.
        for (int s = 0; s < sections; ++s)
        {
            xl = ap[s][0].process (xl);
            xr = ap[s][1].process (xr);
        }

        const double mOut = 0.5 * ((double) xl + (double) xr);
        pkOut = juce::jmax (pkOut, std::abs (mOut));
        sqOut += mOut * mOut;

        l[i] = xl;
        if (r != nullptr) r[i] = xr;
    }

    if (cfg::dispAdaptive)
    {
        searchPos += n;
        if (searchPos >= searchLen)
            updatePlacement();
    }

    winPos += n;
    if (winPos >= winLen)
    {
        // Crest = peak / RMS. The RMS is untouched by an all-pass, so any change in
        // crest is pure peak reduction — exactly the headroom this mode is buying.
        const double rmsIn  = std::sqrt (sqIn  / (double) winPos);
        const double rmsOut = std::sqrt (sqOut / (double) winPos);

        if (rmsIn > 1.0e-6 && rmsOut > 1.0e-6 && pkIn > 1.0e-6 && pkOut > 1.0e-6)
        {
            const double crestIn  = pkIn  / rmsIn;
            const double crestOut = pkOut / rmsOut;
            const float got = (float) (20.0 * std::log10 (crestIn / juce::jmax (1.0e-9, crestOut)));
            reductionDb += (juce::jlimit (-6.0f, 12.0f, got) - reductionDb) * 0.3f;
        }

        pkIn = sqIn = pkOut = sqOut = 0.0;
        winPos = 0;
    }
}

//==============================================================================
//  FUTURE ENGINE — partial-based dynamics. See the long note in PluginProcessor.h.
//
//  Stage one: take the signal apart and put it back together, nothing else. If this
//  is not transparent, per-tone dynamics cannot be built on it, and it is far cheaper
//  to learn that here than after writing the rest.
//==============================================================================
// juce::dsp::FFT must be constructed with its order, so the header cannot read it from
// Config.h (which it does not include). Catch any drift between the two at compile time.
static_assert (HouseCompAudioProcessor::kFutureOrderCheck == cfg::futureFftOrder,
               "PartialEngine::kOrder in PluginProcessor.h must match cfg::futureFftOrder in Config.h");

void HouseCompAudioProcessor::PartialEngine::prepare (double sr, int maxBlock)
{
    juce::ignoreUnused (maxBlock);

    sampleRate = sr;
    fftSize = 1 << kOrder;
    hop     = juce::jmax (1, fftSize / juce::jmax (2, cfg::futureOverlap));

    // Hann, applied on BOTH the way in and the way out. Using it twice is what keeps
    // a modified frame from producing edge discontinuities once we start editing
    // spectra in stage two — the price is that the squared window must sum to a
    // constant across hops, which it does at 75% overlap.
    //
    // PERIODIC Hann (divide by fftSize), NOT the symmetric one (fftSize - 1). This is
    // the difference between exact reconstruction and roughly -87 dB of error: only the
    // periodic form tiles seamlessly when shifted by a hop. Measured, not assumed —
    // the symmetric version looks identical and quietly costs ~50 dB.
    window.resize ((size_t) fftSize);
    for (int k = 0; k < fftSize; ++k)
        window[(size_t) k] = (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                            * (double) k / (double) fftSize));

    for (auto& b : inFifo) b.assign ((size_t) fftSize, 0.0f);
    for (auto& b : outAcc) b.assign ((size_t) fftSize, 0.0f);
    for (auto& b : spec)   b.assign ((size_t) fftSize * 2, 0.0f);

    // --- Normalisation, MEASURED rather than assumed ------------------------
    // 1. COLA: how much the squared window sums to at hop spacing. Computed at an
    //    interior point so no edge case can skew it.
    double cola = 0.0;
    const int n0 = fftSize / 2;
    for (int k = n0 % hop; k < fftSize; k += hop)
        cola += (double) window[(size_t) k] * (double) window[(size_t) k];

    // 2. FFT round-trip gain. Whether a library normalises its inverse transform is
    //    exactly the kind of assumption that silently costs you 60 dB, so measure it:
    //    push a unit impulse through forward+inverse and read back what comes out.
    std::fill (spec[0].begin(), spec[0].end(), 0.0f);
    spec[0][0] = 1.0f;
    fft.performRealOnlyForwardTransform (spec[0].data(), true);
    fft.performRealOnlyInverseTransform (spec[0].data());
    const double rt = (std::abs ((double) spec[0][0]) > 1.0e-12) ? (double) spec[0][0] : 1.0;

    outScale = (float) (1.0 / (cola * rt));

    // --- per-tone dynamics state -------------------------------------------
    numBins = fftSize / 2 + 1;
    binGain.assign     ((size_t) numBins, 1.0f);
    // Holds a linear bin magnitude in FUTURE and a band level in dB in FUTURE 2; -200
    // reads as silence on either scale, so the first frame is an onset everywhere, which
    // is correct — everything is new — and the age ramp fades compression in safely.
    binPrevMag.assign  ((size_t) numBins, -200.0f);
    binAge.assign      ((size_t) numBins, 0);
    binAgeLimit.assign ((size_t) numBins, cfg::futureAgeFrames);
    binMag.assign      ((size_t) numBins, 0.0f);
    binWDb.assign      ((size_t) numBins, -120.0f);
    binSpread.assign   ((size_t) numBins, -120.0f);
    gainTmp.assign     ((size_t) numBins, 1.0f);
    binPrevRe.assign   ((size_t) numBins, 0.0f);
    binPrevIm.assign   ((size_t) numBins, 0.0f);
    binTonal.assign    ((size_t) numBins, 0.0f);
    binOnset.assign    ((size_t) numBins, 0);
    binAmtScale.assign ((size_t) numBins, 1.0f);
    binSinceOnset.assign ((size_t) numBins, -1);
    upSlope.assign     ((size_t) numBins, 0.0f);
    downSlope.assign   ((size_t) numBins, 0.0f);
    bandHalf.assign    ((size_t) numBins, 1);
    binAgeBase.assign  ((size_t) numBins, (float) cfg::futureAgeFrames);
    binAgeMax.assign   ((size_t) numBins, cfg::futureAgeMaxFrames);
    prefix.assign      ((size_t) numBins + 1, 0.0f);
    rotRe.assign       ((size_t) numBins, 1.0f);
    rotIm.assign       ((size_t) numBins, 0.0f);

    // Masking spread slopes and the expected per-hop phase rotation, both per bin.
    {
        const double binHz = sr / (double) fftSize;
        for (int b = 0; b < numBins; ++b)
        {
            const double f = (double) b * binHz;

            // Zwicker critical bandwidth: how wide a "unit of hearing" is at this
            // frequency. Converting the per-Bark slopes through it is what stops the
            // shadow from racing across the top end, where a Bark spans many bins.
            const double bw = 25.0 + 75.0 * std::pow (1.0 + 1.4 * (f / 1000.0) * (f / 1000.0), 0.69);
            const double perBin = binHz / juce::jmax (1.0, bw);

            upSlope[(size_t) b]   = (float) (cfg::futureMaskSpreadUpDb   * perBin);
            downSlope[(size_t) b] = (float) (cfg::futureMaskSpreadDownDb * perBin);

            // Half a critical band, in bins. Everything downstream — the level a tone is
            // judged by and the width its gain is smoothed over — now works at this
            // resolution, because it is the resolution the EAR has. A fixed number of
            // bins is wrong in both directions: at 50 Hz seven bins spans an octave and
            // a half (merging kick, sub and bass into one gain), while at 8 kHz the same
            // seven bins spans a hundredth of an octave and does nothing at all.
            bandHalf[(size_t) b] = juce::jlimit (1, juce::jmax (1, numBins / 4),
                                       (int) std::round (0.5 * bw / binHz));

            // FUTURE 2: how many FRAMES one cycle of this tone lasts. Bin 0 has no
            // frequency, so it falls back to the frame constants like everything below
            // the point where the cycle window overtakes them.
            const double framesPerCycle = (f > 1.0)
                                        ? (sr / f) / (double) juce::jmax (1, hop)
                                        : 0.0;
            binAgeBase[(size_t) b] = juce::jmax ((float) cfg::futureAgeFrames,
                                        (float) (cfg::futureAgeCycles * framesPerCycle));
            binAgeMax[(size_t) b]  = juce::jmax (cfg::futureAgeMaxFrames,
                                        (int) std::lround (cfg::futureAgeMaxCycles * framesPerCycle));

            // A steady tone in bin b advances by exactly this much phase per hop.
            const double dphi = 2.0 * juce::MathConstants<double>::pi
                              * (double) b * (double) hop / (double) fftSize;
            rotRe[(size_t) b] = (float) std::cos (dphi);
            rotIm[(size_t) b] = (float) std::sin (dphi);
        }
    }


    // PERCEPTUAL WEIGHT — the thing that stops the detector being blind.
    // A classic compressor collapses the whole spectrum into one amplitude, so a kick
    // and a vocal at the same amplitude are literally the same event to it. Here every
    // tone arrives WITH its frequency, so it can be weighted by how much the ear
    // actually counts energy there.
    //
    // The curve is the BS.1770 K-weighting the LUFS meter in this plugin already uses,
    // evaluated analytically at each bin centre: an RLB high-pass at 38 Hz (the ear
    // barely counts sub energy) plus a +4 dB shelf above ~1.7 kHz (it counts presence
    // heavily). Analytic rather than filter-designed because we only need the magnitude.
    binWeight.assign ((size_t) numBins, 0.0f);
    {
        const double f0 = 38.13547087602444, q0 = 0.5003270373238773;   // RLB high-pass
        const double fs = 1681.974450955533;                            // shelf corner
        const double g  = std::pow (10.0, 3.999843853973347 / 20.0);    // shelf gain

        for (int b = 0; b < numBins; ++b)
        {
            const double f = (double) b * sr / (double) fftSize;
            const double f2 = f * f;

            const double hpDen = std::sqrt ((f0 * f0 - f2) * (f0 * f0 - f2)
                                          + (f0 * f / q0) * (f0 * f / q0));
            const double hp = (hpDen > 1.0e-12) ? (f2 / hpDen) : 0.0;

            const double r  = f / fs;
            const double sh = std::sqrt ((1.0 + r * r * g * g) / (1.0 + r * r));

            binWeight[(size_t) b] = (float) (hp * sh);
        }
    }

    // Ballistics run once per FRAME, not per sample, so the coefficients are built
    // from the hop duration.
    const float hopSec = (float) hop / (float) juce::jmax (1.0, sr);
    auto coeff = [hopSec] (float ms)
    {
        return 1.0f - std::exp (-hopSec / juce::jmax (1.0e-4f, 0.001f * ms));
    };
    atkC   = coeff (cfg::futureAtkMs);
    relC   = coeff (cfg::futureRelMs);
    memC   = coeff (cfg::futureMemMs);
    peakC  = coeff (cfg::futurePeakMemMs);
    // The peak follower now lives on a dB scale, so its decay is expressed as a constant
    // fall per frame — the same slope the old multiplicative follower had.
    peakDropDb = -20.0f * std::log10 (juce::jmax (1.0e-6f, 1.0f - peakC));
    tonalC = coeff (cfg::futureTonalSmMs);

    binAtkC.assign     ((size_t) numBins, atkC);
    binRelC.assign     ((size_t) numBins, relC);
    // Was MISSING. envCoeff() indexes it on every frame, so an empty vector meant a
    // read past the end of memory — which is what made the host disable the plugin
    // with "an error during processing".
    binRelFastC.assign ((size_t) numBins, relC);
    binPeakMag.assign  ((size_t) numBins, 0.0f);
    binPeakWDb.assign  ((size_t) numBins, -200.0f);
    timingAtk01 = timingRel01 = -1.0f;   // force a rebuild on the first block

    flush();
}

// ATTACK and RELEASE, reinvented. The knobs no longer mean milliseconds — they mean
// PERIODS OF EACH TONE'S OWN WAVE.
//
// Why: a fixed time means completely different things at different frequencies. An
// attack of 5 ms on a 50 Hz bass note is a QUARTER of one cycle, so the gain moves
// inside a single oscillation — that is not dynamics any more, it is modulation, and
// it is exactly where a compressor dirties the bass. The same 5 ms on a hi-hat is an
// eternity. One number cannot serve both, which is why the old knobs always felt like
// a compromise.
//
// Here every tone arrives with its frequency, so "one cycle" is a number we actually
// have. Setting the attack in cycles makes the bass physically impossible to modulate
// while the top end is still grabbed instantly — and it does that from ONE knob
// position, with no per-band tuning at all.
void HouseCompAudioProcessor::PartialEngine::setTiming (float atkCycles, float relBeats,
                                                        double bpm, bool smartRelease)
{
    const float atk01 = juce::jlimit (0.25f, 8.0f, atkCycles);
    relBeats = juce::jlimit (0.01f, 8.0f, relBeats);
    if (! (bpm > 1.0 && bpm < 1000.0)) bpm = cfg::futureFallbackBpm;

    smartRel = smartRelease;

    if (std::abs (atk01 - timingAtk01) < 1.0e-4f
     && std::abs (relBeats - timingRel01) < 1.0e-5f
     && std::abs (bpm - timingBpm) < 1.0e-3)
        return;                                  // nothing moved: no need to rebuild

    timingAtk01 = atk01;
    timingRel01 = relBeats;
    timingBpm   = bpm;
    rebuildTiming();
}

void HouseCompAudioProcessor::PartialEngine::rebuildTiming()
{
    if (numBins <= 0 || sampleRate <= 0.0) return;

    // ATTACK: in cycles of each tone's own wave, taken verbatim from the knob.
    const float atkCycles = timingAtk01;

    // RELEASE: a NOTE VALUE, converted through the project tempo. Milliseconds tuned at
    // 128 BPM are simply wrong at 124; a note length stays right at any tempo, which is
    // what dance music actually needs. The tempo is the one SET IN THE PROJECT — the
    // host hands it over, it is not guessed from the audio.
    const float relBeats = timingRel01;          // already in beats (see setTiming)
    const float beatMs   = (float) (60000.0 / timingBpm);
    const float relMs    = juce::jlimit (cfg::futureRelFloorMs, cfg::futureRelCeilMs,
                                         relBeats * beatMs);

    const float hopSec = (float) hop / (float) sampleRate;
    const float binHz  = (float) sampleRate / (float) fftSize;

    for (int b = 0; b < numBins; ++b)
    {
        // Bin 0 is DC and has no period; treat it as the lowest real tone.
        const float f = juce::jmax (binHz, (float) b * binHz);
        const float periodMs = 1000.0f / f;

        const float atkMs = juce::jlimit (cfg::futureAtkFloorMs, cfg::futureAtkCeilMs,
                                          atkCycles * periodMs);

        // The release is the SAME beat fraction for every tone, deliberately.
        //
        // I did try scaling it with frequency, because at 8 kHz the attack is ~1 ms while
        // the release stays at a full beat — an uneven-looking ratio. But that reads as
        // a defect only on paper: holding the top end to the beat is what gives BEAT its
        // character, and shortening it just turned BEAT into SMART. Two switch positions
        // that behave alike are worth nothing, so the difference stays:
        //   BEAT  = every tone locked to the grid, denser and more rhythmic up top.
        //   SMART = tones let go once they have actually died, cleaner and more open.
        binAtkC[(size_t) b]     = 1.0f - std::exp (-hopSec / (0.001f * atkMs));
        binRelC[(size_t) b]     = 1.0f - std::exp (-hopSec / (0.001f * relMs));
        binRelFastC[(size_t) b] = 1.0f - std::exp (-hopSec
                                       / (0.001f * juce::jmax (cfg::futureRelFloorMs,
                                                    relMs * cfg::futureRelSmartScale)));
    }
}

// ALL MIX for FUTURE. The four knobs become a per-tone scale on AMOUNT rather than four
// separate compressors fed by crossovers.
//
// This is where the partial engine pays off: a classic multiband must SPLIT the signal
// with filters, compress each piece and add them back, which is exactly where its phase
// dips and smearing at the band edges come from. Here the spectrum is already in pieces,
// so there is nothing to split — the "bands" are just a weighting, blended smoothly
// across each boundary. And inside a band the tones stay independent: a kick and a bass
// note both sitting in LOW keep their own envelopes instead of sharing one band gain.
void HouseCompAudioProcessor::PartialEngine::setBandAmounts (const float* amt4, float mix01)
{
    if (numBins <= 0) return;

    const float binHz = (float) sampleRate / (float) fftSize;
    // Same split points as the classic 4-band mode, so the knobs mean the same thing.
    const float edge[3] = { cfg::bandSplit1Hz, cfg::bandSplit2Hz, cfg::bandSplit3Hz };
    const float blend = juce::jmax (0.01f, cfg::futureBandBlendOct);

    for (int b = 0; b < numBins; ++b)
    {
        const float f = juce::jmax (binHz, (float) b * binHz);

        // Weight of each band at this frequency: 1 inside, fading over `blend` octaves
        // either side of a boundary. No hard edges anywhere.
        float w[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
        for (int e = 0; e < 3; ++e)
        {
            // 0 well below the edge, 1 well above it, smooth in between.
            const float oct = std::log2 (f / edge[e]) / blend;
            const float t = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * oct);
            const float s = t * t * (3.0f - 2.0f * t);      // smoothstep

            w[e + 1] = w[e] * s;                            // what moves up a band
            w[e]     = w[e] * (1.0f - s);                   // what stays behind
        }

        float amt = 0.0f, sum = 0.0f;
        for (int i = 0; i < 4; ++i) { amt += w[i] * amt4[i]; sum += w[i]; }
        amt = (sum > 1.0e-6f) ? (amt / sum) : 1.0f;

        // Crossfade toward the band scaling so switching ALL MIX is click-free, exactly
        // as the classic path does with allMixFade.
        binAmtScale[(size_t) b] = 1.0f + (amt - 1.0f) * juce::jlimit (0.0f, 1.0f, mix01);
    }
}

float HouseCompAudioProcessor::PartialEngine::envCoeff (int b, float target, float current) const
{
    if (target < current)
        return binAtkC[(size_t) b];          // gain falling = compressing = attack

    // SMART: the beat fraction is a MAXIMUM, not a wait. A tone that has already died
    // away is let go early instead of holding an empty pause — which is exactly where a
    // purely tempo-locked release is heard as "something is being held down" (a skipped
    // kick, a syncopation). It also puts back what one global tempo cannot express: a
    // reverb tail and a hat do not decay alike, so each tone gets to leave on its own.
    if (smartRel)
    {
        if (future2)
        {
            // Both figures are CRITICAL-BAND levels in dB, so this is a straight
            // subtraction, judged at the ear's resolution rather than the FFT grid's.
            const float peak = binPeakWDb[(size_t) b];
            if (peak > -190.0f && binWDb[(size_t) b] - peak < -cfg::futureRelDropDb)
                return binRelFastC[(size_t) b];
        }
        else
        {
            const float peak = binPeakMag[(size_t) b];
            const float mag  = binMag[(size_t) b];
            if (peak > 1.0e-9f
                && juce::Decibels::gainToDecibels (mag / peak, -120.0f) < -cfg::futureRelDropDb)
                return binRelFastC[(size_t) b];
        }
    }

    return binRelC[(size_t) b];
}

// BOND, step 1: find the loudest tones and learn which of them fire TOGETHER.
//
// Only the loudest few are considered, because a full pairwise map grows with the
// square of the tone count and almost all of it would be noise anyway. The ones that
// matter — kick, sub, lead — are by definition the loud ones.
void HouseCompAudioProcessor::PartialEngine::updateBonds()
{
    // Pick this frame's anchors: the strongest tones, each taken as a local maximum so
    // one broad peak does not occupy every anchor slot with its own neighbours.
    const int want = juce::jlimit (2, (int) kMaxAnchors, cfg::bondAnchors);
    anchorCount = 0;

    for (int pass = 0; pass < want; ++pass)
    {
        int   best = -1;
        float bestMag = 0.0f;
        for (int b = 2; b < numBins - 1; ++b)
        {
            const float m = binMag[(size_t) b] * binWeight[(size_t) b];
            if (m <= bestMag) continue;
            if (binMag[(size_t) b] < binMag[(size_t) (b - 1)]
             || binMag[(size_t) b] < binMag[(size_t) (b + 1)]) continue;   // local max only

            bool tooClose = false;
            for (int a = 0; a < anchorCount; ++a)
                if (std::abs (b - anchorBin[a]) < 3) { tooClose = true; break; }
            if (tooClose) continue;

            best = b; bestMag = m;
        }
        if (best < 0) break;
        anchorBin[anchorCount++] = best;
    }

    // Which anchors just started? Reuse the onset test the age protection already uses,
    // so "an event" means the same thing everywhere in the engine.
    for (int a = 0; a < anchorCount; ++a)
        anchorOnset[a] = binOnset[(size_t) anchorBin[a]] != 0;

    // Learn. Two anchors that fire in the same frame gain agreement; one firing alone
    // loses it. Slow on both sides, so a single coincidence proves nothing.
    for (int i = 0; i < anchorCount; ++i)
    {
        if (! anchorOnset[i]) continue;
        for (int j = 0; j < anchorCount; ++j)
        {
            if (i == j) continue;
            const float target = anchorOnset[j] ? 1.0f : 0.0f;
            bondScore[i][j] += (target - bondScore[i][j]) * cfg::bondForget;
            if (bondEvents[i][j] < 1000000) ++bondEvents[i][j];
        }
    }
}

// BOND, step 2: pull bonded tones toward a shared gain — but only where all four
// guards agree that the pair really is one source.
void HouseCompAudioProcessor::PartialEngine::applyBonds()
{
    const float binHz = (float) sampleRate / (float) fftSize;

    for (int i = 0; i < anchorCount; ++i)
    {
        const int bi = anchorBin[i];
        const float fi = juce::jmax (binHz, (float) bi * binHz);

        // Gather the gain this tone SHOULD have if its partners are taken into account.
        float sum = gainTmp[(size_t) bi], wsum = 1.0f;

        for (int j = 0; j < anchorCount; ++j)
        {
            if (i == j) continue;

            // GUARD 1 — agreement. Below this it is coincidence, not a relationship.
            if (bondScore[i][j] < cfg::bondMinAgreement) continue;

            // GUARD 2 — time. A pair has to prove itself over many onsets, not one.
            if (bondEvents[i][j] < cfg::bondMinEvents) continue;

            // GUARD 3 — distance. A kick at 50 Hz and a hat at 8 kHz can hit perfectly
            // in time all night and still be DIFFERENT instruments. Giving them one
            // gain is precisely the classic pumping this engine was built to avoid, so
            // frequency distance is a hard veto no amount of agreement can override.
            const int bj = anchorBin[j];
            const float fj = juce::jmax (binHz, (float) bj * binHz);
            const float octaves = std::abs (std::log2 (fj / fi));
            if (octaves > cfg::bondMaxOctaves) continue;

            const float w = bondScore[i][j];
            sum  += gainTmp[(size_t) bj] * w;
            wsum += w;
        }

        if (wsum <= 1.0f) continue;             // nothing bonded to this one

        // GUARD 4 — strength. Even a confident bond only pulls PART of the way, so each
        // tone keeps some dynamics of its own. A full merge would turn a bonded group
        // back into exactly the shared gain we removed.
        const float shared = sum / wsum;
        const float blended = gainTmp[(size_t) bi]
                            + (shared - gainTmp[(size_t) bi]) * cfg::bondStrength;

        // Apply across the tone's own lobe, not a single bin, so the peak keeps shape.
        for (int o = -1; o <= 1; ++o)
        {
            const int idx = bi + o;
            if (idx >= 0 && idx < numBins) gainTmp[(size_t) idx] = blended;
        }
    }
}

// Compress each tone on its own. This is the whole point of the engine: there is no
// shared gain anywhere in here, so there is nothing that can pump, and a loud kick
// cannot drag the hats down with it — they are different tones with different envelopes.
void HouseCompAudioProcessor::PartialEngine::applyDynamics()
{
    float* gain = binGain.data();
    float* prev = binPrevMag.data();
    int*   age  = binAge.data();

    int* ageLim = binAgeLimit.data();

    // 1. TONE OR NOISE, first — because the weighted level below now depends on it.
    //    A real tone's phase advances by the same amount every hop; noise does not.
    //    Dividing out the expected rotation separates hats and air from kick and bass.
    for (int b = 0; b < numBins; ++b)
    {
        // Mid (L+R) rather than left alone: the gain decision is stereo-linked, so the
        // tonality behind it has to be too. Judging by one channel would call a tone
        // "noise" whenever it happened to sit mostly in the other one.
        const float re = spec[0][(size_t) (2 * b)]     + spec[1][(size_t) (2 * b)];
        const float im = spec[0][(size_t) (2 * b + 1)] + spec[1][(size_t) (2 * b + 1)];
        const float pr = binPrevRe[(size_t) b],     pi = binPrevIm[(size_t) b];

        // d = z * conj(zPrev) * conj(expectedRotation); a steady tone leaves d pointing
        // straight along the real axis.
        const float cr = re * pr + im * pi;
        const float ci = im * pr - re * pi;
        const float dr = cr * rotRe[(size_t) b] + ci * rotIm[(size_t) b];
        const float di = ci * rotRe[(size_t) b] - cr * rotIm[(size_t) b];

        const float dm = std::sqrt (dr * dr + di * di);
        const float t = (dm > 1.0e-20f) ? juce::jlimit (0.0f, 1.0f, dr / dm) : 0.0f;
        binTonal[(size_t) b] += (t - binTonal[(size_t) b]) * tonalC;

        binPrevRe[(size_t) b] = re;
        binPrevIm[(size_t) b] = im;
    }

    // 2. Magnitude per tone, weighted by how much the ear counts it. STEREO-LINKED:
    //    the louder of the two channels defines the tone, so one gain serves both and
    //    the stereo image cannot drift.
    for (int b = 0; b < numBins; ++b)
    {
        const float reL = spec[0][(size_t) (2 * b)],     imL = spec[0][(size_t) (2 * b + 1)];
        const float reR = spec[1][(size_t) (2 * b)],     imR = spec[1][(size_t) (2 * b + 1)];
        const float mag = juce::jmax (std::sqrt (reL * reL + imL * imL),
                                      std::sqrt (reR * reR + imR * imR));
        binMag[(size_t) b] = mag;

        // The weight carries BOTH stories at once: how much the ear counts energy at
        // this frequency (K-weighting) AND whether this is a tone or noise. Noise reads
        // quieter, so it crosses the threshold less often through the ordinary
        // mechanism — no separate multiplier, and no tug of war with the K-weighting,
        // which is what the old tonalScale created.
        const float tonalW = cfg::futureNoiseScale
                           + (1.0f - cfg::futureNoiseScale) * binTonal[(size_t) b];
        const float w = mag * binWeight[(size_t) b] * tonalW;
        gainTmp[(size_t) b] = w * w;      // weighted ENERGY, summed into bands below

        // FUTURE: the peak this tone is measured against, taken from its own bin.
        if (! future2)
        {
            float& pk = binPeakMag[(size_t) b];
            pk = (mag > pk) ? mag : pk + (mag - pk) * peakC;
        }
    }

    // 3. CRITICAL-BAND LEVELS. This is the step that makes the top end register at all.
    //
    //    A tone and a noise spread their energy completely differently. A kick at
    //    -6 dBFS sits in about four bins, so each bin reads -12. A hi-hat at -20 dBFS is
    //    smeared over hundreds of bins, so each bin reads about -47. As EVENTS they are
    //    14 dB apart; PER BIN they are 35 dB apart. With the threshold 26 dB below the
    //    loudest bin, the hat's bins land below it and were never compressed at all, at
    //    any AMOUNT — which is exactly what you hear.
    //
    //    Judging a bin by the energy in the ear's critical band around it fixes that,
    //    and it is self-adjusting: a TONE keeps nearly all its energy in a couple of
    //    bins, so its band total is barely more than its own bin and per-tone
    //    independence survives. NOISE has its energy spread, so the band total gathers
    //    it back up and the hat finally reads like a hat.
    //
    //    Done with a running prefix sum so a band total costs one subtraction rather
    //    than a loop — the widest bands up top are ~70 bins across.
    prefix[0] = 0.0f;
    for (int b = 0; b < numBins; ++b)
        prefix[(size_t) (b + 1)] = prefix[(size_t) b] + gainTmp[(size_t) b];

    for (int b = 0; b < numBins; ++b)
    {
        const int h  = bandHalf[(size_t) b];
        const int lo = juce::jmax (0, b - h);
        const int hi = juce::jmin (numBins, b + h + 1);
        const float e = prefix[(size_t) hi] - prefix[(size_t) lo];
        const float wDb = 10.0f * std::log10 (juce::jmax (1.0e-20f, e));
        binWDb[(size_t) b] = wDb;

        // Running peak this tone is measured against, so SMART can tell whether it has
        // actually died away, and so AUTO can time how long it rings. Jumps up instantly,
        // falls at a fixed slope — otherwise the peak would track the tone down and
        // nothing would ever look "quiet".
        //
        // FUTURE 2 only: measured on the BAND, for the same reason its onset test is.
        // One bin of noise swings at random, and a Rayleigh-distributed magnitude sits
        // 6 dB under its own peak about one frame in twenty, so SMART kept dropping into
        // its fast release on hats and air with nothing having happened. Averaging dozens
        // of bins cancels that jitter while a real decay still shows.
        if (future2)
        {
            float& pk = binPeakWDb[(size_t) b];
            pk = (wDb > pk) ? wDb : pk - peakDropDb;
        }
    }

    // 4. Threshold hangs off a slow memory of the LOUDEST tone, so it follows the
    //    material instead of an absolute level the user would have to dial in.
    //
    //    The memory is FROZEN during silence. Left running, it decays through a pause
    //    and drags the threshold toward the floor — then the first hit after the pause
    //    arrives far above it and gets slammed. Holding the last real level instead
    //    means the engine picks up exactly where the music left off.
    //    The reference is the loudest BAND, not the loudest bin, so it is on the same
    //    scale as what it is compared against. (No feedback risk: the analysis FIFO is
    //    fed from the plugin's input, never from its own output, so every frame is
    //    measured on clean audio.)
    float loudestDb = -120.0f;
    for (int b = 0; b < numBins; ++b)
        loudestDb = juce::jmax (loudestDb, binWDb[(size_t) b]);

    // SEED THE MEMORY FROM THE FIRST REAL FRAME instead of crawling up from -60 dB.
    //
    // The threshold hangs off this memory, so while the memory is still down at its
    // starting value every tone reads as tens of dB over the threshold and the engine
    // slams into full reduction on all of them at once. It recovers as the memory
    // catches up over ~400 ms — which is exactly the dip heard when a track starts.
    //
    // The first frame already knows the real level; there is nothing to converge to.
    // Only a frame with actual signal counts, so silence before the transport rolls
    // cannot seed it at the noise floor.
    if (! lvlMemSeeded)
    {
        if (loudestDb > -80.0f) { lvlMemDb = loudestDb; lvlMemSeeded = true; }
    }
    else if (loudestDb > lvlMemDb - 40.0f || lvlMemDb <= -59.0f)
    {
        lvlMemDb += (loudestDb - lvlMemDb) * memC;
    }
    const float threshDb   = lvlMemDb - cfg::futureThreshDb;
    const float upThreshDb = lvlMemDb - cfg::futureUpThreshDb;
    const float upFloorDb  = lvlMemDb - cfg::futureUpFloorDb;
    const float slope = 1.0f - 1.0f / juce::jmax (1.0f, cfg::futureRatio);

    // 5. MASKING BETWEEN TONES. A loud kick physically hides its neighbours, and a
    //    tone you cannot hear is a tone there is no point compressing. Each tone casts
    //    a shadow that spreads strongly UPWARD in frequency and weakly downward; a
    //    tone sitting deep under someone else's shadow is left alone.
    //    Two O(n) sweeps rather than an O(n^2) convolution — the same trick the mask
    //    clipper uses on its six bands, just at bin resolution.
    for (int b = 0; b < numBins; ++b) binSpread[(size_t) b] = binWDb[(size_t) b];
    for (int b = 1; b < numBins; ++b)
        binSpread[(size_t) b] = juce::jmax (binSpread[(size_t) b],
                                            binSpread[(size_t) (b - 1)] - upSlope[(size_t) b]);
    for (int b = numBins - 2; b >= 0; --b)
        binSpread[(size_t) b] = juce::jmax (binSpread[(size_t) b],
                                            binSpread[(size_t) (b + 1)] - downSlope[(size_t) b]);

    // 6. Target gain per tone.
    for (int b = 0; b < numBins; ++b)
    {
        const float wDb = binWDb[(size_t) b];

        // AUDIBILITY — ONE factor, replacing the old exposure x tonalScale pair.
        //
        // Those two answered the same question ("does compressing this tone matter to
        // what is actually heard?") in different words, and multiplying them produced a
        // figure nobody could predict: 0.5 x 0.6 left a third of the intended reduction
        // with no way to see why. Worse, they pulled against each other — the
        // K-weighting lifts the top ~4 dB saying "hats read louder", then tonalScale cut
        // their compression by 40% saying "hats matter less".
        //
        // The noise consideration now lives in the DETECTOR (see where binWDb is built),
        // which is where it belongs: noise that reads quieter crosses the threshold less
        // often and is compressed less through the ordinary mechanism, with no second
        // multiplier and no contradiction. What remains here is one thing only — how far
        // this tone sits under someone else's shadow.
        const float shadow = binSpread[(size_t) b] - wDb;
        const float audibility = juce::jlimit (0.0f, 1.0f,
                                    1.0f - shadow / juce::jmax (1.0f, cfg::futureMaskDepthDb));

        // AMOUNT for THIS tone. Normally the global knob; with ALL MIX on, scaled by
        // whichever band knob owns this frequency (blended across the boundaries).
        const float amtHere = amount01 * binAmtScale[(size_t) b];

        float redDb = 0.0f;
        const float over = wDb - threshDb;
        if (over > 0.0f)
        {
            // futureCompScale is applied OUTSIDE the cap on purpose, so the deepest
            // reductions are pulled back by the same proportion as the shallow ones.
            // Inside it, anything already at the ceiling would ignore the scale entirely
            // and the loudest tones — the ones that set the loudness — would not move.
            redDb = -juce::jmin (cfg::futureMaxRedDb, over * slope * amtHere)
                  * audibility * cfg::futureCompScale;
        }

        // UPWARD: lift quiet tones — reverb tails, air, detail that normally sinks.
        // Safe here in a way broadband upward compression is not: with no shared gain
        // it can only lift tones that actually exist, masked ones are skipped, and the
        // floor stops it dragging noise out of the silence between hits.
        if (upward01 > 0.0f && over <= 0.0f && wDb > upFloorDb)
        {
            // NOTE: audibility is deliberately NOT applied here, unlike in the
            // compression branch above. For compression it is right — a tone you cannot
            // hear does not need holding down. For a LIFT it is backwards: a quiet,
            // masked tail is precisely the thing you are reaching for, and gating it by
            // audibility made the feature refuse to work exactly where it was meant to.
            // With a masked tail at audibility ~0.3 the 3 dB ceiling became 0.9 dB —
            // under what the ear resolves, which is why the knob did nothing at 100%.
            const float under = upThreshDb - wDb;
            if (under > 0.0f)
            {
                // The ramp reaches the ceiling after about 9 dB and then HOLDS it. That
                // plateau is not a defect — it is the saturation an upward compressor is
                // supposed to have, and it keeps the curve monotone: the quieter a tone
                // is, the more it is lifted, up to a bounded maximum.
                //
                // (Stretching the ramp across the whole window was tried and reverted. It
                // made the shape a triangle peaking mid-window, so lift FELL again for the
                // very quietest tones — backwards for a feature whose whole job is to
                // reach them — and halved the lift over the range most material sits in.
                // Audibly weaker, which is exactly how it was caught.)
                const float lift = juce::jmin (cfg::futureUpMaxDb, under * slope);

                // FADE OUT toward the floor instead of stopping dead at it. The floor
                // used to be a plain boolean, so a tone 59.9 dB under got the whole
                // +7 dB and one 0.2 dB quieter got nothing — a 7 dB step exactly where
                // this feature aims. A reverb tail decaying past the floor lost the
                // entire lift in a single frame: a gate breathing, backwards.
                const float fade = juce::jlimit (0.0f, 1.0f,
                                       (wDb - upFloorDb) / juce::jmax (1.0f, cfg::futureUpFadeDb));

                redDb = lift * upward01 * fade;
            }
        }

        // AGE OF THE TONE. A tone whose level just jumped is an ONSET — leave its
        // attack completely alone and phase the compression in afterwards. This is what
        // dissolves the old "fast attack kills punch" trade-off: punch is preserved PER
        // TONE, so the body of the kick can be squeezed as hard as you like while its
        // leading edge stays untouched.
        // How LONG it stays protected scales with how sharply it jumped: a kick's attack
        // is short, a pad's is long, and one fixed number cannot serve both.
        //
        // FUTURE reads the jump from the tone's own BIN. FUTURE 2 reads it from the
        // CRITICAL BAND, because a single bin of noise fluctuates at random — a 3 dB jump
        // between frames is ordinary there, not an event — so the per-bin test fires on
        // roughly one frame in five for anything noise-like. Every false onset resets the
        // age to zero and the ramp then holds the reduction down, so hats, snare bodies
        // and reverb tails end up quietly under-compressed whatever the knobs say. Tonal
        // content never shows the fault: a steady tone's bin is steady.
        //
        // That under-compression is also part of what FUTURE sounds like, which is why
        // removing it belongs in a mode of its own rather than in both.
        const float mag = binMag[(size_t) b];
        const float onsetRiseDb = 20.0f * std::log10 (juce::jmax (1.001f, cfg::futureOnsetRise));

        binOnset[(size_t) b] = future2
                             ? (binWDb[(size_t) b] > prev[b] + onsetRiseDb ? 1 : 0)
                             : (mag > prev[b] * cfg::futureOnsetRise       ? 1 : 0);

        // MEASURE THE DECAY, for AUTO. Count the frames from a tone's onset until it
        // has fallen away, and average that across tones. This is a measurement, not a
        // guess: it is how long this particular material actually rings.
        if (binOnset[(size_t) b])
        {
            binSinceOnset[(size_t) b] = 0;
        }
        else if (binSinceOnset[(size_t) b] >= 0)
        {
            ++binSinceOnset[(size_t) b];

            // Has it fallen away yet, and if so how much does its vote count?
            // WEIGHTED by loudness because there are ~1000 bins and the vast majority sit
            // up high, where everything decays in milliseconds — the kick was outvoted a
            // hundred to one and AUTO always landed on the shortest note. Squaring makes
            // the dominant tones decide, which is what you actually hear ring out.
            bool  died = false;
            float rel  = 0.0f;

            if (future2)
            {
                const float pk = binPeakWDb[(size_t) b];
                died = (pk > -190.0f && wDb - pk < -cfg::futureRelDropDb);
                // Against the loudest BAND, so reference and peak share a scale. Divided
                // by the band's width so each critical band casts ONE vote: measured per
                // bin, a hat's band spans ~150 bins against the kick's ~5 and would win
                // on sheer count, which is the very bug the weighting exists to stop.
                if (died)
                    rel = juce::Decibels::decibelsToGain (pk - loudestDb)
                        / (float) (2 * bandHalf[(size_t) b] + 1);
            }
            else
            {
                const float pk = binPeakMag[(size_t) b];
                died = (pk > 1.0e-7f
                        && juce::Decibels::gainToDecibels (mag / pk, -120.0f) < -cfg::futureRelDropDb);
                if (died)
                    rel = pk * binWeight[(size_t) b] / juce::jmax (1.0e-9f, inputLoudest);
            }

            if (died)
            {
                const float sec = (float) binSinceOnset[(size_t) b] * (float) hop / (float) sampleRate;
                const float w = juce::jlimit (0.0f, 1.0f, rel) * juce::jlimit (0.0f, 1.0f, rel);

                decaySec += (juce::jlimit (0.02f, 2.0f, sec) - decaySec) * 0.05f * w;
                binSinceOnset[(size_t) b] = -1;      // wait for the next onset
            }
        }

        if (binOnset[(size_t) b])
        {
            age[b] = 0;

            if (future2)
            {
                // How sharply it jumped, on the same band scale as the test above.
                const float riseDb = binWDb[(size_t) b] - prev[b];
                // The protection window is in CYCLES OF THIS TONE, not in frames — the
                // same argument that made ATTACK cycles-based. Three frames is 32 ms: at
                // 8 kHz that is 250 cycles, at 50 Hz barely one and a half. One number
                // cannot mean the same thing at both ends, and the end it shortchanges is
                // the low one, which is exactly where the punch lives. Floored at the
                // frame counts, so nothing above ~100 Hz moves at all.
                const int lim = (int) (binAgeBase[(size_t) b] * riseDb / onsetRiseDb);
                ageLim[b] = juce::jlimit (1, binAgeMax[(size_t) b], lim);
            }
            else
            {
                const float rise = mag / juce::jmax (prev[b], 1.0e-12f);
                ageLim[b] = juce::jlimit (1, cfg::futureAgeMaxFrames,
                                (int) ((float) cfg::futureAgeFrames * rise / cfg::futureOnsetRise));
            }
        }
        else if (age[b] < ageLim[b])
        {
            ++age[b];
        }

        if (ageLim[b] > 0 && age[b] < ageLim[b])
            redDb *= (float) age[b] / (float) ageLim[b];

        // Whatever the onset test compares, prev has to hold the same thing.
        prev[b] = future2 ? binWDb[(size_t) b] : mag;
        gainTmp[(size_t) b] = juce::Decibels::decibelsToGain (redDb);
    }

    // 7. BOND. Tones that consistently start together are treated as one source and
    //     pulled toward a shared gain — see the note on the struct. Runs BEFORE the
    //     harmonic link and the frequency smoothing, so both still get the last word.
    if (bondOn)
    {
        updateBonds();
        applyBonds();
    }

    // 8. HARMONIC LINKING. 50 Hz and 150 Hz are ONE kick, not two independent tones.
    //    Compressing them by different amounts changes its timbre, so pull the
    //    harmonics' gain toward the fundamental's: the kick gets more even without
    //    stopping sounding like itself. This is the step from "per bin" toward "per
    //    note" — but done from a known fundamental, so there is no fragile partial
    //    tracking that could lose or swap notes.
    if (cfg::futureHarmonicLink > 0.0f && numBins > 4)
    {
        const float binHz = (float) sampleRate / (float) fftSize;
        const int maxFundBin = juce::jlimit (2, numBins - 1, (int) (cfg::futureHarmonicMaxHz / binHz));

        int   f0 = 0;
        float best = 0.0f;
        for (int b = 2; b <= maxFundBin; ++b)
            if (binWDb[(size_t) b] > -120.0f && binMag[(size_t) b] > best)
                { best = binMag[(size_t) b]; f0 = b; }

        // HYSTERESIS. Chosen fresh each frame, the fundamental flips between the kick
        // and whatever bass note is sounding, and each flip re-points the entire set of
        // linked harmonics — heard as a step in the gain. A new candidate has to be
        // clearly louder than the one already held before it takes over.
        if (prevF0 >= 2 && prevF0 < numBins
            && best < binMag[(size_t) prevF0] * 1.5f)
            f0 = prevF0;

        if (f0 >= 2 && best > 1.0e-9f)
        {
            prevF0 = f0;
            const float gFund = gainTmp[(size_t) f0];

            for (int k = 2; k <= cfg::futureHarmonicCount; ++k)
            {
                const int hb = f0 * k;
                if (hb >= numBins - 1) break;

                // Cover the harmonic's CRITICAL BAND, not just three bins. The gain
                // smoothing that runs next averages over that same width, so a narrow
                // edit here would simply be diluted away: at 2.4 kHz it was setting 3
                // bins out of a 17-bin window, leaving under a fifth of the intended
                // link. Matching the widths means the smoothing preserves it instead.
                const int h  = bandHalf[(size_t) hb];
                const int lo = juce::jmax (0, hb - h);
                const int hi = juce::jmin (numBins - 1, hb + h);
                for (int idx = lo; idx <= hi; ++idx)
                    gainTmp[(size_t) idx] += (gFund - gainTmp[(size_t) idx]) * cfg::futureHarmonicLink;
            }
        }
    }

    // 9. Smooth the gain ACROSS FREQUENCY, over the ear's CRITICAL BAND at each bin.
    //
    //    Without smoothing, neighbouring bins get noticeably different gains and the
    //    result warbles. But the width has to follow hearing, not a fixed bin count:
    //    seven bins is an octave and a half at 50 Hz — which merged kick, sub and bass
    //    note into one shared gain, destroying the per-tone independence this engine
    //    exists for — and a hundredth of an octave at 8 kHz, where it did nothing.
    //
    //    Scaled by the same critical bandwidth used for the levels, so the engine
    //    smooths exactly as far as the ear cannot tell two events apart anyway.
    if (cfg::futureBinSmooth > 0)
    {
        // Prefix sum again: the bands are wide up top, so a per-bin loop would be
        // thousands of adds per frame.
        prefix[0] = 0.0f;
        for (int b = 0; b < numBins; ++b)
            prefix[(size_t) (b + 1)] = prefix[(size_t) b] + gainTmp[(size_t) b];

        for (int b = 0; b < numBins; ++b)
        {
            // futureBinSmooth now scales the critical band rather than counting bins.
            const int h  = juce::jmax (1, (bandHalf[(size_t) b] * cfg::futureBinSmooth) / 3);
            const int lo = juce::jmax (0, b - h);
            const int hi = juce::jmin (numBins, b + h + 1);
            const int cnt = hi - lo;

            const float target = (cnt > 0)
                               ? (prefix[(size_t) hi] - prefix[(size_t) lo]) / (float) cnt
                               : 1.0f;
            gain[b] += (target - gain[b]) * envCoeff (b, target, gain[b]);
        }
    }
    else
    {
        for (int b = 0; b < numBins; ++b)
        {
            const float target = gainTmp[(size_t) b];
            gain[b] += (target - gain[b]) * envCoeff (b, target, gain[b]);
        }
    }

    // 10. Apply the SAME gain to both channels, and report how much was taken off.
    //
    // The meter reads the LOUDNESS-WEIGHTED average reduction, not the deepest single
    // tone. Reporting the deepest was misleading: one quiet tone squeezed hard would
    // read as -12 dB while the mix was barely touched. What the ear registers is how
    // much came off the tones that are actually carrying the sound, which is what this
    // weights by.
    double wSum = 0.0, gSum = 0.0;
    for (int b = 0; b < numBins; ++b)
    {
        const float g = gain[b];
        spec[0][(size_t) (2 * b)]     *= g;
        spec[0][(size_t) (2 * b + 1)] *= g;
        spec[1][(size_t) (2 * b)]     *= g;
        spec[1][(size_t) (2 * b + 1)] *= g;

        // METERING. Average only over the tones that are ACTUALLY BEING COMPRESSED, and
        // average in dB rather than in linear gain.
        //
        // Averaging over every bin under-read the meter badly and systematically. Most of
        // the ~1000 bins are untouched at any moment — more so now the threshold only
        // catches the top — and those 1.0 gains diluted the reading toward nothing: a
        // kick held down by 6 dB occupies about five bins, so the average across the
        // spectrum reported tenths of a dB. Averaging linear gains and only then taking
        // the logarithm pulled it down again.
        //
        // This mattered beyond cosmetics. COMP was being compared against LIM, which
        // reports a true per-block MAXIMUM, so the panel was showing an average next to a
        // peak and inviting exactly the wrong conclusion ("the compressor is doing
        // nothing, the limiter took over"). The threshold was pushed from 14 to 26 partly
        // on that comparison.
        if (g < 0.999f)
        {
            const double w = (double) binMag[(size_t) b] * (double) binWeight[(size_t) b];
            wSum += w;
            gSum += w * (double) -juce::Decibels::gainToDecibels (g, -60.0f);
        }
    }
    maxRedDb = (wSum > 1.0e-12) ? juce::jmax (0.0f, (float) (gSum / wSum)) : 0.0f;
}

// Everything here is measured in units that differ between FUTURE and FUTURE 2 —
// binPrevMag holds a linear magnitude in one and a dB band level in the other — so it
// has to be cleared when the mode changes or the first frame after the switch compares
// two different scales and fires a false onset across the whole spectrum.
void HouseCompAudioProcessor::PartialEngine::resetEventState()
{
    std::fill (binPrevMag.begin(),  binPrevMag.end(),  -200.0f);
    std::fill (binPeakMag.begin(),  binPeakMag.end(),  0.0f);
    std::fill (binPeakWDb.begin(),  binPeakWDb.end(),  -200.0f);
    std::fill (binAge.begin(),      binAge.end(),      0);
    std::fill (binAgeLimit.begin(), binAgeLimit.end(), cfg::futureAgeFrames);
    std::fill (binOnset.begin(),      binOnset.end(),      0);
    std::fill (binSinceOnset.begin(), binSinceOnset.end(), -1);
}

void HouseCompAudioProcessor::PartialEngine::flush()
{
    for (auto& b : inFifo) std::fill (b.begin(), b.end(), 0.0f);
    for (auto& b : outAcc) std::fill (b.begin(), b.end(), 0.0f);
    for (auto& b : spec)   std::fill (b.begin(), b.end(), 0.0f);
    pos = 0;

    // EVERY per-bin state has to go, not just the obvious ones. Leaving the tonality
    // estimate, the peak memory or the onset ages behind means the engine starts the
    // next piece of material still believing things about the last one.
    std::fill (binGain.begin(),     binGain.end(),     1.0f);
    std::fill (binPrevMag.begin(),  binPrevMag.end(),  -200.0f);
    std::fill (binAge.begin(),      binAge.end(),      0);
    std::fill (binAgeLimit.begin(), binAgeLimit.end(), cfg::futureAgeFrames);
    std::fill (binPeakMag.begin(),  binPeakMag.end(),  0.0f);
    std::fill (binPeakWDb.begin(),  binPeakWDb.end(),  -200.0f);
    std::fill (binTonal.begin(),    binTonal.end(),    0.0f);
    std::fill (binPrevRe.begin(),   binPrevRe.end(),   0.0f);
    std::fill (binPrevIm.begin(),   binPrevIm.end(),   0.0f);
    std::fill (binOnset.begin(),      binOnset.end(),      0);
    std::fill (binSinceOnset.begin(), binSinceOnset.end(), -1);
    decaySec = 0.25f;

    // Learned bonds belong to the material that taught them, so a new piece must not
    // inherit them.
    anchorCount = 0;
    for (int i = 0; i < kMaxAnchors; ++i)
    {
        anchorBin[i] = 0;
        anchorOnset[i] = false;
        for (int j = 0; j < kMaxAnchors; ++j) { bondScore[i][j] = 0.0f; bondEvents[i][j] = 0; }
    }

    lvlMemDb = -60.0f;
    // Re-seed from the next frame that has signal. The host calls flush() on every
    // transport jump, so without this the level memory would only be seeded once per
    // session and every later start would crawl up from -60 again — the same dip.
    lvlMemSeeded = false;
    maxRedDb = 0.0f;
    prevF0 = 0;
}

void HouseCompAudioProcessor::PartialEngine::analyseFrame (int ch)
{
    const int N = fftSize;
    auto& s = spec[(size_t) ch];

    for (int k = 0; k < N; ++k)
        s[(size_t) k] = inFifo[(size_t) ch][(size_t) k] * window[(size_t) k];
    std::fill (s.begin() + N, s.end(), 0.0f);

    fft.performRealOnlyForwardTransform (s.data(), true);

    // Loudest INPUT tone, captured before applyDynamics touches anything — FUTURE weighs
    // AUTO's decay votes against it. Skipped entirely in FUTURE 2, which takes its
    // reference from the critical-band levels it already computes: that is both the right
    // scale and free, and it saves ~1000 square roots per channel per frame.
    if (future2) return;

    if (ch == 0) inputLoudest = 1.0e-9f;
    for (int b = 0; b < numBins; ++b)
    {
        const float re = s[(size_t) (2 * b)], im = s[(size_t) (2 * b + 1)];
        inputLoudest = juce::jmax (inputLoudest,
                                   std::sqrt (re * re + im * im) * binWeight[(size_t) b]);
    }
}

void HouseCompAudioProcessor::PartialEngine::synthFrame (int ch)
{
    const int N = fftSize, H = hop;
    auto& s = spec[(size_t) ch];

    fft.performRealOnlyInverseTransform (s.data());

    // Slide the accumulator along by one hop and clear the freshly exposed tail.
    std::copy (outAcc[(size_t) ch].begin() + H, outAcc[(size_t) ch].end(), outAcc[(size_t) ch].begin());
    std::fill (outAcc[(size_t) ch].end() - H, outAcc[(size_t) ch].end(), 0.0f);

    for (int k = 0; k < N; ++k)
        outAcc[(size_t) ch][(size_t) k] += s[(size_t) k] * window[(size_t) k];

    std::copy (inFifo[(size_t) ch].begin() + H, inFifo[(size_t) ch].end(), inFifo[(size_t) ch].begin());
}

void HouseCompAudioProcessor::PartialEngine::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int N = fftSize, H = hop;
    if (n <= 0 || N <= 0) return;

    float* chan[2] = { buffer.getWritePointer (0),
                       numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float xl = chan[0][i];
        float xr = (chan[1] != nullptr) ? chan[1][i] : xl;   // mono: mirror, so the
        if (! std::isfinite (xl)) xl = 0.0f;                 // stereo link does not
        if (! std::isfinite (xr)) xr = 0.0f;                 // see silence on the right

        inFifo[0][(size_t) (N - H + pos)] = xl;
        inFifo[1][(size_t) (N - H + pos)] = xr;

        chan[0][i] = outAcc[0][(size_t) pos] * outScale;
        if (chan[1] != nullptr) chan[1][i] = outAcc[1][(size_t) pos] * outScale;

        if (++pos >= H)
        {
            // Analyse BOTH channels first, then decide the gains once for the pair, then
            // synthesise. The decision has to sit between the two halves — that is what
            // makes it stereo-linked instead of two independent compressors that would
            // drift the image apart.
            analyseFrame (0);
            analyseFrame (1);

            if (amount01 > 0.0f)
                applyDynamics();      // at amount 0 this is skipped, so the round trip
                                      // stays the transparent one proven by test
            else
                maxRedDb = 0.0f;      // or the make-up that follows this would stay
                                      // lifted on a stale reading with nothing compressed

            synthFrame (0);
            synthFrame (1);
            pos = 0;
        }
    }
}

//==============================================================================
//  CREST ROTATOR — free headroom from an allpass. See the note in the header.
//==============================================================================

// RBJ allpass. Note b0 == a2 and b1 == a1 and b2 == 1: that symmetry is exactly what
// makes the magnitude response flat at every frequency, so this cannot colour the
// sound however far the search wanders.
void HouseCompAudioProcessor::CrestRotator::AP2::set (double f0, double q, double sr)
{
    const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / sr;
    const double cw    = std::cos (w0);
    const double sw    = std::sin (w0);
    const double alpha = sw / (2.0 * juce::jmax (0.05, q));
    const double a0    = 1.0 + alpha;

    b0 = (float) ((1.0 - alpha) / a0);
    b1 = (float) ((-2.0 * cw)   / a0);
    b2 = 1.0f;
    a1 = (float) ((-2.0 * cw)   / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

void HouseCompAudioProcessor::CrestRotator::updateCoeffs()
{
    const float lo = juce::jlimit (cfg::rotMinHz, cfg::rotMaxHz, fHz / cfg::rotProbeSpread);
    const float hi = juce::jlimit (cfg::rotMinHz, cfg::rotMaxHz, fHz * cfg::rotProbeSpread);

    for (auto& a : audio) a.set (fHz, cfg::rotQ, sampleRate);
    probe[0].set (lo,  cfg::rotQ, sampleRate);
    probe[1].set (fHz, cfg::rotQ, sampleRate);
    probe[2].set (hi,  cfg::rotQ, sampleRate);
}

void HouseCompAudioProcessor::CrestRotator::prepare (double sr)
{
    sampleRate = sr;
    winLen = juce::jmax (1, (int) (cfg::rotSearchSec * sr));
    fHz    = juce::jlimit (cfg::rotMinHz, cfg::rotMaxHz, 60.0f);
    updateCoeffs();
    flush();
}

void HouseCompAudioProcessor::CrestRotator::flush()
{
    for (auto& a : audio) a.reset();
    for (auto& p : probe) p.reset();
    for (int k = 0; k < 3; ++k) { pk[k] = 0.0; sq[k] = 0.0; }
    winPos = 0;
    dir = 0;
}

// One search window has elapsed: whichever probe saw the LOWEST crest factor wins, and
// the audio filter starts drifting that way. Crest factor is peak/RMS, which is
// gain-invariant — so this decision is unaffected by how loud the material is.
void HouseCompAudioProcessor::CrestRotator::decideDirection()
{
    int    best      = 1;
    double bestCrest = 1.0e30;

    for (int k = 0; k < 3; ++k)
    {
        const double rms = std::sqrt (sq[k] / (double) juce::jmax (1, winPos));
        if (rms < 1.0e-6) continue;            // silence: no opinion, hold position
        const double crest = pk[k] / rms;
        if (crest < bestCrest) { bestCrest = crest; best = k; }
    }

    dir = (bestCrest > 1.0e29) ? 0 : (best - 1);   // probe 0 = down, 1 = hold, 2 = up

    for (int k = 0; k < 3; ++k) { pk[k] = 0.0; sq[k] = 0.0; }
    winPos = 0;
}

void HouseCompAudioProcessor::CrestRotator::processBlock (juce::AudioBuffer<float>& buffer)
{
    const int n     = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        float xl = l[i];
        float xr = r != nullptr ? r[i] : xl;
        // These filters are recursive, so a stray NaN would poison them permanently.
        if (! std::isfinite (xl)) xl = 0.0f;
        if (! std::isfinite (xr)) xr = 0.0f;

        // Measurement only — audio does NOT pass through the probes.
        const float m = 0.5f * (xl + xr);
        for (int k = 0; k < 3; ++k)
        {
            const float y = probe[k].process (m);
            const double a = (double) std::abs (y);
            if (a > pk[k]) pk[k] = a;
            sq[k] += (double) y * (double) y;
        }

        l[i] = audio[0].process (xl);
        if (r != nullptr) r[i] = audio[1].process (xr);
    }

    winPos += n;
    if (winPos >= winLen)
        decideDirection();

    // Glide. Rate-limited to cfg::rotGlideRate per second, which is why a moving
    // allpass here does not read as a phaser sweep.
    if (dir != 0)
    {
        const float sec = (float) n / (float) sampleRate;
        const float next = juce::jlimit (cfg::rotMinHz, cfg::rotMaxHz,
                                         fHz * (1.0f + (float) dir * cfg::rotGlideRate * sec));
        if (next != fHz)
        {
            fHz = next;
            updateCoeffs();
        }
    }
}

//==============================================================================
void HouseCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool bypassOn = apvts.getRawParameterValue (ParamID::bypass)->load() > 0.5f;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();


    const float amountTarget  = apvts.getRawParameterValue (ParamID::amount)->load();
    const float inGainTarget   = apvts.getRawParameterValue (ParamID::inGain)->load();
    const bool  autoGainOn     = apvts.getRawParameterValue (ParamID::autoGain)->load() > 0.5f;
    // Clipper is ALWAYS on (you pick the FLAVOUR, not on/off). clipMode is the ladder:
    //   0 CLIP = ADAA1, 1 HI-Q = ADAA2, 2 MASK = PC-CFR in front of ADAA2.
    const int   clipMode      = juce::jlimit (0, 2,
                                  (int) apvts.getRawParameterValue (ParamID::clipMode)->load());
    const bool  clipHiQparam  = clipMode >= 1;   // MASK keeps the best ADAA order
    const bool  maskMode      = clipMode == 2;
    const bool  clipOn        = true;
    const float ceilingDb     = apvts.getRawParameterValue (ParamID::ceiling)->load();
    const bool  lookaheadOn   = apvts.getRawParameterValue (ParamID::lookahead)->load() > 0.5f;

    // SPECTRAL is now ALWAYS on (removed from the UI) — the multiband limiter is
    // core to the sound, so it's hard-wired here regardless of the stored param.
    const bool  specLimitOn   = true;

    // New optional modes (defaults keep the current sound).
    const float character   = apvts.getRawParameterValue (ParamID::character)->load(); // -1..+1
    const bool  smartOn     = apvts.getRawParameterValue (ParamID::smart)->load()    > 0.5f;
    // LUFS make-up is always on now (its AUTO | LUFS switch was removed from the panel).
    // Forced here in the engine rather than by writing the parameter from the editor —
    // writing it during editor construction reads as a user edit to the host, which then
    // re-applies state and wipes every other parameter.
    const bool  lufsGainOn  = true;
    juce::ignoreUnused (apvts.getRawParameterValue (ParamID::lufsGain));

    // SMART: hand the frequency-aware release flag to every limiter instance.
    for (auto& sl : specLim) sl.setSmart (smartOn);
    limiter.setSmart (smartOn);

    // CASCADE is now ALWAYS on (removed from the UI) — low-distortion smoothing is
    // always desirable, so it's hard-wired on.
    for (auto& sl : specLim) sl.setCascade (true);
    limiter.setCascade (true);

    // ENGINE: CLASSIC (false) = original sound. MODERN (true) = corrections on.
    // Flush the spectral phase-compensation allpasses when the engine switches so
    // the change is click-free.
    const int engineIdx = juce::jlimit (0, 3,
                            (int) apvts.getRawParameterValue (ParamID::engineMode)->load());
    // FUTURE builds on MODERN's corrections — it changes how dynamics are APPLIED, not
    // whether the chain's known fixes are in.
    const bool modernEngine = engineIdx >= 1;
    const bool futureEngine = engineIdx >= 2;
    // Critical-band event detection is what FUTURE now IS — value 3 is the same thing
    // under its trial name. The per-bin path behind this flag is the previous behaviour,
    // kept one version for a quick flip back; nothing on the panel selects it.
    const bool future2Now   = (engineIdx >= 2);
    if (future2Now != partialEng.future2)
    {
        partialEng.future2 = future2Now;
        partialEng.resetEventState();   // the two store different units — see the note
    }
    comp.modern = modernEngine;
    for (auto& bc : bandComp)
    {
        bc.modern = modernEngine;
        // The band compressors already SEE only their own band, so a detector HPF
        // there is wrong: on the LOW band (0-160 Hz) it would blind the detector to
        // the sub it is supposed to control. The other MODERN improvements (smoothed
        // envelope, soft reduction cap) still apply to every band.
        bc.useScHpf = false;
    }
    comp.useScHpf = true;   // wideband detector: this is where the HPF belongs
    limiter.setModern (modernEngine);
    for (auto& sl : specLim) sl.setModern (modernEngine);
    if (modernEngine != prevModernEngine)
    {
        for (auto& bb : specAp) for (auto& ss : bb) for (auto& f : ss) f.reset();
        for (auto& sl : specLim) sl.flush();
        prevModernEngine = modernEngine;
    }

    // NOTE: the engine change deliberately does NOT flush the partial engine. Zeroing
    // its overlap-add accumulator empties a whole window — 2048 samples, ~43 ms of
    // silence — which is exactly the dropout it was meant to prevent. Because both
    // paths now share the same latency, the switch is a straight handover and there is
    // nothing stale to clear.
    prevEngineIdx = engineIdx;

    // IRC IV: 6-band psychoacoustic spectral mode (vs 3-band). Flush the spectral
    // filters/limiters when the band count changes so there's no click.
    const int  limMode = juce::jlimit (0, 2,
                           (int) apvts.getRawParameterValue (ParamID::limMode)->load());
    const bool irc4On = limMode >= 1;

    // PHASE is retired. The physics held up — spreading the tones' phases really does
    // lower the peak without touching loudness — but the bill lands on the KICK'S
    // ATTACK, because dispersion smears an impulse in time. In house that attack is the
    // product, so it buys headroom with the one thing the genre can least spare.
    // Pinned off here; the parameter and the engine code stay so stored projects load.
    const bool phaseMode = false;

    if (limMode != prevLimMode)
    {
        disperser.flush();                  // no stale phase state across a mode change
        prevLimMode = limMode;
    }

    if (irc4On != prevIrc4)
    {
        for (auto& sl : specLim) sl.flush();
        for (auto& sx : specXover) for (auto& f : sx) f.reset();
        prevIrc4 = irc4On;
    }

    // SWAP: run the CLIPPER before the LIMITER when on (clipper shaves transients,
    // limiter smooths the result). Off = default LIMITER -> CLIPPER (flat tops).
    const bool swapOrder = apvts.getRawParameterValue (ParamID::clipOrder)->load() > 0.5f;

    // CHARACTER shapes the compressor feel around the current defaults:
    //   char = 0  -> multipliers all 1.0 (exactly today's sound).
    //   char < 0  -> softer: less threshold depth, gentler ratio, more make-up link.
    //   char > 0  -> denser: deeper threshold, harder ratio, a touch more drive.
    // Only small ranges so it stays musical and can't blow up the chain.
    // CHARACTER is INERT in FUTURE. Its two real jobs — moving the threshold and the
    // ratio — belong to the classic compressors, which FUTURE holds at zero amount, so
    // they did nothing there. What still leaked through was make-up and clipper drive:
    // the knob quietly changed LOUDNESS while its caption promised character, which is
    // worse than having no knob at all.
    //
    // Nor is it worth re-pointing at the partial engine: AMOUNT already sets how deeply
    // each tone is compressed, so a "harder" control beside it would just be a second
    // way to say the same thing — and moving the threshold would fight the masking model
    // that decides which tones matter in the first place.
    const float charAmt = futureEngine ? 0.0f : character;
    const float charThreshScale = 1.0f + 0.35f * charAmt;  // 0.65 .. 1.35
    const float charRatioScale  = 1.0f + 0.40f * charAmt;  // 0.60 .. 1.40
    const float charMakeupScale = 1.0f - 0.25f * charAmt;  // 1.25 .. 0.75 (softer boosts more)

    // LUFS GAIN — SAME loudness as AUTO GAIN, just steadier.
    // Instead of chasing an abstract target (which was either too quiet or too loud),
    // LUFS GAIN is anchored to EXACTLY the AUTO-GAIN static make-up, then adds only a
    // small slow correction (+/- a few dB) to even out loud/quiet sections. So:
    //   - the moment you turn it on it matches AUTO GAIN (no jump, no overload),
    //   - it then gently pulls sections toward one loudness (that's the whole point),
    //   - the correction is clamped small, so it can never blow up into the clipper.
    const float amt01forMk = juce::jlimit (0.0f, 1.0f, amountTarget * 0.01f);
    // AUTO-GAIN static make-up = the loudness LUFS starts from (NO drop on switch).
    // FUTURE ADDS A REDUCTION-FOLLOWING TERM. The static make-up above is derived from
    // AMOUNT alone and never sees the partial engine's own gain reduction, so FUTURE paid
    // for compression that nothing gave back — and it paid MORE the further AMOUNT went,
    // because the reduction grows linearly with AMOUNT while this term grows about four
    // times slower. That is why FUTURE beat CLASSIC at 20% and lost badly at 50%: a
    // slope error, which no constant offset can fix.
    //
    // Linked at 0.75 rather than 1.0 for the reason makeupLink exists: full compensation
    // would cancel the compression outright, leaving AMOUNT with no effect on loudness
    // and nothing driving the clipper. No feedback risk — the engine measures its
    // reduction from the input analysis, never from the gain applied here.
    const float futureMkDb = futureEngine
                           ? partialEng.lastReductionDb() * cfg::futureMakeupLink
                           : 0.0f;
    const float autoMkDb = amt01forMk * cfg::staticMakeupMaxDb * cfg::makeupLink
                         * charMakeupScale + futureMkDb;
    // Resting point LUFS settles toward: the SAME loudness as AUTO GAIN (no drop).
    const float restDb = autoMkDb;
    constexpr float lufsCorrMax = 3.0f;      // stay within +/-3 dB of the rest point
    if (lufsGainOn)
    {
        // Snap to the AUTO-GAIN level whenever we are far from it. The old test was
        // "did LUFS just get switched on" — but LUFS is now permanently on, so that
        // branch never fired again after the first block and the gain was left to crawl
        // up from zero at 0.008 dB a time, taking tens of seconds to become audible.
        if (! prevLufsGain || std::abs (lufsGainDb - autoMkDb) > 6.0f)
            lufsGainDb = autoMkDb;

        const float measured = meterLufsDb.load();
        if (measured > -50.0f)               // ignore silence/very quiet
        {
            // Crawl slowly toward the rest point. The correction is deliberately NOT
            // allowed to chase the measured loudness back to a fixed target: doing that
            // makes the plugin undo its own work — turn AMOUNT up, the level drops, the
            // correction puts it straight back, and the knob appears to do nothing.
            // The rest point already scales with AMOUNT, so tracking only has to smooth
            // section-to-section differences, not restore whatever was just compressed.
            const float target = juce::jlimit (restDb - lufsCorrMax, restDb + lufsCorrMax,
                                               restDb + juce::jlimit (-1.5f, 1.5f,
                                                                      (-7.0f - measured) * 0.15f));
            lufsGainDb += juce::jlimit (-0.008f, 0.008f, target - lufsGainDb); // gentle glide
        }
    }
    else
    {
        lufsGainDb = autoMkDb;               // track AUTO GAIN level while off (no stale jump)
    }
    prevLufsGain = lufsGainOn;
    const float lufsGainLin = juce::Decibels::decibelsToGain (lufsGainDb);

    // --- Lookahead / latency bookkeeping -----------------------------------
    // With SPECTRAL on, the 3 band limiters carry the lookahead; the final catch
    // limiter runs WITHOUT lookahead (it's just a summed-peak safety) so total
    // latency stays exactly 1x laSamples (not 2x). Report PDC only when it CHANGES.
    for (auto& sl : specLim) sl.setLookahead (lookaheadOn);
    limiter.setLookahead (lookaheadOn && ! specLimitOn);

    // PDC = limiter lookahead + the OVERSAMPLER's own group delay. The oversampling
    // latency was previously omitted, so the plugin sat out of time with the session
    // (audible as comb-filtering on parallel routing / failed null tests).
    const int osIdxForLatency = juce::jlimit (0, kNumOsFactors - 1,
                                  (int) apvts.getRawParameterValue (ParamID::osFactor)->load());
    int osLatency = 0;
    if (oversampling[osIdxForLatency] != nullptr)
        osLatency = juce::roundToInt (oversampling[osIdxForLatency]->getLatencyInSamples());

    // MASK adds its own lookahead (the cancellation pulse reaches both backwards and
    // forwards around a peak). Only in MASK — CLIP and HI-Q stay at zero extra.
    const int maskLatency = maskMode ? maskClip.latencySamples() : 0;

    // The analysis window's delay is reported in EVERY engine, not just FUTURE: the
    // other two are held back by engDelay to match. Latency that changes when you
    // switch engines forces the host to re-align the track mid-playback, which is
    // heard as the sound dropping out — that was the real cause of the dip, and it is
    // not something buffer priming can fix. Constant latency removes it at the source.
    const int futureLatency = partialEng.latencySamples();

    const int reportLatency = (lookaheadOn ? limiter.maxLatencySamples() : 0)
                            + osLatency + maskLatency + futureLatency;
    if (reportLatency != prevReportedLatency)
    {
        setLatencySamples (reportLatency);
        prevReportedLatency = reportLatency;
    }

    // Flush limiter delay-lines when a routing/lookahead toggle changes (no click).
    if (lookaheadOn != prevLookahead || specLimitOn != prevSpecLimit)
    {
        limiter.flush();
        for (auto& sl : specLim) sl.flush();
        prevLookahead = lookaheadOn;
        prevSpecLimit = specLimitOn;
    }

    // Switching the clipper mode must not drag stale lookahead content across — the
    // work buffer holds real audio, so re-entering MASK with old samples in it would
    // click. Same edge-detect pattern as the lookahead/IRC/engine toggles above.
    if (clipMode != prevClipMode)
    {
        maskClip.flush();
        crestRot.flush();
        prevClipMode = clipMode;
    }

    // Fixed compression ratio (Flat/Punch modes removed).
    const float ratio = cfg::ratioPunch;

    // Attack/Release slew speeds (dB/ms) come straight from the knobs.
    const float atkSlew = apvts.getRawParameterValue (ParamID::atkSlew)->load();
    const float relSlew = apvts.getRawParameterValue (ParamID::relSlew)->load();

    // 4-band "All Mix Comp" mode + its per-band amounts.
    const bool allMix = apvts.getRawParameterValue (ParamID::allMix)->load() > 0.5f;
    const float bandAmt01[kBands] = {
        apvts.getRawParameterValue (ParamID::compLow)->load()   * 0.01f,
        apvts.getRawParameterValue (ParamID::compLoMid)->load() * 0.01f,
        apvts.getRawParameterValue (ParamID::compHiMid)->load() * 0.01f,
        apvts.getRawParameterValue (ParamID::compHigh)->load()  * 0.01f
    };

    amountSmoothed.setTargetValue (amountTarget);
    inGainSmoothed.setTargetValue (inGainTarget);

    // Keep a delayed dry copy for the BYPASS crossfade (phase-aligned to latency).
    // dryCopy is a MEMBER pre-sized in prepareToPlay, so this copy does NOT allocate
    // on the audio thread (realtime-safe). Same data, just no per-block heap churn.
    dryCopy.makeCopyOf (buffer, true);   // avoidReallocating = true

    // MASK: flatten the waveform's crest with an allpass BEFORE anything else looks at
    // it, so every stage downstream gets the headroom. Safe to run here rather than
    // inside the per-sample loop after the INPUT gain: an allpass is linear and a gain
    // is a scalar, so the order between them cannot change the result — and crest
    // factor is peak/RMS, which is gain-invariant, so the search sees the same thing
    // either way. Placed after the dry copy so BYPASS still returns the true input.
    if (maskMode && cfg::maskPhaseRotate)
        crestRot.processBlock (buffer);

    // PHASE: spread the tones' phases so the PEAK drops while the energy — and so the
    // loudness — stays put. Runs here, ahead of the compressor and the limiter, so
    // every stage downstream inherits the headroom instead of having to earn it by
    // turning something down.
    if (phaseMode)
        disperser.processBlock (buffer);

    // FUTURE: STAGE 1 happens HERE instead of in the per-sample loop below. The signal
    // is taken apart into individual tones, each tone's loudness is controlled on its
    // own envelope, and it is put back together. There is no shared gain anywhere in
    // that path, which is why it cannot pump: a loud kick has no mechanism by which to
    // drag the hats down with it. The classic compressors further down still run, but
    // at zero amount, so nothing is compressed twice.
    if (futureEngine)
    {
        // Use the SMOOTHED amount, not the raw parameter: the engine applies this once
        // per block, so feeding it the knob directly steps the gain on every automation
        // move. The per-sample path below already reads the same smoother.
        partialEng.setAmount (juce::jlimit (0.0f, 1.0f,
                                amountSmoothed.getCurrentValue() * 0.01f));
        partialEng.setUpward (juce::jlimit (0.0f, 1.0f,
                                apvts.getRawParameterValue (ParamID::upComp)->load() * 0.01f));
        // BOND is retired from the panel, so it is pinned OFF here rather than read from
        // the parameter — the parameter survives only so stored projects still load.
        partialEng.setBond (false);

        // ALL MIX: the four band knobs scale AMOUNT per tone. Uses the SAME smoothed
        // fade as the classic path so switching the mode is click-free either way.
        {
            // NOTE: cfg::bandStrengthScale is deliberately NOT applied here. It exists to
            // stop the classic band compressors — where a whole band shares one gain —
            // from swallowing the top end, and it trims HI-MID/HIGH to 0.8/0.7. FUTURE
            // already eases off on the top by itself, via futureNoiseScale: hats are
            // noise and get 0.6x. Applying both stacked to 0.42, so the HIGH knob at 100%
            // delivered under half of what it promised — barely audible.
            const float inv = 1.0f / juce::jmax (0.01f, amountTarget * 0.01f);
            const float bandRel[kBands] = {
                bandAmt01[0] * inv, bandAmt01[1] * inv,
                bandAmt01[2] * inv, bandAmt01[3] * inv
            };
            // allMixFade is advanced inside the per-sample loop further down, which runs
            // AFTER this. Project it forward over the block so FUTURE and the classic
            // path move in step instead of the band scaling lagging by one buffer.
            const float fadeTarget = allMix ? 1.0f : 0.0f;
            float fadeNow = allMixFade;
            for (int i = 0; i < numSamples; ++i)
                fadeNow += (fadeTarget - fadeNow) * 0.0015f;

            partialEng.setBandAmounts (bandRel, fadeNow);
        }

        // ATTACK / RELEASE finally do something in FUTURE — they used to be dead here,
        // because the classic compressors they fed are held at zero amount in this mode.
        // Their meaning changes: ATTACK is now in cycles of each tone's own wave, and
        // RELEASE is a fraction of a beat at the PROJECT tempo (read from the host, not
        // detected from the audio — so there is nothing for it to get wrong).
        double bpm = cfg::futureFallbackBpm;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto t = pos->getBpm())
                    bpm = *t;

        // Cycles come straight from their own knob now — no remapping from the dB/ms
        // scale, so the number the user sees IS the number the engine uses.
        const float atkCyc = apvts.getRawParameterValue (ParamID::atkCycles)->load();

        // AUTO: pick the note value closest to how long this material's tones actually
        // take to fade. The decay is MEASURED from the audio, so this is a fact being
        // rounded to the musical grid — not a guess. (ATTACK deliberately has no AUTO:
        // it is already automatic per tone, and what is left is taste.)
        float relBeats;
        if (apvts.getRawParameterValue (ParamID::relAuto)->load() > 0.5f)
        {
            const float wantBeats = partialEng.measuredDecaySec() * (float) (bpm / 60.0);
            int best = 0;
            float bestErr = 1.0e9f;
            for (int i = 0; i < relnote::count; ++i)
            {
                const float err = std::abs (std::log2 (relnote::beatsAt (i) / juce::jmax (0.01f, wantBeats)));
                if (err < bestErr) { bestErr = err; best = i; }
            }
            relBeats = relnote::beatsAt (best);
            autoRelNote.store (best);            // so the UI can show what it chose
        }
        else
        {
            const int noteIdx = (int) apvts.getRawParameterValue (ParamID::relNote)->load();
            relBeats = relnote::beatsAt (noteIdx);
            autoRelNote.store (-1);
        }

        partialEng.setTiming (atkCyc, relBeats, bpm,
                              apvts.getRawParameterValue (ParamID::relSmart)->load() > 0.5f);

        partialEng.processBlock (buffer);
    }
    else if (engDelayLen > 0)
    {
        // Not in FUTURE: hold the signal back by the same amount FUTURE would, so the
        // plugin's latency is identical in every engine. This is what makes switching
        // seamless — the host has nothing to re-align, and both paths sit in the same
        // timeframe. A plain ring delay, no filtering, so it cannot colour anything.
        auto* wl = buffer.getWritePointer (0);
        auto* wr = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
        int p = engDelayPos;
        for (int i = 0; i < numSamples; ++i)
        {
            const float dl = engDelayL[(size_t) p];
            const float dr = engDelayR[(size_t) p];
            engDelayL[(size_t) p] = wl[i];
            engDelayR[(size_t) p] = (wr != nullptr) ? wr[i] : wl[i];
            wl[i] = dl;
            if (wr != nullptr) wr[i] = dr;
            if (++p >= engDelayLen) p = 0;
        }
        engDelayPos = p;
    }

    const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);
    // In FUTURE the compressor meter has to read the partial engine, because the
    // classic compressor is deliberately doing nothing.
    float maxGrThisBlock  = futureEngine ? partialEng.lastReductionDb() : 0.0f;
    float maxLimGrThisBlk = 0.0f; // most the LIMITER pulled down this block (dB, >=0)

    // One-sample spectral/IRC limiter pass (shared by the normal path and the
    // SWAP path). Splits into N bands, limits each, sums, then a final catch limiter.
    auto runLimiterSample = [&] (float& l, float& r)
    {
        const int   nb = irc4On ? 6 : 3;
        static constexpr int split3[2] = { 1, 3 };            // 200 Hz, 3 kHz
        static constexpr int split6[5] = { 0, 1, 2, 3, 4 };   // all five
        // Psychoacoustic ceiling weight per band (>1 = softer/higher ceiling).
        static constexpr float w6[6] = { 1.02f, 1.00f, 0.96f, 1.06f, 1.04f, 0.98f };
        static constexpr float w3[3] = { 1.00f, 1.00f, 1.00f };
        float bandsL[6], bandsR[6];
        float curL = l, curR = r;
        for (int b = 0; b < nb; ++b)
        {
            if (b < nb - 1)
            {
                const int sidx = (nb == 6) ? split6[b] : split3[b];
                float lpL, hpL, lpR, hpR;
                specXover[sidx][0].processSample (0, curL, lpL, hpL);
                specXover[sidx][1].processSample (0, curR, lpR, hpR);
                bandsL[b] = lpL; bandsR[b] = lpR;
                curL = hpL; curR = hpR;

                // MODERN: phase-compensate this band through the allpass of every
                // LATER split, so all bands share phase and sum FLAT. Without this
                // the crossovers (90/200 Hz = kick fundamental) get ±3 dB notches
                // even when no limiting happens at all.
                if (modernEngine)
                    for (int later = b + 1; later < nb - 1; ++later)
                    {
                        const int lidx = (nb == 6) ? split6[later] : split3[later];
                        bandsL[b] = specAp[b][lidx][0].processSample (0, bandsL[b]);
                        bandsR[b] = specAp[b][lidx][1].processSample (0, bandsR[b]);
                    }
            }
            else { bandsL[b] = curL; bandsR[b] = curR; }
            const float w = (nb == 6) ? w6[b] : w3[b];
            const float bandCeil = ceilingLin * cfg::specBandCeilingScale * w;
            specLim[b].process (bandsL[b], bandsR[b], bandCeil, true);
        }
        float sumL = 0.0f, sumR = 0.0f;
        for (int b = 0; b < nb; ++b) { sumL += bandsL[b]; sumR += bandsR[b]; }
        l = sumL; r = sumR;
        limiter.process (l, r, ceilingLin, true);   // final summed-peak catch
    };

    // ====================================================================
    //  3-STAGE CHAIN per sample:
    //   INPUT gain -> [1] compressor (even sections) -> [2] limiter (peaks)
    //   (clipper is the final block-stage below, driven by AMOUNT)
    // ====================================================================
    for (int n = 0; n < numSamples; ++n)
    {
        const float amount01 = amountSmoothed.getNextValue() * 0.01f; // 0..1 knob
        const float inGainDb = inGainSmoothed.getNextValue();
        const float inGain   = juce::Decibels::decibelsToGain (inGainDb);

        // INPUT gain first. Sanitise NaN/Inf coming from upstream plugins BEFORE it
        // enters any recursive state (RMS, envelopes, biquads, DC blocker) — a single
        // non-finite sample would otherwise poison those filters permanently.
        float inL = buffer.getSample (0, n);
        float inR = (numCh > 1 ? buffer.getSample (1, n) : inL);
        if (! std::isfinite (inL)) inL = 0.0f;
        if (! std::isfinite (inR)) inR = 0.0f;
        float l = inL * inGain;
        float r = inR * inGain;

        // LOW TRIM: when INPUT boosts above 0 dB, lift the sub/low band (<120 Hz)
        // 1.5 dB LESS than the rest, so pushing level doesn't over-drive the bass.
        // The trim fades in with the boost (0 at INPUT=0), so the low end is
        // untouched at unity. Split the low band, attenuate it, and re-sum.
        if (inGainDb > 0.01f)
        {
            const float trimDb  = -juce::jmin (1.5f, inGainDb);   // 0..-1.5 dB
            const float trimGain = juce::Decibels::decibelsToGain (trimDb) - 1.0f; // delta
            float loL, hiL, loR, hiR;
            inLowXover[0].processSample (0, l, loL, hiL);
            inLowXover[1].processSample (0, r, loR, hiR);
            // Add the (negative) delta of the low band only => low band ends up
            // trimGain quieter, the rest untouched. (hi+lo already sums to l.)
            l += loL * trimGain;
            r += loR * trimGain;
        }
        else
        {
            // Keep the filters warm (running) even at unity so toggling is click-free.
            float loL, hiL, loR, hiR;
            inLowXover[0].processSample (0, l, loL, hiL);
            inLowXover[1].processSample (0, r, loR, hiR);
            juce::ignoreUnused (loL, hiL, loR, hiR);
        }

        // --- STAGE 1: compression -----------------------------------------
        // BOTH the single-band and 4-band paths run EVERY sample (so the crossover
        // filters never freeze), and we crossfade between them with a smoothed
        // 'allMixFade' -> switching ALL MIX COMP is click-free.
        const float fadeTarget = allMix ? 1.0f : 0.0f;
        allMixFade += (fadeTarget - allMixFade) * 0.0015f;  // ~ tens of ms glide
        const float fade = allMixFade;

        // In FUTURE the compression has ALREADY happened, per tone, up in the partial
        // engine. The classic compressors still run (so their filters and envelopes
        // stay warm and switching engines is click-free) but at zero amount, i.e. they
        // pass through. Without this the signal would be compressed twice.
        const float compAmt = futureEngine ? 0.0f : amount01;

        // Single-band path.
        float sL = l, sR = r, sAmount = amount01;
        {
            const float det = juce::jmax (std::abs (l), std::abs (r));
            const float maxRed = juce::jmap (compAmt, 0.0f, 1.0f, 0.0f, cfg::downRangeDb);
            float grDb = 0.0f;
            const float g = comp.process (det, compAmt, ratio * charRatioScale, atkSlew,
                                          relSlew, maxRed, grDb, charThreshScale);
            sL = l * g; sR = r * g;
            if (fade < 0.5f) maxGrThisBlock = juce::jmax (maxGrThisBlock, std::abs (grDb));
        }

        // 4-band path (always processed so filters stay warm).
        float mL = 0.0f, mR = 0.0f, mAvg = 0.0f;
        {
            float curL = l, curR = r;
            for (int b = 0; b < kBands; ++b)
            {
                float bl, br;
                if (b < kBands - 1)
                {
                    float lpL, hpL, lpR, hpR;
                    xover[b][0].processSample (0, curL, lpL, hpL);
                    xover[b][1].processSample (0, curR, lpR, hpR);
                    bl = lpL; br = lpR;
                    curL = hpL; curR = hpR;
                }
                else { bl = curL; br = curR; }

                // Phase compensation: pass the LOW band through allpasses of the
                // later splits, LO-MID through the last one -> all bands share
                // phase and sum flat at the crossover frequencies.
                if (b == 0)
                {
                    bl = apLowB[0][0].processSample (0, bl);
                    br = apLowB[0][1].processSample (0, br);
                    bl = apLowB[1][0].processSample (0, bl);
                    br = apLowB[1][1].processSample (0, br);
                }
                else if (b == 1)
                {
                    bl = apMidB[0][0].processSample (0, bl);
                    br = apMidB[0][1].processSample (0, br);
                }

                // Band strength = its knob amount * per-band scale from Config
                // (lets you make e.g. highs / high-mids softer by hand).
                //
                // Zeroed in FUTURE, exactly like the wideband compressor above. This was
                // missed: FUTURE muted `comp` but left the four band compressors running,
                // so switching ALL MIX on stacked them ON TOP of the per-tone engine and
                // the sound fell apart. Only one stage-1 compressor may ever be live.
                const float a = futureEngine ? 0.0f
                              : juce::jlimit (0.0f, 1.0f, bandAmt01[b] * cfg::bandStrengthScale[b]);
                const float det = juce::jmax (std::abs (bl), std::abs (br));
                const float maxRed = juce::jmap (a, 0.0f, 1.0f, 0.0f, cfg::downRangeDb);
                float grDb = 0.0f;
                const float g = bandComp[b].process (det, a, ratio * charRatioScale, atkSlew,
                                                     relSlew, maxRed, grDb, charThreshScale);
                mL += bl * g; mR += br * g;
                if (fade >= 0.5f) maxGrThisBlock = juce::jmax (maxGrThisBlock, std::abs (grDb));
                // Accumulate the KNOB value, not the zeroed one: mAvg drives the make-up
                // gain, so feeding it the muted amount would drop the level to nothing the
                // moment ALL MIX was switched on in FUTURE.
                mAvg += juce::jlimit (0.0f, 1.0f, bandAmt01[b] * cfg::bandStrengthScale[b]);
            }
            mAvg /= (float) kBands;
        }

        // Crossfade the two paths.
        l = (1.0f - fade) * sL + fade * mL;
        r = (1.0f - fade) * sR + fade * mR;
        const float usedAmount = (1.0f - fade) * sAmount + fade * mAvg;

        // MAKE-UP GAIN — two mutually-exclusive modes (radio, enforced by the UI):
        //   AUTO GAIN  = STATIC make-up (constant, amount-scaled, gentle makeupLink).
        //   LUFS GAIN  = slow loudness-tracking gain toward a target (cleaner/steadier).
        // If both are off, no compensation is applied.
        if (lufsGainOn)
        {
            l *= lufsGainLin; r *= lufsGainLin;
        }
        else if (autoGainOn)
        {
            const float mk = juce::Decibels::decibelsToGain (
                usedAmount * cfg::staticMakeupMaxDb * cfg::makeupLink * charMakeupScale
                + futureMkDb);
            l *= mk; r *= mk;
        }

        // --- STAGE 2: limiter (spectral/IRC-style) --------------------------
        // With SWAP off it runs here (limiter -> clipper). With SWAP on it is
        // skipped here and run as a block pass AFTER the clipper (clipper -> limiter).
        if (! swapOrder)
        {
            const float preLimPeak = juce::jmax (std::abs (l), std::abs (r));
            runLimiterSample (l, r);
            const float postLimPeak = juce::jmax (std::abs (l), std::abs (r));
            if (preLimPeak > 1.0e-6f && postLimPeak < preLimPeak)
                maxLimGrThisBlk = juce::jmax (maxLimGrThisBlk,
                    -juce::Decibels::gainToDecibels (postLimPeak / preLimPeak));
        }

        buffer.setSample (0, n, l);
        if (numCh > 1) buffer.setSample (1, n, r);
    }

    currentGainReductionDb.store (-maxGrThisBlock);
    compGrDb.store    (-maxGrThisBlock);
    limiterGrDb.store (-maxLimGrThisBlk);

    // Total reduction (comp + limiter) and compressor-only, so the graph can
    // shade the two contributions. Pushed together at the same write position.
    const float totalGrThisBlock = maxGrThisBlock + maxLimGrThisBlk;

    // Push a few INPUT waveform points per block into the scope ring buffer so
    // the UI can draw the real signal shape (not just the GR envelope).
    {
        const int pts = juce::jmin (8, numSamples);
        const int step = juce::jmax (1, numSamples / pts);
        int pos = scopeWritePos.load();
        for (int i = 0; i < numSamples; i += step)
        {
            // peak of both channels at this point
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                s = juce::jmax (s, std::abs (buffer.getSample (ch, i)));
            scopeHistory[(size_t) pos].store (s);
            pos = (pos + 1) % kScopeSize;
        }
        scopeWritePos.store (pos);
    }

    // ---- CLIP DRIVE + oversampled hard clipper ----------------------------
    // AMOUNT also drives the signal INTO the clipper: more AMOUNT -> louder push
    // before the clip -> the clipper flattens more of the top = the "even, clipped
    // to 0" sound you described (level up, slice everything over the ceiling).
    float clipActivityThisBlock = 0.0f;
    if (clipOn)
    {
        const float ceiling = juce::Decibels::decibelsToGain (ceilingDb);

        // Drive amount from the (smoothed) AMOUNT knob — no zipper on automation.
        // CHARACTER adds a touch more push to the right (denser), less to the left.
        const float amt01 = juce::jlimit (0.0f, 1.0f, amountSmoothed.getCurrentValue() * 0.01f);
        const float charDrive = juce::jlimit (0.5f, 1.5f, 1.0f + 0.25f * charAmt);
        const float drive = juce::Decibels::decibelsToGain (amt01 * cfg::clipDriveMaxDb * charDrive);

        // CLIP ACTIVITY: how much of the signal sits above the ceiling BEFORE the
        // clip slices it (peak overshoot in dB, mapped 0..1 over ~6 dB). Measured
        // pre-clip on the driven buffer so the meter reflects real slicing.
        {
            float maxOver = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const auto* rp = buffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    maxOver = juce::jmax (maxOver, std::abs (rp[i]) * drive);
            }
            if (maxOver > ceiling)
                clipActivityThisBlock = juce::jlimit (0.0f, 1.0f,
                    juce::Decibels::gainToDecibels (maxOver / ceiling) / 6.0f);
        }

        if (drive != 1.0f)
            buffer.applyGain (drive);

        // ---- MASK: cancel the peaks BEFORE the clipper ever sees them ------
        // Position matters. This sits AFTER the drive and BEFORE the oversampler, so
        // the threshold it works against is exactly the one the clipper would slice
        // at — anywhere earlier in the chain it would be cancelling against the wrong
        // level. Whatever it leaves behind still falls through to the ADAA clipper
        // below, so the ceiling is guaranteed by the same code as always.
        if (maskMode)
        {
            // Rendering is not realtime, so spend more iterations there — it converges
            // closer to the peak-cancellation optimum for free.
            maskClip.processBlock (buffer, ceiling, cfg::maskAmount,
                                   isNonRealtime() ? cfg::maskItersOffline : cfg::maskIters);
            maskTilt.store (maskClip.lastTilt());
        }
        else
        {
            maskTilt.store (0.0f);
        }

        // CLIP CHARACTER: hardness morph + ADAA order.
        //   hardness (1-clipShape): 1 = hard brickwall, <1 = warmer soft-clip.
        //   hiQ (HI-Q selected): 2nd-order ADAA (cleaner) vs 1st-order for basic CLIP.
        //   Same oversampler for both -> identical loudness, no peaks slipping past.
        // CLIP SHAPE is INERT in ACR, and by design. ACR cancels the peaks with pulses
        // BEFORE the clipper, so only a dB or two ever reaches the clip curve — reshaping
        // that curve has almost nothing left to reshape. On top of that the soft curve
        // saturates at +/-2/3 rather than +/-1, so turning it up mostly just loses ~3.5 dB
        // that the LUFS make-up then puts back: you hear "the same", not "softer".
        // It stays fully live in HI-Q, where the clipper really does all the slicing.
        const float shapeAmt = maskMode ? 0.0f
                             : juce::jlimit (0.0f, 1.0f,
                                   apvts.getRawParameterValue (ParamID::clipShape)->load());
        const float hardness = 1.0f - shapeAmt;
        const bool  hiQ      = clipHiQparam;

        // Both CLIP and HI-Q use the SAME min-phase oversampler (no linear-phase FIR)
        // so there's NO loudness difference and no pre-ring peaks slipping past the
        // ceiling. HI-Q only changes the ADAA order (2nd vs 1st) = cleaner, same level.
        const int osIdx = juce::jlimit (0, kNumOsFactors - 1,
                          (int) apvts.getRawParameterValue (ParamID::osFactor)->load());
        auto& os = *oversampling[osIdx];

        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = os.processSamplesUp (block);

        // Asymmetry = DIFFERENT drive per half-wave, divided back out so the signal
        // is UNCHANGED below the ceiling and only the CLIPPED tops differ (punch).
        const float asym = cfg::clipAsym;
        // All ADAA math in DOUBLE (see clipmath note): float32 cancellation at high
        // oversampling was the root cause of the HI-Q hiss.
        const double hard  = (double) hardness;
        const double asymD = (double) asym;
        // Clip a hair BELOW the ceiling in the oversampled domain. The min-phase
        // half-band IIR on the way back down adds a small overshoot; leaving this
        // guard means the base-rate stage never has to hard-clamp (a clamp there is
        // an un-oversampled clip = aliasing, exactly what the 16x is for).
        const float  ceilingOs = ceiling * cfg::clipDownGuard;
        const double ceilD     = (double) ceilingOs;

        // ADAA1 helper (used directly for basic CLIP).
        auto adaa1 = [hard] (double a, double b) -> double
        {
            const double diff = a - b;
            if (std::abs (diff) < 1.0e-6) return clipmath::f (0.5 * (a + b), hard);
            return (clipmath::F1 (a, hard) - clipmath::F1 (b, hard)) / diff;
        };

        for (size_t ch = 0; ch < osBlock.getNumChannels() && ch < 2; ++ch)
        {
            auto* d = osBlock.getChannelPointer (ch);
            double x1 = adaaPrevX[ch];    // previous input sample (normalised)
            double x2 = adaaPrevX2[ch];   // the one before that (for ADAA2)
            for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
            {
                // ADAA needs a SMOOTH input sequence, so asymmetry is applied as a
                // continuous offset (not a sign-flipping drive). HI-Q -> ADAA2.
                const double x0 = ((double) d[i] / ceilD) + asymD;
                double y;
                if (hiQ)
                {
                    // 2nd-order ADAA (Chowdhury), with the REFERENCE ill-condition
                    // fallback so the output stays continuous when x0 ~ x2.
                    const double eps = 1.0e-6;
                    auto t = [&] (double a, double b) -> double
                    {
                        const double diff = a - b;
                        if (std::abs (diff) < eps) return clipmath::F1 (0.5 * (a + b), hard);
                        return (clipmath::F2 (a, hard) - clipmath::F2 (b, hard)) / diff;
                    };
                    const double den = x0 - x2;
                    if (std::abs (den) > eps)
                    {
                        y = 2.0 * (t (x0, x1) - t (x1, x2)) / den;
                    }
                    else
                    {
                        // Reference fallback: expand around xBar = (x0+x2)/2 vs x1.
                        const double xBar  = 0.5 * (x0 + x2);
                        const double delta = xBar - x1;
                        if (std::abs (delta) > eps)
                            y = (2.0 / delta) * (clipmath::F1 (xBar, hard)
                                  + (clipmath::F2 (x1, hard) - clipmath::F2 (xBar, hard)) / delta);
                        else
                            y = clipmath::f (0.5 * (xBar + x1), hard);
                    }
                }
                else
                {
                    y = adaa1 (x0, x1);              // ADAA1 (default, low latency)
                }
                x2 = x1; x1 = x0;
                d[i] = juce::jlimit (-ceilingOs, ceilingOs, (float) ((y - asymD) * ceilD));
            }
            adaaPrevX[ch]  = x1;
            adaaPrevX2[ch] = x2;
        }

        os.processSamplesDown (block);

        // DC-blocker (removes the tiny DC the asymmetric clip adds) + a SMOOTH catch.
        // The old code hard-clamped here, twice. That clamp ran at BASE rate, so it
        // was an un-oversampled clip — it put back exactly the aliasing the 16x
        // oversampling exists to remove. Now the OS clipper leaves cfg::clipDownGuard
        // of headroom, so the only thing that can still poke above the ceiling is
        // downsampler ripple (tiny), and a short gain dip removes it without
        // generating any harmonics at all.
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float x1 = dcX1[ch], y1 = dcY1[ch], g = dipG[ch];
            for (int i = 0; i < numSamples; ++i)
            {
                const float s = d[i];
                const float y = s - x1 + 0.9995f * y1;   // 1st-order high-pass ~ <5 Hz
                x1 = s; y1 = y;

                // Catch instantly (so the peak lands EXACTLY on the ceiling), let go
                // over ~cfg::clipDipSamples. Gain is applied to the whole waveform,
                // so there's no corner in the signal — unlike a clamp.
                const float a   = std::abs (y);
                const float tgt = (a > ceiling) ? (ceiling / a) : 1.0f;
                if (tgt < g) g = tgt;
                else         g += (1.0f - g) * juce::jmax (0.01f, dipRelCoeff);
                // Floor it. This only ever catches downsampler ripple, a fraction of a
                // dB — so anything approaching a real attenuation here means something
                // upstream misbehaved, and without a floor the plugin would simply go
                // quiet and stay quiet.
                g = juce::jlimit (0.25f, 1.0f, g);
                d[i] = y * g;
            }
            dcX1[ch] = x1; dcY1[ch] = y1; dipG[ch] = g;
        }
    }

    // SWAP: with the order swapped, the LIMITER runs HERE — after the clipper — so
    // the clipper shaves the transients first and the limiter smooths the result.
    if (swapOrder)
    {
        auto* wl = buffer.getWritePointer (0);
        auto* wr = numCh > 1 ? buffer.getWritePointer (1) : wl;
        for (int n = 0; n < numSamples; ++n)
        {
            float l = wl[n], r = wr[n];
            const float preLimPeak = juce::jmax (std::abs (l), std::abs (r));
            runLimiterSample (l, r);
            const float postLimPeak = juce::jmax (std::abs (l), std::abs (r));
            if (preLimPeak > 1.0e-6f && postLimPeak < preLimPeak)
                maxLimGrThisBlk = juce::jmax (maxLimGrThisBlk,
                    -juce::Decibels::gainToDecibels (postLimPeak / preLimPeak));
            wl[n] = l; if (numCh > 1) wr[n] = r;
        }
        limiterGrDb.store (-maxLimGrThisBlk);
    }

    clipActivity.store (clipOn ? clipActivityThisBlock : 0.0f);

    // Push one point per block into the scrolling history (total GR, comp-only GR,
    // clip activity), all at the same write position so the graph stays aligned.
    {
        const int pos = historyWritePos.load();
        grHistory[(size_t) pos].store     (-totalGrThisBlock);
        grHistoryComp[(size_t) pos].store (-maxGrThisBlock);
        clipHistory[(size_t) pos].store   (clipOn ? clipActivityThisBlock : 0.0f);
        historyWritePos.store ((pos + 1) % kHistorySize);
    }

    // ---- BYPASS crossfade — dry is delay-aligned to the processed latency. -----
    {
        // The dry copy has to be held back by everything the wet path went through,
        // otherwise the BYPASS crossfade comb-filters. That is now dominated by the
        // analysis window, which every engine carries.
        const int lat = (lookaheadOn ? limiter.maxLatencySamples() : 0)
                      + partialEng.latencySamples();
        const int delayUse = juce::jlimit (0, dryDelayLen, lat);
        auto* wl = buffer.getWritePointer (0);
        auto* wr = numCh > 1 ? buffer.getWritePointer (1) : wl;
        const auto* dcl = dryCopy.getReadPointer (0);
        const auto* dcr = dryCopy.getNumChannels() > 1 ? dryCopy.getReadPointer (1) : dcl;

        for (int i = 0; i < numSamples; ++i)
        {
            // push current dry into the ring, read the delayed dry out.
            float dL, dR;
            if (delayUse == 0)
            {
                dL = dcl[i]; dR = dcr[i];
            }
            else
            {
                dL = dryDelayL[(size_t) dryDelayPos];
                dR = dryDelayR[(size_t) dryDelayPos];
                dryDelayL[(size_t) dryDelayPos] = dcl[i];
                dryDelayR[(size_t) dryDelayPos] = dcr[i];
                dryDelayPos = (dryDelayPos + 1) % delayUse;
            }

            // Smooth crossfade to dry when BYPASS is on (no click). 0=proc, 1=dry.
            const float target = bypassOn ? 1.0f : 0.0f;
            bypassFade += (target - bypassFade) * 0.002f;
            if (bypassFade > 0.0001f)
            {
                wl[i] = (1.0f - bypassFade) * wl[i] + bypassFade * dL;
                if (numCh > 1) wr[i] = (1.0f - bypassFade) * wr[i] + bypassFade * dR;
            }
        }
    }

    // ---- OUTPUT trim: ATTENUATION ONLY (<= 0 dB) --------------------------
    // Applied last, after the clipper. Because it can only turn DOWN, it can never
    // push anything back over the ceiling — no new peaks are possible here.
    {
        const float outDb = juce::jmin (0.0f, apvts.getRawParameterValue (ParamID::outGain)->load());
        outGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outDb));
        if (outDb < -0.001f || outGainSmoothed.isSmoothing())
        {
            auto* wl = buffer.getWritePointer (0);
            auto* wr = numCh > 1 ? buffer.getWritePointer (1) : wl;
            for (int i = 0; i < numSamples; ++i)
            {
                const float g = outGainSmoothed.getNextValue();
                wl[i] *= g;
                if (numCh > 1) wr[i] *= g;
            }
        }
    }

    // ---- OUTPUT METERS: true RMS + ITU-R BS.1770 LUFS ----------------------
    {
        // RMS: TRUE per-channel energy average (L^2+R^2)/2 (not ((L+R)/2)^2, which
        // under-reads on wide/anti-phase stereo). ~300 ms exponential window.
        const float rmsCoeff = std::exp (-1.0f / (0.300f * (float) currentSampleRate));
        // BS.1770-4: accumulate K-weighted energy into 100 ms blocks. Momentary /
        // short-term / integrated are all derived from those blocks (see
        // pushLufsBlock) using RECTANGULAR windows, as the spec requires.

        for (int n = 0; n < numSamples; ++n)
        {
            const float oL = buffer.getSample (0, n);
            const float oR = numCh > 1 ? buffer.getSample (1, n) : oL;
            const float energy = 0.5f * (oL * oL + oR * oR);
            meterRmsSq = rmsCoeff * meterRmsSq + (1.0f - rmsCoeff) * energy;

            // LUFS: K-weight each channel (shelf -> HPF). BS.1770 SUMS channel
            // mean-squares for stereo (no averaging). Sliding 400 ms window.
            const double wL = kHpfL.process (kShelfL.process ((double) oL));
            const double wR = kHpfR.process (kShelfR.process ((double) oR));
            const double kEnergy = wL * wL + wR * wR;
            // Accumulate into the current 100 ms block; emit it when full.
            lufsBlockAcc += kEnergy;
            if (++lufsBlockPos >= lufsBlockLen)
            {
                pushLufsBlock (lufsBlockAcc / (double) lufsBlockLen);
                lufsBlockAcc = 0.0;
                lufsBlockPos = 0;
            }
        }
        meterRmsDb.store  (juce::Decibels::gainToDecibels (std::sqrt (meterRmsSq) + 1.0e-9f));
    }

    // ---- A/B LOUDNESS-MATCH TRIM (very last, post-meter) --------------------
    // The editor sets abMatchTrimDb so that the A and B snapshots play at matched
    // perceived loudness (compare CHARACTER, not level). 0 dB = no change. Applied
    // AFTER the meters so the per-slot loudness we measure stays the natural one.
    {
        const float trimDb = abMatchTrimDb.load();
        if (std::abs (trimDb) > 0.01f)
            buffer.applyGain (juce::Decibels::decibelsToGain (trimDb));
    }

    // ---- FINAL BRICKWALL SAFETY CLAMP (ALWAYS, unless bypassed) -------------
    // Absolutely nothing may leave above the ceiling. The limiter is NOT a true
    // brickwall (fast release can let the odd inter-sample peak through), and the
    // clipper's drive/summation/AB-trim could nudge a sample over. This hard clamp
    // at the very output guarantees no sample ever passes the ceiling (0 dBFS-safe).
    // Skipped only when fully bypassed, so BYPASS stays a true dry passthrough.
    if (bypassFade < 0.99f)
    {
        const float ceilOut = juce::Decibels::decibelsToGain (ceilingDb);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = juce::jlimit (-ceilOut, ceilOut, d[i]);
        }
    }
}

//==============================================================================
void HouseCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void HouseCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    // MIGRATION: presets written before the CLIP|HI-Q|MASK ladder only carry the old
    // 'cliphiq' bool. Seed clipmode from it (false -> CLIP, true -> HI-Q) so an old
    // preset restores the clipper it was actually saved with. It can never land on
    // MASK — that mode did not exist when the preset was written.
    auto param = [&tree] (const char* id) { return tree.getChildWithProperty ("id", juce::String (id)); };

    auto seed = [&tree, &param] (const char* newId, const char* legacyId, float fallback)
    {
        if (param (newId).isValid()) return;

        const auto legacy = param (legacyId);
        const float v = legacy.isValid() ? ((float) legacy.getProperty ("value") > 0.5f ? 1.0f : 0.0f)
                                         : fallback;
        juce::ValueTree t ("PARAM");
        t.setProperty ("id",    juce::String (newId), nullptr);
        t.setProperty ("value", v, nullptr);
        tree.addChild (t, -1, nullptr);
    };

    // Neither can land on the new third position — those modes did not exist when the
    // preset was written, so an old preset always restores what it was actually saved with.
    seed (ParamID::clipMode,   ParamID::clipHiQ, 1.0f);   // absent -> HI-Q (the old default)
    seed (ParamID::engineMode, ParamID::engine,  0.0f);   // absent -> CLASSIC

    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* HouseCompAudioProcessor::createEditor()
{
    return new HouseCompAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HouseCompAudioProcessor();
}
