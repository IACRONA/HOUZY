<div align="center">

**English** · [Русский](README.ru.md)

# HOUZY

<sup>**v4.4.0** · 17 August 2026</sup>

**A next-generation mastering compressor**

Three original technologies: **HOUZY** — compression with no shared gain,
**ACR** — a clipper that stops chopping the highs,
**CYCLES / BEATS** — attack in wave cycles and release in beat fractions.

[![Version](https://img.shields.io/badge/version-4.4.0-5fd0e2?style=flat-square)]()
[![Windows](https://img.shields.io/badge/Windows-VST3-5fd0e2?style=flat-square)]()
[![macOS](https://img.shields.io/badge/macOS-VST3%20%2B%20AU-5fd0e2?style=flat-square)]()
[![Free](https://img.shields.io/badge/price-free-3ddc84?style=flat-square)]()

<br>

### [⬇ Windows](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-Setup.exe) · [⬇ macOS VST3](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-macOS-VST3.zip) · [⬇ macOS AU](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-macOS-AU.zip)

Windows: installer · 13 MB — or [VST3 as a zip](https://github.com/IACRONA/HOUZY/raw/main/Releases/HOUZY-VST3-Windows.zip) for a manual install
macOS: Universal (Apple Silicon + Intel) — **AU** is the one Logic and GarageBand use

> **macOS is currently on 3.9 and an update is on the way** — a fresh build lands
> shortly. The macOS version is compiled on a real Mac rather than automatically, so it
> trails Windows. The current build works; it simply does not have the newer controls
> or the ACR fix yet.

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

- **PUNCH** — how much of the hit stays untouched. A sound that has just started is
  normally not compressed at all: the reduction eases in, so the leading edge passes
  through whole. This knob decides how much of that protection to take away — left for
  a punchy, alive hit, right for flat and dense
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

**macOS** — download the zip for the format your DAW uses, unpack it, and drag the
bundle into the matching folder:

| format | put it in | used by |
|---|---|---|
| `HOUZY.vst3` | `/Library/Audio/Plug-Ins/VST3/` | Ableton, Reaper, Cubase, Bitwig, FL |
| `HOUZY.component` | `/Library/Audio/Plug-Ins/Components/` | Logic Pro, GarageBand |

Both are Universal — Apple Silicon and Intel alike.

> **macOS will block it on first launch** — the plugin is not signed with an Apple
> certificate, and unsigned plugins are quarantined. Clear that with one command in
> Terminal:
>
> ```bash
> xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/HOUZY.vst3
> xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/HOUZY.component
> ```
>
> Run only the line for the format you installed. Then restart your DAW and rescan.

Building from source needs CMake ≥ 3.22 and C++17. JUCE is fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## What's new

## v4.4.0 · 17 August 2026

- **The ACR clipper finally does what it promised.** A calculation error left it behaving
  exactly like HI-Q — identical output, while still adding its own latency. It now works
  as designed: the top end survives kick hits noticeably better, and the leading edge of
  a transient stays clean. If you have been using ACR, this is the first time you are
  actually hearing it
- **Unevenness on dense material is gone.** The clipper was retuning its own shape too
  abruptly, and on a busy mix that came through as a wobble in the sound. It now settles
  smoothly, with no loss of the top end it exists to protect

*ACR now removes peaks for real, so the output sits about 0.3 dB quieter than before —
that is the stage doing its job rather than passing the work downstream.*

## v4.3.1 · 14 August 2026

- **The plugin tells you when there is a new version.** **UPDATE** lights up in the top
  left; clicking it asks which system you are on — Windows, macOS VST3 or macOS AU — so
  a Mac user is never handed a Windows file. The notice stays lit until you actually
  update. Switch it off with the checkbox in the ACRONA card; the only thing that leaves
  your machine is a request for the version number, with no statistics and no identifiers
- **The installer filename carries the version** — `HOUZY-4.3.1-Setup.exe`. A month
  later you can tell which of the downloaded files is the recent one without opening
  its properties

## v4.3 · 14 August 2026

### Two new controls

- **GAIN MATCH** — matches the output to the input level so **BYPASS compares character
  rather than loudness**. Without it the plugin is always louder, and "better" just
  means "louder". Top right, above the meters; off by default
- **PUNCH** — how much of the hit stays untouched. A sound that has just started is
  normally not compressed at all: the reduction eases in, so the leading edge passes
  through whole. This knob decides how much of that protection to take away — left for
  a punchy, alive hit, right for flat and dense. It shares a row with DASH; click the
  label to swap between them

**ALL MIX is now 6 bands** instead of four, with two new splits at 650 and 3050 Hz:

`SUB 0–160 · LOW 160–650 · LO-MID 650–1500 · MID 1500–3050 · HI-MID 3050–7000 · HIGH 7000+`

One knob used to cover everything from 0 to 200 Hz, which meant the sub and the body of
the kick were compressed together — exactly the pair you need to separate on a master.

### The look

**The sound is untouched** — every setting, default and preset behaves exactly as it
did before, so existing projects open and play identically.

- **Depth throughout.** One light source, from above. Every surface is either sunk into
  the panel or raised out of it, buttons press in when clicked, and the dB ticks on the
  meters are engraved into the track rather than drawn on top of it.
- **ALL MIX now tells you when it is off.** Clicking a band knob while ALL MIX is
  disabled pulses the switch that turns it on, with the knob you pressed glowing back in
  time. Those knobs used to do nothing at all, which reads as broken rather than as
  switched off.
- **The meter scale is readable.** The dB numbers were below the contrast a label that
  small needs.
- **Corner and header controls tidied up.** A / B / RU are smaller, evenly spaced and
  legible; the gaps between ALL MIX, BYPASS and LOOKAHEAD are equal in both languages
  instead of one being 68 % wider than the other.
- **Typography down to three sizes** from six — the old ones were too close together to
  read as a hierarchy.

*macOS builds in the current release predate this work.*

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

