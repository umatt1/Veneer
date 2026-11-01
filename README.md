# Veneer

A free, open-source finishing stage for your mix bus. Put it on the master channel, set the input
level, and bring up **Amount** until the mix feels finished.

> **Status: pre-alpha.** The current build is a pass-through skeleton. Nothing below is implemented
> yet except the plugin shell.

## Planned controls

| Control | What it does |
|---|---|
| Input | Trim into the processor. The meter shows where the dynamics are designed to work. |
| Amount | 0–200%. Scales tone, compression, saturation and high-frequency width together. |
| Weight / Body / Air | Per-band level and drive for the low, mid and high bands. |
| Push | How hard the limiter is driven. |
| Output | Final trim after the limiter. |

Meters show per-band gain reduction, limiter gain reduction, true peak and LUFS.

Planned formats: VST3, AU and Standalone on macOS; VST3 and Standalone on Windows and Linux.

## How it compares

Veneer plays a similar role to commercial one-stop mix-bus finishing plugins such as Cradle's
*The God Particle*. It is an independent project with its own design and sound, and it is not
affiliated with, endorsed by, or sponsored by Cradle or anyone else named in this repository.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Veneer_VST3 -j4
```

To reuse an existing JUCE checkout, add `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`. Builds do
not install anything into your plugin folders.

## License

[AGPL-3.0](LICENSE). Third-party components and trademarks are listed in [NOTICE.md](NOTICE.md).
The clean-room policy is in [docs/LEGAL.md](docs/LEGAL.md).
