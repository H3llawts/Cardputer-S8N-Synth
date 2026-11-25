# S8N Synth

A pocket synth, sampler and groovebox project for S8N’s M5Stack Cardputer, UnitSynth and Unit MIDI.

**v0.1 milestone: playable MIDI synth controller.** Sequencing and sampling are roadmap items, not implemented features. Target: original Cardputer (K132). ADV has not been validated.

## Play now

Connect **one** UnitSynth or Unit MIDI to the Cardputer with a standard Grove cable. The same firmware drives either module; no profile selection is necessary.

- **UnitSynth:** sound comes from the module's speaker.
- **Unit MIDI:** use its dedicated audio/headphone output (not its TRS MIDI ports). Separate mode also forwards the Cardputer's output to external MIDI gear. Bypass mode sends Cardputer notes to the internal synth while connecting external MIDI IN directly to OUT.
- The Cardputer speaker is not used. This version sends MIDI only; it does not listen to MIDI IN, accept a USB MIDI keyboard, or load audio samples.

Boot starts on GM **Saw Lead**, program **82** on screen (81 in MIDI), channel 1, velocity 96, volume 80. Drums use MIDI channel 10.

| Keys | Action |
|---|---|
| `A W S E D F T G Y H U J K` | Chromatic notes C through the next C |
| `1` / `2` | Previous / next GM instrument (PLAY only) |
| `3` / `4` | Volume down / up |
| `5` / `6` | Note velocity down / up |
| `7` / `8` | Octave down / up; C0 through C8 bases |
| `9` | Toggle PLAY / DRUM |
| `0` | Reverb send: 0 / 24 / 64 |
| `Space` or physical `G0` button | Panic: release notes and silence all 16 MIDI channels |
| `Enter` | Help screen / return |

Drum keys in the same order: kick, rimshot, snare, clap, closed hat, open hat, pedal hat, low floor tom, high floor tom, low tom, crash, ride, cowbell.

The keyboard is not velocity-sensitive: the velocity setting applies to subsequent note attacks. Original Cardputer matrix rollover limits which chords register; firmware cannot remove that hardware limit. Changing octave, patch, mode or help state silences existing notes. Release held keys before playing again. Settings reset on reboot in v0.1.

## Wiring

| Grove wire | Original Cardputer | Module |
|---|---|---|
| Black | GND | GND |
| Red | 5V | 5V |
| Yellow | GPIO2, UART TX | UART RX (MIDI input) |
| White | GPIO1, unused by this firmware | Unit MIDI TX / UnitSynth unused pin |

UART is **31250 baud, 8N1**. No I2C addresses, no I2C hub, no crossed Grove cable. Do not connect both modules with an ordinary splitter. Dual-module routing requires a separately verified wiring/power design. Connect modules with power off.

## Build and upload

Install Python and PlatformIO Core, or open this folder in VS Code with PlatformIO IDE.

```sh
python -m pip install platformio==6.2.0
python -m platformio run
python -m platformio run --target upload
```

Run these commands from the repository root. Select a port explicitly if needed:

```sh
python -m platformio run --target upload --upload-port COM5
```

On Linux a port may look like `/dev/ttyACM0`. Uploading replaces the firmware currently installed on the Cardputer. If the board will not enter download mode, use M5Stack's documented G0-at-power-on procedure.

Build outputs are in `.pio/build/cardputer/`. `firmware.bin` is an **application image**, not a merged image for address 0. Use PlatformIO's upload command to write the application, bootloader and partition table at the correct offsets. M5Launcher installation has not yet been tested.

The GitHub Actions workflow builds firmware and uploads the application, bootloader and partition files as an artifact. Download these only from a successful run. No public release is published automatically.

## Validation

The portable note engine has host tests for held notes, same-count key swaps, chords, note-off pairing, panic, mode and octave changes, GM program wrapping, and MIDI value bounds:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iinclude tests/performance_test.cpp -o /tmp/s8n-test
/tmp/s8n-test
```

Hardware validation remains required. Follow [the bench checklist](docs/BENCH_TEST.md) with each module before considering this a release. The UI shows outgoing note activity, not confirmation that a module is connected.

## Next milestones

1. Bench-check both modules, note release behavior and keyboard rollover.
2. Add a nonblocking 16-step drum/bass/lead sequencer, tempo, swing and mutes.
3. Add versioned pattern/settings storage on microSD.
4. Add MIDI input and clock handling after verifying Unit MIDI switch routing.
5. Investigate a separate software sampler/audio path. The SAM2695 is a ROM sound engine; samples cannot simply be uploaded into it.

## Implementation references

- [Cardputer hardware, Grove pins and build configuration](https://docs.m5stack.com/en/core/Cardputer)
- [UnitSynth](https://docs.m5stack.com/en/unit/Unit-Synth)
- [Unit MIDI and switch routing](https://docs.m5stack.com/en/unit/Unit-MIDI)
- [M5Cardputer library](https://github.com/m5stack/M5Cardputer)
- [M5Unit-Synth MIDI implementation](https://github.com/m5stack/M5Unit-Synth)

S8N Synth uses a small, testable UART MIDI sender rather than depending on an additional synth library. M5Stack libraries retain their respective licenses.
