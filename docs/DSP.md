# DSP design

All processing is in `Source/dsp`. `Engine` ties the stages together and has no dependency on the plugin
wrapper, so the tests drive it directly.

## Signal path

```
input trim -> 3-band LR4 split -> per-band compressor + band gain -> sum -> push -> limiter -> output trim
                                                                                       |
                                                    dry path delayed by the limiter latency (bypass)
```

Meters tap the input after the trim and the final output.

## Crossover

`Crossover` splits at 200 Hz and 5 kHz with Linkwitz-Riley 4th-order filters (`juce::dsp::LinkwitzRileyFilter`).
The high band passes through an all-pass at the low crossover so all three bands share one phase response and
sum to an all-pass of the input. The magnitude is flat; the phase is not. That means the processor with
everything neutral is magnitude-transparent but is not a sample-exact null against the input. Bypass is
sample-exact.

## Band compressor

Feed-forward, stereo-linked (the larger of the two channel levels drives both channels).

1. Level: one-pole mean square, scaled so a sine of peak amplitude A reads as A.
2. Static curve: soft knee, as in Giannoulis, Massberg and Reiss, "Digital Dynamic Range Compressor Design:
   A Tutorial and Analysis" (JAES, 2012).
3. Ballistics: gain reduction is smoothed in the dB domain. Release blends between a fast and a slow time
   constant according to how much reduction has been sustained over the last ~300 ms, so short transients
   recover quickly and held compression recovers slowly.

A ratio of exactly 1 returns a gain of exactly 1.

## Limiter

Lookahead brick-wall limiter with true-peak detection.

1. `TruePeakDetector` interpolates 8x with a Kaiser-windowed sinc (513 taps, polyphase). Its phase-0 output is
   the input delayed by 32 samples, so the detector never reads below the sample peak.
2. Required gain per sample is `min(1, limit / peak)`, then the minimum of the current and previous value so
   both intervals touching a sample are protected. `limit` is the ceiling minus a 0.3 dB margin.
3. A sliding minimum over N samples (N = 2 ms) makes the gain fall before the peak arrives.
4. Release: the envelope follows the minimum instantly downward and releases exponentially upward. The release
   time moves between 30 ms and 400 ms according to how much reduction has been sustained, as in the
   compressor.
5. A moving average over N samples smooths the corners. Because the envelope is never above the windowed
   minimum, the average cannot exceed the gain each sample needs.
6. The audio is delayed by `(N - 1) + 32` samples so the gain lines up with the peak it protects. That sum is
   the plugin's reported latency (127 samples, 2.6 ms, at 48 kHz) and does not depend on any parameter.
7. A final clamp at the ceiling guards the sample peak against rounding.

### What the ceiling means

The ceiling is a guarantee, not a target. The 0.3 dB margin means a simple tone peaks 0.3 dB under it.

The margin exists because no practical detector sees everything. The interpolation filter rolls off near
Nyquist, so energy there hides inter-sample peaks. Two measurements shaped the design:

- The size of the filter matters more than the oversampling factor for broadband material. On a heavily
  limited pink-noise render, 4x with 64 taps per phase read closer to the truth than 8x with 32. The detector
  is 8x with 64, which keeps both the fine grid that high-frequency sines need and a narrow transition band.
- With that detector, the largest underestimate seen on real limited noise was 0.26 dB, so the margin is 0.3 dB.

Measured against an ideal 16x sinc reconstruction (`LimiterTests.cpp`, `EngineTests.cpp`), overshoot above the
ceiling, where negative means under it:

| Signal | Overshoot |
|---|---|
| Pink-ish noise, about 10 dB of limiting | -0.26 dB |
| Sweep 20 Hz to 20 kHz, fs/4 inter-sample sine, pulses | -0.30 dB |
| Hard square wave | -0.26 dB |
| Full-band white noise | -0.15 dB |
| 20 s of pink noise rendered at extreme settings, ceiling -1.0 | -0.04 dB |
| The same render with ceiling -0.3 | -0.18 dB |

Material with energy right at Nyquist can still hide peaks from the detector, so the guarantee is stated for
the signals above rather than for every possible waveform.

A second, short safety stage that measured the first stage's output was tried and removed: it changed
nothing on any of the synthetic signals and cost 31 samples of latency.

## Loudness

`LoudnessMeter` implements ITU-R BS.1770-4 with the EBU R 128 time scales.

- K-weighting: the shelf and high-pass biquads are computed for any sample rate and match the normative
  48 kHz coefficients (checked in `LoudnessTests.cpp`).
- Energy is accumulated in 10 ms sub-blocks. Momentary is the last 400 ms, short-term the last 3 s.
- Integrated loudness uses 400 ms blocks every 100 ms, the -70 LUFS absolute gate and the -10 LU relative
  gate. Blocks go into a fixed 0.01 LU histogram, so memory is constant and nothing allocates on the audio
  thread.

## Verification

`Tests/` covers the crossover response, compressor static curve and ballistics, limiter ceiling on hostile
signals (checked against an independent 16x sinc reference), the loudness meter against the signals defined in
EBU Tech 3341 (synthesized from the description in the document) and the engine end to end.

Not covered: EBU Tech 3341 cases 6, 7, 8 and 20-23. They need the multichannel or programme files, or the
exact resampler, from the EBU test set.

## Cost

Release build, stereo, measured on the development machine (Apple silicon):

| Sample rate | Speed | CPU |
|---|---|---|
| 48 kHz | 65x realtime | 1.5% of one core |
| 96 kHz | 32x realtime | 3.1% of one core |
| 192 kHz | 16x realtime | 6.1% of one core |

## Not built yet

Voicing EQ, saturation and high-frequency widening (milestone 2). `Voicing.cpp` holds placeholder
compressor settings that will be tuned there.
