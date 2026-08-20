<div align="center">

**English** · [Русский](README.ru.md)

# HOUZY

<sup>**v4.4.3** · 20 August 2026</sup>

**A next-generation mastering compressor**

Three original technologies: **HOUZY** — compression with no shared gain,
**ACR** — a clipper that stops chopping the highs,
**CYCLES / BEATS** — attack in wave cycles and release in beat fractions.

[![Version](https://img.shields.io/badge/version-4.4.3-5fd0e2?style=flat-square)]()
[![Windows](https://img.shields.io/badge/Windows-VST3-5fd0e2?style=flat-square)]()
[![macOS](https://img.shields.io/badge/macOS-VST3%20%2B%20AU-5fd0e2?style=flat-square)]()
[![Free](https://img.shields.io/badge/price-free-3ddc84?style=flat-square)]()

<br>

### [⬇ Windows](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-Setup.exe) · [⬇ macOS installer](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-Installer.pkg)

Windows: installer · 13 MB — or [VST3 as a zip](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-VST3-Windows.zip) for a manual install
macOS: installer · 42 MB — puts VST3 and AU where they belong. Or the bundles on their own: [VST3](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-macOS-VST3.zip) · [AU](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-macOS-AU.zip) — **AU** is the one Logic and GarageBand use

<br>

<img src="panel.jpg" width="820" alt="HOUZY">

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

- **GAIN MATCH** — matches the output to the input level so **BYPASS compares character
  rather than loudness**. Without it the plugin is always louder, and "better" just
  means "louder"
- **DASH** — one knob takes the compressor from punchy to even
- **Spectral limiter** — 6 bands, so a peak in the bass does not duck the highs
- **Upward compression** — lifts the quiet parts: reverb tails, air, detail
- **ALL MIX** — 6 bands with their own knobs when you want control per range
- **A / B** — two settings slots, compared at matched loudness
- **Oversampling up to 64x**, honest RMS / LUFS meters, per-stage gain reduction graph
- **English and Russian** interface, language follows the system on first run

---

## Installing

**Windows** — download `HOUZY-Setup.exe` and run it.
The plugin lands in `C:\Program Files\Common Files\VST3`.

**macOS** — download `HOUZY-Installer.pkg`, double-click it, and tick the formats
you want. The installer puts them where they belong.

> **The system will block the package on first launch** — it is not signed with an Apple
> certificate, and macOS quarantines anything unsigned. Get past it with
> **right-click the file → Open → Open** again in the warning. Or: System Settings →
> Privacy & Security → "Open Anyway" at the bottom.
>
> Nothing is broken and nothing is infected — macOS treats every unsigned installer
> this way.

Both formats are Universal — Apple Silicon and Intel alike.
**AU** is the one Logic and GarageBand use, **VST3** is for everything else.

<details>
<summary>Install by hand, without the installer</summary>

Download the zip for the format your DAW uses, unpack it, and drag the bundle into the
matching folder:

| format | put it in | used by |
|---|---|---|
| `HOUZY.vst3` | `/Library/Audio/Plug-Ins/VST3/` | Ableton, Reaper, Cubase, Bitwig, FL |
| `HOUZY.component` | `/Library/Audio/Plug-Ins/Components/` | Logic Pro, GarageBand |

Installing by hand leaves the quarantine flag on, so clear it in Terminal — only the
line for the format you actually installed:

```bash
xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/HOUZY.vst3
xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/HOUZY.component
```

Then restart your DAW and rescan.

</details>
Building from source needs CMake ≥ 3.22 and C++17. JUCE is fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## What's new

## v4.4.3 · 20 August 2026

- **The graph shows the audio now.** The signal enters on the right as it arrived and
  leaves on the left processed — the difference between the two halves is what the
  plugin did to it, read directly off the picture
- **The output is drawn exactly as it lands in the track.** It used to be captured one
  stage early, so the loudest moments appeared taller on screen than they came out in
  the render
- **The trace no longer stutters.** It moved in jerks because the picture refreshed on
  one clock while the data arrived on another; the graph now has its own and scrolls
  evenly
- **The compression scale is finer, and labelled.** The half a dB and one dB you
  actually work with are visible instead of being lost, and depth can be read rather
  than guessed from the shape
- **Two readouts left the corner.** They repeated what the picture already showed, and
  sitting side by side they were computed differently — which was misleading rather
  than informative


## v4.4.2 · 18 August 2026

- **CLIP SHAPE is warm now.** The old curve was symmetrical, and a symmetrical curve can
  only produce hard, glassy overtones — which is exactly where the extra top end came
  from. It now behaves like a tube stage: a soft colour appears while the harshness
  measurably goes *down*. Loudness stays matched, as it always did
- **DASH responds evenly across its whole travel.** Almost all of its effect used to sit
  in the first quarter, with the rest of the knob barely answering. Every turn now gives
  the same amount of change
- **The PUNCH knob is gone.** Measurement showed that at any setting other than zero it
  reproduced the disabled behaviour exactly — it was doing nothing. DASH handles evenness
  on its own now. Projects saved with PUNCH open and play back unchanged
- **CHARACTER no longer disappears in HOUZY.** It stays in place, dimmed, so you can see
  the control exists and simply does not apply in that engine
- **The main AMOUNT knob turns more smoothly and precisely**


## v4.4.1 · 17 August 2026

- **Switching modes no longer drops the sound out.** Changing the clipper, the
  oversampling or the limiter mode while the track played punched a short gap in the
  audio. It sounded like the CPU giving up, but it was not: the plugin was erasing
  audio it still had in flight. Every one of those switches is clean now
- **The ACR clipper finally does what it promised.** A calculation error left it
  behaving exactly like HI-Q — identical output, while still adding its own latency. It
  now works as designed: the top end survives kick hits noticeably better, and the
  leading edge of a transient stays clean. If you have been using ACR, this is the first
  time you are actually hearing it
- **Unevenness on dense material is gone.** The clipper was retuning its own shape too
  abruptly, and on a busy mix that came through as a wobble in the sound
- **GAIN MATCH can no longer push the signal above zero.** It was allowed to turn the
  output up, and doing that after the clipper is what let peaks over the ceiling
- **The gain-reduction graph was rebuilt.** The clipper is a proper curve now instead of
  a few pixels along the bottom edge, and the scale matches what a master actually does,
  so gentle compression reads as movement rather than a flat line

*ACR now removes peaks for real, so the output sits about 0.3 dB quieter than before —
that is the stage doing its job rather than passing the work downstream.*

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

