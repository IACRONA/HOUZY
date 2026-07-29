<div align="center">

# HOUZY

**A next-generation mastering compressor**

Three original technologies: **HOUZY** — compression with no shared gain,
**ACR** — a clipper that stops chopping the highs,
**CYCLES / BEATS** — attack in wave cycles and release in beat fractions.

[![Windows](https://img.shields.io/badge/Windows-VST3-4dd8ef?style=flat-square)]()
[![macOS](https://img.shields.io/badge/macOS-VST3%20%2B%20AU-4dd8ef?style=flat-square)]()
[![Free](https://img.shields.io/badge/price-free-3ddc84?style=flat-square)]()

<br>

<img src="Houzy%20(1).png" width="820">

</div>
---

## The problem

An ordinary compressor is `output = gain × signal`. **One number for the whole sound.**

Every classic complaint follows from that:

- **the detector is blind** — the entire spectrum is collapsed into one number, so a
  kick and a vocal of the same amplitude are indistinguishable to it;
- **the kick drags everything with it** — it is the loudest thing, the gain drops,
  and the hats, the vocal and the reverb tails dive with it. That is pumping;
- **attack fights punch** — one envelope has to serve both the transient and the body;
- **the bass gets dirty** — multiplication is modulation: the gain moves, and sidebands
  grow around a 50 Hz kick. The compressor ruins the very thing it was evening out.

Multiband does not fix this. It gives you four numbers instead of one, but each is
still a single number for an entire band.

---

## HOUZY — compression with no shared gain

The sound is taken apart into **individual tones**, and each one's loudness is evened
out by its own envelope. **There is no shared gain, so there is nothing to pump.**

Pumping is not treated here. It cannot happen.

**What that buys you:**

| | |
|---|---|
| **No frequency drags the others down** | The kick and the hat are different tones with different envelopes. The kick can be squeezed as hard as you like and the top end never notices. |
| **The detector can finally see** | Every tone arrives with its own frequency, so loudness is judged with a hearing weighting — the same curve used by LUFS meters. |
| **Punch survives per tone** | A tone that has only just appeared is not compressed. The body of the kick can be crushed while its leading edge stays untouched. |
| **The bass cannot be modulated** | Attack is measured in **wave cycles**, not milliseconds, and can never physically become faster than half a cycle. |

---

## CYCLES and BEATS — attack and release, reinvented

The millisecond is a poor unit for both. That follows from arithmetic, not taste.

### Attack in wave cycles, not milliseconds

**5 ms on a 50 Hz bass note is a quarter of its wave.** The gain moves inside a single
oscillation: that is no longer dynamics, it is modulation — and it is exactly where a
compressor dirties the low end.

**The same 5 ms on an 8 kHz hat is forty cycles.** An eternity.

One number physically cannot serve both. But in HOUZY every tone arrives **with its own
frequency**, so "one cycle" is a quantity we actually have. Attack is set in cycles and
turns itself into the right number of milliseconds at every frequency.

> One knob, one position: **45 ms at 50 Hz and 1 ms at 8 kHz.**
> The attack is **never faster than half a cycle** of the tone — the bass cannot be
> modulated at all, and that is guaranteed by the design rather than by careful setting.

The knob is labelled **CYCLES**.

### Release in beat fractions, not milliseconds

A release dialled in at 128 BPM is wrong at 124: the beat has moved, the milliseconds
have not. The compressor stops landing with the music, and it reads as a mix that will
not sit together.

**HOUZY takes the tempo from the project settings** — the host reports the number you
set, rather than guessing it from the audio. The release is a fraction of a bar and
does not drift when the tempo changes.

> Works at **any tempo** — 60 or 190 alike. A beat fraction stays a beat fraction, and
> the plugin does the conversion.

The knob is labelled **BEATS**.

### BEAT | SMART

A switch sits under the release knob:

- **BEAT** — exactly the beat fraction you asked for;
- **SMART** — that fraction as a **maximum**: a tone that has already died away is let
  go early instead of holding an empty pause. You hear it on a skipped kick and on
  syncopation.

There is also **AUTO**: the plugin measures how long the material actually rings and
picks the nearest note value. That is a measurement, not a guess — how long a sound
rings is a fact about the audio, unlike the shape of an attack, which is a matter of
taste. That is why ATTACK deliberately has no such button.

---

## ACR — a clipper that stops chopping the highs

An ordinary clipper **slices** the top off the waveform. A slice is an abrupt event, and
its error is broadband. So every kick hit subtracts that error from everything riding on
top of it: the hats, the vocal, the reverb tails.

Hence the classic problem with clipped house: **the harder you drive the kick, the
duller and grittier the top end gets.**

**Oversampling does not fix this.** 16x makes the error clean, but it does not change
*where* the error sits in the spectrum.

**Instead of slicing, ACR subtracts a short pulse** placed exactly at the peak. The
pulse's spectrum is shaped so the distortion lands where the kick itself masks it. The
peak comes off just as flat, but the top end survives.

The technique is borrowed from mobile network transmitters, where it is applied to LTE
signals, and the pulse kernel is designed from a psychoacoustic model of hearing.

> **ACR and a plain clipper match in loudness** — you can compare them directly with no
> level matching. That is not luck: with equal weights the pulse degenerates into
> exactly an ordinary hard clip, which has been verified numerically.

---

## What else is in there

- **Spectral limiter** — 6 bands, so a peak in the bass does not duck the highs
- **Upward compression** — lifts the quiet parts: reverb tails, air, detail
- **ALL MIX** — 4 bands with their own knobs when you want control per range
- **A / B** — two settings slots, compared at matched loudness
- **Oversampling up to 64x**, honest RMS / LUFS meters, per-stage gain reduction graph
- **English and Russian** interface, language follows the system on first run

---

## Installing

**Windows** — download `HOUZY-Setup.exe` and run it.
The plugin lands in `C:\Program Files\Common Files\VST3`.

**macOS** — download `HOUZY-Installer.pkg` and pick VST3 and/or AU (Logic, GarageBand).

> **macOS will warn you on first launch** — the plugin is not signed with an Apple
> certificate. Right-click the `.pkg` → **Open** → Open.
> Or: System Settings → Privacy & Security → "Open Anyway".

Building from source needs CMake ≥ 3.22 and C++17. JUCE is fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## Licence

**Free.** Music made with HOUZY can be sold and released with no royalties and no
credit required.

You may not: sell the plugin itself, include it in paid bundles, present it as your own
work, modify and redistribute it, or borrow the processing methods used in it for other
products.

Full text in `installer/LICENSE_EN.txt`.

---

<div align="center">

**ACRONA AUDIO**

</div>
