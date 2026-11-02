# DSP design

All processing is in `Source/dsp`. `Engine` ties the stages together and has no dependency on the plugin
wrapper, so the tests drive it directly. The tone curve, saturation and widening values are in
[VOICING.md](VOICING.md).

## Signal path

```
input trim -> tone curve -> 3-band LR4 split -> per-band compressor -> saturator -> band gain -> sum
           -> widener -> push -> limiter -> output trim

bypass: the input, delayed by the total latency, crossfaded in
```

Meters tap the input after the trim and the final output. The engine publishes the last block's values
(input RMS, per-band and limiter gain reduction, true peak, loudness) as atomics. It also keeps a second set that
holds the largest gain reduction and true peak since the interface last took them, so a display that refreshes
at 30 Hz sees every peak between refreshes.

## Latency

The plugin reports one latency, which does not change with any parameter:

| Source | Samples at 48 kHz |
|---|---|
| Saturator oversampling filters | 15 |
| Limiter look-ahead (2 ms window, 95 samples) plus detector delay (48) | 143 |
| **Total** | **158 (3.3 ms)** |

Bypass delays the dry signal by the same total, so switching is click-free and sample-exact.

## Tone curve, saturation and widening

See [VOICING.md](VOICING.md) for values and measurements. Structure:

- **Tone curve** (`ShelfBellEq`): RBJ low shelf, bell and high shelf in series, in double precision. A section
  at 0 dB has identical numerator and denominator, so the signal passes bit for bit.
- **Saturator** (`Saturator`, `HalfbandOversampler`): a tanh curve at twice the sample rate with first-order
  antiderivative anti-aliasing. Only the distortion products go through the oversampler: the output is the
  input delayed by 15 samples plus the decimated difference between the shaped and unshaped signal. The
  curve has a slope of 1 at the origin, so small signals pass at unit gain and a drive of 0 is an exact
  delay. The half-band filters are linear-phase Kaiser windowed sincs (31 taps, beta 7, unity gain at DC)
  in polyphase form: one phase is a delayed copy and the other a 16-tap FIR.
- **Widener** (`Widener`): a high shelf on the side signal only, leaving the mid untouched.

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

1. `TruePeakDetector` interpolates 8x with a Kaiser-windowed sinc (769 taps, beta 3, polyphase, 96 taps per
   phase). Its phase-0 output is the input delayed by 48 samples, so the detector never reads below the
   sample peak.
2. Required gain per sample is `min(1, limit / peak)`, then the minimum of the current and previous value so
   both intervals touching a sample are protected. `limit` is the ceiling minus a 0.3 dB margin.
3. A sliding minimum over N samples (N = 2 ms) makes the gain fall before the peak arrives.
4. Release: the envelope follows the minimum instantly downward and releases exponentially upward. The release
   time moves between 30 ms and 400 ms according to how much reduction has been sustained, as in the
   compressor.
5. A moving average over N samples smooths the corners. Because the envelope is never above the windowed
   minimum, the average cannot exceed the gain each sample needs.
6. The audio is delayed by `(N - 1) + 48` samples so the gain lines up with the peak it protects.
7. A final clamp at the ceiling guards the sample peak against rounding.

The dot products in the detector are written with NEON on arm64 and SSE2 on x86-64, with a scalar fallback.
The compiler's own version ran at a sixth of the speed. The SSE2 path is compile-checked only: no x86-64
machine was available, so CI is its first run.

### What the ceiling means

The ceiling is a guarantee for the signals tested below, and a close approximation beyond them. The 0.3 dB
margin means a simple tone peaks 0.3 dB under the ceiling.

The margin exists because no practical detector sees everything. The interpolation filter rolls off near
Nyquist, so energy there hides inter-sample peaks. What the measurements showed:

- Filter length matters more than the oversampling factor. On heavily limited noise, 4x with 64 taps per
  phase read closer to the truth than 8x with 32.
- A 2 s noise test hid the problem. On 12 s of full-band noise driven 25 dB into the limiter, the first
  detector (8x, 64 taps per phase, beta 6) read up to 0.74 dB low, because the highest true peak is chosen from
  thousands of near-peak samples and the tail grows with duration. On a synthetic drum mix at extreme
  settings it overshot by +0.21 dB.
- The detector is now 8x with 96 taps per phase and beta 3. That cut the worst under-read on that mix from
  0.51 dB to 0.27 dB and on the noise from 0.74 dB to 0.47 dB. A lower beta narrows the transition band at
  the cost of a shallower stopband, which makes the detector read noise confined to the top 3 kHz up to
  0.4 dB high. That is the safe direction (`Tests/TruePeakTests.cpp` holds it to under 0.6 dB high and never
  low).
