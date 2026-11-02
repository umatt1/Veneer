# Veneer

[![CI](https://github.com/umatt1/Veneer/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/umatt1/Veneer/actions/workflows/ci.yml)

A free, open-source finishing stage for your mix bus. Put it on the master channel, set the input
level, and bring up **Amount** until the mix feels finished.

> **Status: pre-alpha.** The signal chain and the interface are built and tested: tone curve, multiband
> dynamics, saturation, high-frequency widening, a true-peak limiter, loudness metering and a scalable custom
> interface. CI builds and tests it on macOS, Linux and Windows. The VST3 passes pluginval on all three, the Audio
> Unit passes pluginval and `auval`, and the Intel build passes its tests under Rosetta. In REAPER on macOS the VST3
> renders the same audio as the engine. It has not been tried in Logic or in any host on Windows or Linux, and the
> voicing has been measured but not tuned by ear.
> See [docs/VOICING.md](docs/VOICING.md) for what it does and how to audition it,
> [docs/UI.md](docs/UI.md) for the interface and [docs/DSP.md](docs/DSP.md) for how it is built and tested.

## Controls

| Control | What it does |
|---|---|
| Input | Trim into the processor. The meter shows where the dynamics are designed to work. |
| Amount | 0–200%. Scales tone, compression, saturation and high-frequency width together. |
| Weight / Body / Air | Per-band level and drive for the low, mid and high bands. |
| Push | How hard the limiter is driven. |
| Output | Final trim after the limiter. |

Meters show the input level against a lit target band, per-band and limiter gain reduction, loudness (momentary,
short-term and integrated LUFS) and true peak. The interface scales from 75% to 200%.

Formats: VST3, AU and Standalone on macOS (universal, Apple silicon and Intel). VST3 and Standalone for Windows and
Linux. CI runs the unit tests on the Windows and Linux builds, but nobody has tried them in a host yet.

## Install

Download the file for your system from the [Releases page](https://github.com/umatt1/Veneer/releases). The first
release is a pre-release, so GitHub does not mark it "latest".

| System | File | Copy it to |
|---|---|---|
| macOS | `Veneer-<version>-macOS.zip` | `Veneer.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, `Veneer.component` to `~/Library/Audio/Plug-Ins/Components/` |
| Windows | `Veneer-<version>-Windows.zip` | the `Veneer.vst3` folder to `C:\Program Files\Common Files\VST3\` |
| Linux | `Veneer-<version>-Linux.tar.gz` | the `Veneer.vst3` folder to `~/.vst3/` |

Then rescan plug-ins in your host. The macOS builds are signed ad hoc and not notarised, so macOS blocks them until
you clear the download flag from what you installed:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Veneer.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Veneer.component
```

Each archive has an `INSTALL.txt` with the same steps, and the release has a `SHA256SUMS.txt` for checking the
downloads.

## How it compares

Veneer plays a similar role to commercial one-stop mix-bus finishing plugins such as Cradle's
*The God Particle*. It is an independent project with its own design and sound, and it is not
affiliated with, endorsed by, or sponsored by Cradle or anyone else named in this repository.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Veneer_VST3 -j4
```

Tests:

```sh
cmake -S . -B build-test -DVENEER_BUILD_TESTS=ON
cmake --build build-test --target VeneerTests -j4
ctest --test-dir build-test --output-on-failure
```

To reuse an existing JUCE checkout, add `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE`. Builds do
not install anything into your plugin folders.

### Seeing and hearing it without installing it

`VeneerRender` runs a WAV file through the same engine and prints loudness and peak figures:

```sh
cmake -S . -B build-tools -DVENEER_BUILD_TOOLS=ON
cmake --build build-tools --target VeneerRender -j4
build-tools/VeneerRender in.wav out.wav --amount 100 --push 3 --match
```

`--match` scales the output to the input's integrated loudness so two settings can be compared at equal level.

`VeneerSnapshot` draws the real interface offscreen to a PNG, with the meters filled by playing a WAV through the
processor first. It opens no window and touches no audio device (see [docs/UI.md](docs/UI.md)):

```sh
cmake --build build-tools --target VeneerSnapshot -j4
build-tools/VeneerSnapshot shot.png --wav in.wav --seconds 8 --pixels 2
```

## License

[AGPL-3.0](LICENSE). Third-party components and trademarks are listed in [NOTICE.md](NOTICE.md).
The clean-room policy is in [docs/LEGAL.md](docs/LEGAL.md).
