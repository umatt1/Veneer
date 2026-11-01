# Clean-room statement

Veneer is an independent implementation. It has its own name, artwork, DSP and defaults.

## What we used

- Published DSP literature and standards: ITU-R BS.1770-4 (loudness, true peak), EBU Tech 3341/3342
  (test signals), Linkwitz-Riley crossover design, RBJ biquad cookbook, anti-derivative anti-aliasing
  (ADAA) for waveshapers.
- Publicly available, general descriptions of what a one-knob mix-bus "finisher" does: multiband
  dynamics, harmonic saturation, tone shaping, high-frequency widening, limiting.
- Our own listening and measurements of Veneer itself.

## What we did not use

- No decompilation, disassembly, memory inspection or traffic capture of any commercial plugin.
- No cracked, pirated or otherwise unlicensed copies of any commercial plugin.
- No audio captures or measurements of any commercial plugin used to fit curves, ratios, crossover
  points or presets.
- No text, artwork, preset values or animation copied from any commercial product or its manual.

## Rules for contributors

1. Do not submit code, constants or artwork derived from reverse engineering or from a product you
   are not licensed to analyse. If you can only explain a number by "that's what plugin X does", it
   does not belong here.
2. Parameter IDs, labels and documentation are written in our own words.
3. Other products may be named only to identify them factually (see the README), never in the
   plugin's name, IDs, logo or repository metadata.

This document describes project policy. It is not legal advice.