- Cutting off above Nyquist instead was tried and rejected: it over-reads a tone at 0.49 of the sample rate
  by 4 dB.

Overshoot above the ceiling against an independent 16x sinc reconstruction, where negative means under it.

Unit tests (`LimiterTests.cpp`, `EngineTests.cpp`):

| Signal | Overshoot |
|---|---|
| Pink-ish noise, about 10 dB of limiting | -0.25 dB |
| Sweep 20 Hz to 20 kHz | -0.30 dB |
| Inter-sample sine at a quarter of the sample rate | -0.29 dB |
| Pulses | -0.30 dB |
| Hard square wave | -0.34 dB |
| Full-band white noise | -0.25 dB |
| Engine at extreme settings, pink noise, worst channel | -0.25 dB |
| Engine at extreme settings, white noise, worst channel | -0.18 dB |

Renders of 12 to 24 s through `VeneerRender` (`overshoot` here is the ideal reconstruction; ffmpeg's BS.1770
true-peak meter in the last column):

| Material | Settings | Limiter gain reduction | Overshoot | ffmpeg `ebur128` |
|---|---|---|---|---|
| Synthetic mix | defaults, ceiling -1.0 | up to 1.8 dB | -0.29 dB | -1.3 dBTP |
| Synthetic mix | defaults, ceiling -0.3 | up to 1.1 dB | -0.29 dB | -0.6 dBTP |
| Programme noise | defaults | none | -1.9 dB | -2.9 dBTP |
| Synthetic mix | limiter only, +24 dB drive | up to 25 dB | -0.15 dB | -1.2 dBTP |
| Programme noise | limiter only, +24 dB drive | up to 21 dB | -0.29 dB | -1.3 dBTP |
| Synthetic mix | extreme settings | up to 19 dB | **+0.10 dB** | -0.6 dBTP |
| Programme noise | extreme settings | up to 18 dB | -0.28 dB | -1.3 dBTP |
| White noise, 12 s stereo | limiter only, +24 dB drive | up to 25 dB | **+0.30 dB** | -1.2 dBTP |
| White noise, 12 s stereo | Amount 200, +24 dB drive | up to 20 dB | **+0.12 dB** | -1.2 dBTP |

"Extreme settings" means Input +12 dB, Push +12 dB, Amount 200% and Weight, Body and Air at +6 dB.

So at the defaults and under moderate limiting the peak sits 0.3 dB under the ceiling. With 20 dB or more of
limiter gain reduction on full-band material the peak can exceed the ceiling by up to 0.3 dB when measured
against an ideal reconstruction, which no finite detector can see. The meters a producer is likely to use
read differently: ffmpeg's BS.1770 4x meter reads the white-noise cases under the ceiling, and it reads the
extreme mix 0.3 dB above the ideal reconstruction. A harder guarantee would need a larger margin or an
oversampled gain stage; neither has been built.

A second, short safety stage that measured the first stage's output was tried and removed: it shares the
first stage's blind spot, changed nothing on any of the synthetic signals and cost 31 samples of latency.

## Loudness

`LoudnessMeter` implements ITU-R BS.1770-4 with the EBU R 128 time scales.

- K-weighting: the shelf and high-pass biquads are computed for any sample rate and match the normative
  48 kHz coefficients (checked in `LoudnessTests.cpp`).
- Energy is accumulated in 10 ms sub-blocks. Momentary is the last 400 ms, short-term the last 3 s.
- Integrated loudness uses 400 ms blocks every 100 ms, the -70 LUFS absolute gate and the -10 LU relative
  gate. Blocks go into a fixed 0.01 LU histogram, so memory is constant and nothing allocates on the audio
  thread.
- Resetting the integrated value starts a new measurement. It clears the histogram and adds no block until a
  full 400 ms of audio after the reset has passed, so nothing from before it leaks into the first blocks. The
  interface asks for a reset with a flag that the audio thread acts on at the start of its next block.

## Verification

`Tests/` covers the crossover response, compressor static curve and ballistics, the halfband oversampler
(delay, flatness and stopband), the saturator against direct integration of its curve and against plain
tanh for aliasing, the tone curve and widener at the points where their shape fixes the gain, the limiter
ceiling on hostile signals (checked against an independent 16x sinc reference), the loudness meter against
the signals defined in EBU Tech 3341 (synthesized from the description in the document) and the engine end
to end.

Not covered: EBU Tech 3341 cases 6, 7, 8 and 20-23. They need the multichannel or programme files, or the
exact resampler, from the EBU test set.

### Sanitizers

