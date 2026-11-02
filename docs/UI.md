# Interface

The interface is drawn in code, with no image files. The faceplate is laid out once in a 760 by 460 space and
scaled as a whole, so every size is the same picture. The sources are `Source/PluginEditor.*` and
`Source/ui/`; `Layout.h` holds every position and `Style.h` every colour.

## Controls

| Control | Range | Default |
|---|---|---|
| Input | -12 to +12 dB | 0 |
| Amount | 0 to 200% | 100 |
| Weight, Body, Air | -6 to +6 dB | 0 |
| Push | 0 to 12 dB | 3 |
| Output | -12 to 0 dB | 0 |
| Bypass | on / off | off |

- Drag a knob, or scroll over it. Double-click or Cmd/Ctrl-click returns it to its default, which is the small
  tick outside the ring.
- Click the value under a knob to type one. `3`, `+3 dB` and `150%` are all accepted.
- Knobs and buttons take keyboard focus: Tab moves between them and the arrow keys step a focused knob.
- Hovering shows a short description of each control.

The gear opens a menu with the interface size, the true-peak ceiling (-1.0 or -0.3 dBTP) and a reset for the
loudness readouts.

## Meters

| Meter | Shows | Behaviour |
|---|---|---|
| Input | RMS of the signal after the Input trim, -48 to 0 dBFS | 300 ms averaging in the engine. Dim teal below the lit band, bright teal inside it, amber above, coral above -9 dBFS |
| Weight, Body, Air bars | Gain reduction in that band, 0 to 12 dB | The largest reduction since the last redraw. Rises at once, falls at 40 dB/s |
| LIM | Gain reduction in the limiter, same scale | Same |
| Readout | Momentary (400 ms), short-term (3 s) and integrated loudness in LUFS, and true peak in dBTP | Dashes below -70. The peak is held since the last reset and turns coral above the ceiling by more than 0.05 dB. Click it to reset the integrated value and the peak |

The meters read held peaks from the engine (see [DSP.md](DSP.md)), so a display refreshing at 30 Hz does not miss
a short peak that falls between two refreshes.

### The lit band on the Input meter

The band is -18 dBFS RMS, give or take 3 dB (`targetLevelDb` and `targetHalfWidthDb` in `Source/dsp/Voicing.h`).
It is where the dynamics are voiced to work, and it was set from this measurement, not by ear. Two reference
signals, both at -18.0 dBFS RMS with Input at 0, were rendered through `VeneerRender` with Amount 100 and Push 3
at different Input trims. The meter reading is -18 plus the trim.

| Input meter (dBFS RMS) | -27 | -24 | -21 | -18 | -15 | -12 |
|---|---|---|---|---|---|---|
| Mid band reduction, mean, synthetic mix | 0.1 | 0.8 | 2.1 | 4.0 | 5.6 | 7.2 |
| Mid band reduction, mean, programme noise | 0.1 | 0.6 | 1.9 | 3.6 | 5.2 | 6.9 |
| Limiter reduction, peak, synthetic mix | 0.0 | 0.0 | 0.0 | 1.8 | 3.4 | 4.8 |
| Limiter reduction, peak, programme noise | 0.0 | 0.0 | 0.0 | 0.0 | 0.1 | 2.0 |

Below about -27 the compressors are idle. Across the band the mid compressor averages roughly 2 to 5.5 dB. Both
signals are synthetic (see [VOICING.md](VOICING.md)), and the voicing itself has not been tuned by ear, so if the
voicing moves, the band should move with it.

## Size

Drag the corner to scale continuously from 75% to 200% with the aspect ratio held, or pick a step from the gear
menu (75, 100, 125, 150, 200%). The width is saved in the session, so a reopened editor comes back at the size
it was left at. A session saved before this existed, or a new instance, opens at 100%.

The faceplate is a child of the editor and carries the scaling transform. The editor itself must not, because
the host's scale factor lives there.

## Seeing it without a host

`VeneerSnapshot` renders the real editor offscreen to a PNG. With `--wav` it plays the file through the
processor first, refreshing the meters at the timer rate, so the meters show what they would show in use:

```sh
cmake -S . -B build-tools -DVENEER_BUILD_TOOLS=ON
cmake --build build-tools --target VeneerSnapshot -j4
build-tools/VeneerSnapshot shot.png --wav mix.wav --seconds 8 --pixels 2 --scale 1.0 --set amount=150
```

`--pixels 2` writes twice the resolution for a Retina screenshot. It does not open a window or touch an audio
device.

## Not done

- It has not been opened in a host yet. That is part of milestone 4: pluginval opens and resizes the VST3
  editor, but nothing has run the AU editor, the standalone app or a DAW.
- The meters and the loudness readout are not exposed to screen readers. They change continuously, and there is
  no other route to the same numbers. Knobs and buttons have names and keyboard focus, which is what JUCE builds
  its screen-reader interface from, but that was not tried with a screen reader.
- Fonts are the system's default sans serif, so the look differs a little between platforms.
- There is no preset menu or A/B switch.
