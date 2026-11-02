# Voicing

What the sound stages do, the numbers behind them, and what has and has not been checked. Every
taste-dependent constant lives in `Source/dsp/Voicing.cpp`.

**These settings were set from measurements and standard mastering practice. They have not been tuned by
ear.** The figures below describe what the processing does to test signals. Whether that sounds good is for
a listener to judge, and changing a constant in `Voicing.cpp` is the intended way to adjust it.

## What Amount does

Amount runs from 0 to 200%. At 0% every stage is neutral: the tone curve is flat, the compressors have a
ratio of 1, the saturators are off and the widener is flat. At 100% the stages are at the values below.
Beyond that they scale up: the tone gains, saturation drive and widening scale linearly with Amount, and
compression deepens (threshold falls by 3 dB and the ratio rises for every 100%).

Order of processing:

1. **Tone curve**, applied before the band split so the compressors react to the shaped signal.
2. **Three bands** (below 200 Hz, 200 Hz to 5 kHz, above 5 kHz), each compressed, then saturated, then
   scaled by its Weight, Body or Air control.
3. **Widening** of the high-frequency side signal.
4. **Push**, the **limiter**, then **Output**.

## Tone curve

Three RBJ-cookbook sections in series. Gains are at 100% and scale linearly in dB with Amount.

| Section | Frequency | Q | Gain at 100% | Gain at 200% |
|---|---|---|---|---|
| Low shelf | 90 Hz | 0.70 | +1.0 dB | +2.0 dB |
| Broad bell | 900 Hz | 0.45 | +1.2 dB | +2.4 dB |
| High shelf | 9 kHz | 0.60 | +0.8 dB | +1.6 dB |

At 0 dB each section has identical numerator and denominator, so it passes the signal through bit for bit.

## Saturation

A tanh curve in each band, run at twice the sample rate with first-order antiderivative anti-aliasing
(Parker et al., DAFx 2016). The curve is scaled so its slope at the origin is 1, so a quiet signal passes at
unit gain whatever the drive.

| Band | Drive at 100% | Bias | Character |
|---|---|---|---|
| Low | 1.0 | 0.08 | mostly odd harmonics, a little second |
| Mid | 0.9 | 0.12 | a little more second harmonic |
| High | 0.7 | 0 | odd harmonics only, kept low to limit aliasing |

- Drive scales linearly with Amount and is 0 at 0%.
- A band raised with Weight, Body or Air is driven harder: 0.5 dB of extra drive per dB of boost.
  Cutting a band does not reduce its drive.
- The bias moves the operating point so the curve is asymmetric. That produces even harmonics and a DC
  offset, which a 5 Hz high-pass removes. The high-pass acts only on the distortion products; the signal
  itself is only delayed.

Measured (`Tests/SaturatorTests.cpp`):

- The harmonics of a 1 kHz sine match direct numerical integration of the curve to within 0.1 dB.
- A symmetric curve leaves even harmonics below -100 dBFS; the biased one produces them.
- Aliasing for a sine of amplitude 0.5 at a drive of 3 (1.5 at the input of tanh), largest spur relative to
  the fundamental:

  | Tone | Plain tanh | Here |
  |---|---|---|
  | 7 kHz | -34.5 dBc | -98.4 dBc |
  | 11 kHz | -18.4 dBc | -69.5 dBc |
  | 13 kHz | -18.4 dBc | -76.5 dBc |

  The worst spurs sit near 19 kHz, where the half-band filter's transition band folds.

- Through the whole engine at 1 kHz and -12 dBFS, harmonics 2 to 5 total -42 dB relative to the fundamental
  at 100% (dominated by the second). At -30 dBFS, where the compressors are idle, they rise 7 dB when Amount
  doubles. At -12 dBFS they rise only 2 dB, because the deeper compression lowers the level into the
  saturator.

## Widening

The side signal (L minus R, halved) goes through a high shelf at 6 kHz, Q 0.7, of +2 dB at 100% and +4 dB
at 200%. The mid signal is not touched, so the mono sum is unchanged exactly (tested), and identical
channels pass bit for bit. Bass is not narrowed: that would change the mix at 0% and is a mastering
decision for a dedicated tool.

## Measured effect on programme material

These come from `VeneerRender` on two synthetic references, rendered at the plugin's default push of 3 dB and
level-matched to the input with `--match`, compared with Amount 0 at the same push. They are not recordings
of real music.

- *Programme noise*: 20 Hz to 20 kHz, flat to 200 Hz then falling 6 dB per octave; about -3 dB, -3 dB and
  -18 dB of its energy below 200 Hz, between 200 Hz and 5 kHz and above 5 kHz.
- *Synthetic mix*: kick, snare, hats, bass, a wide pad and a vocal-like buzz, about -3.5 dB, -3 dB and
  -12 dB in the same bands.

Change in level against Amount 0, third-octave bands of the mono sum, dB:

| Band (Hz) | 31 | 63 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | 16k |
|---|---|---|---|---|---|---|---|---|---|---|
| Programme noise, 100% | +1.1 | +0.9 | +0.2 | -0.6 | -0.5 | -0.3 | -0.6 | 0.0 | +2.2 | +2.9 |
| Programme noise, 200% | +1.1 | +0.7 | -0.4 | -1.4 | -1.0 | -0.5 | -1.1 | +0.4 | +4.5 | +5.8 |
| Mix, 100% | +1.5 | +1.5 | +2.0 | -0.7 | -0.7 | -0.7 | -0.9 | -0.6 | +0.6 | +1.7 |
| Mix, 200% | +1.8 | +1.8 | +1.7 | -1.1 | -0.7 | -0.6 | -1.2 | -1.0 | +0.6 | +2.7 |

The pattern is a lift in the lows and highs with the mids, which take the most compression, a little
behind. Widening raises the side-to-mid ratio by about 2 dB above 8 kHz and 0.3 dB at 4 kHz, and leaves it
unchanged below that; that is read from the programme noise, which has side content at every frequency (the
synthetic mix has almost none at low frequencies, so its ratios there mean little). The large
high-frequency figures on programme noise are partly because that signal has almost no energy there, so any
added harmonics are visible.

Gain reduction per band (low / mid / high), dB, at the default push:

| Signal | Amount | Mean | Peak |
|---|---|---|---|
| Programme noise | 100% | 2.0 / 3.6 / 0.0 | 2.6 / 3.9 / 0.0 |
| Programme noise | 200% | 5.3 / 7.3 / 0.2 | 6.0 / 7.8 / 0.3 |
| Synthetic mix | 100% | 1.5 / 4.0 / 0.8 | 2.8 / 5.6 / 4.1 |
| Synthetic mix | 200% | 5.1 / 7.8 / 3.5 | 6.3 / 9.9 / 7.8 |

The mid band is compressed hardest. A mean of 4 dB at the default is on the heavy side, and it is the first
thing to look at when tuning. The means are the average of each 512-sample block's peak reduction, so they
read slightly high.

## Auditioning

```sh
VeneerRender in.wav a.wav --amount 0   --match
VeneerRender in.wav b.wav --amount 100 --match
```

Both files come out at the input's loudness, so a difference you hear is tone and dynamics rather than level.
`--json file` writes the figures that the tool prints.

## Not done

- No A/B against any other product. There is no reference copy, and the policy in `docs/LEGAL.md` rules out
  measuring one to fit curves.
- No listening-based tuning, and the tone curve, drive, bias and widening values are first guesses.
- The reference signals are synthetic. Real material, especially with a different low-frequency balance, will
  move the compression figures.