The whole suite also runs under AddressSanitizer with UndefinedBehaviorSanitizer, and under ThreadSanitizer.
On macOS the Command Line Tools' clang has all three:

```sh
san=address,undefined   # or: san=thread, in a separate build directory
flags="-fsanitize=$san -fno-omit-frame-pointer -g"
cmake -S . -B build-san -DVENEER_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_OBJCXX_FLAGS="$flags" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=$san"
cmake --build build-san --target VeneerTests -j4
ASAN_OPTIONS=detect_leaks=0 build-san/VeneerTests
```

Both runs are clean: 110 test cases and 1.7 million assertions, with no reports. A clean run only counts if the
tool can fail, so each one was checked first against a deliberate bug (a heap overflow, a signed overflow, a data
race). `Tests/ConcurrencyTests.cpp` is what gives the thread sanitizer something to find: `processBlock` runs on
one thread while another polls the meters, asks for loudness resets, changes parameters and saves the state, as a
host and the interface do. Putting back the original reset design, which cleared the loudness histogram from
the interface thread, makes it report a data race at once.

## Robustness

The properties below are tests, in `Tests/RobustnessTests.cpp`, `RealtimeTests.cpp`, `TailTests.cpp` and
`StateTests.cpp`.

- **Block size.** With fixed settings the output is bit-identical for any block size: 1, 2, 3, 7, 31, 32, 33,
  64, 100, 513, 1000, 4096 and a million samples all match 512, and empty blocks between real ones change
  nothing. Settings that are moving are applied on a 32-sample grid that starts at each block, so with
  automation the block size can change the result slightly.
- **Channels.** A mono bus gives the same samples as the left channel of a dual-mono pair.
- **State.** `reset()` and `prepare()` leave every stage as a fresh engine would be: the next render is
  bit-identical, including after a change of sample rate.
- **Sample rate.** At 44.1, 48, 88.2, 96, 176.4 and 192 kHz, bypass is the input delayed by the reported latency,
  the ceiling holds at extreme settings (true peak 0.23 to 0.27 dB under it on the test programme), and a 1 kHz
  tone at -23 LUFS reads within 0.021 LU.
- **Outside that range.** 8, 11.025, 16, 22.05 and 32 kHz, and 352.8 and 384 kHz, give finite output under the
  ceiling. A filter designed at a fixed frequency is held below 0.45 of the sample rate, so at a low rate the
  9 kHz shelf, the 6 kHz widening and the 5 kHz crossover move down. The tone changes there, and nothing is
  tuned for those rates.
- **Bad input.** NaN and infinity become silence and anything beyond +/-1000 (+60 dBFS) is clipped to it, before
  any filter sees it. A single NaN sample would otherwise stay in the filters until the next reset.
- **No allocation.** `processBlock` makes no C++ heap allocation: counted over blocks of 1 to 4096 samples at five
  different settings, including bypass. Direct calls to `malloc` are not seen by that count.
- **Tail.** The processor reports 0.3 s. After the input stops, the output falls below -100 dBFS within 0.26 s at
  the worst setting measured (extreme settings, a noise burst) and within 0.09 to 0.17 s at the defaults. That
  includes the latency, and it hardly depends on the sample rate.
- **Saved state.** It holds the parameters, the editor width and a version number (1). Data that is not a Veneer
  state, a state cut off inside its content, and a state with a different root tag are ignored and change
  nothing. JUCE's reader tolerates a missing closing tag, so a state cut off only there loads in full. Values
  outside a parameter's range are clamped, and entries this version does not know are ignored.

## Cost

Release build, stereo, measured on the development machine (Apple silicon):

| Sample rate | Speed | CPU |
|---|---|---|
| 48 kHz | 36x realtime | 2.8% of one core |
| 96 kHz | 18x realtime | 5.5% of one core |
| 192 kHz | 9.0x realtime | 11.0% of one core |

By block size at 48 kHz the cost is flat from 16 to 4096 samples (2.7 to 2.8%). A block of 8 costs 2.9% and a
degenerate block of 1 costs 4.2%.

A sampling profile of this benchmark (macOS `sample`, Release build, 1 ms interval) puts about 78% of the audio
thread's own time in the true-peak interpolation filter, which both detectors share: the limiter's and the one
behind the output meter. Every other stage is a few percent or less, roughly 3% each for the crossover and the
saturators and 1% for the compressors. The shares are approximate because the compiler inlines most small
functions.

## Not built yet

Host testing (milestone 4: REAPER, the AU through `auval`, Logic) and CI on Windows and Linux (milestone 5).
The interface is described in [UI.md](UI.md).
