# v0.1 bench acceptance

Status: **not tested on physical hardware**. Run for UnitSynth and Unit MIDI separately.

1. Build/upload using the pinned PlatformIO configuration. Record module, Cardputer model and commit.
2. Power off, attach one module, then power on. Confirm the screen shows S8N SYNTH and Saw Lead. For Unit MIDI use the dedicated audio output, initially at low listening volume.
3. Tap A. Expect C4. Hold A for five seconds: one continuous note, then silence on release.
4. Hold A/E/G-equivalent piano positions **A/D/G** (C/E/G). Record matrix combinations that register; do not mistake keyboard rollover for engine polyphony limits.
5. Exchange A for S without changing the number of held keys. The old note must stop and the new note must sound. Release everything: silence.
6. Hold A and change octave with 8. A must stop. It must stay silent until released and pressed again; then sound C5. Repeat with patch and mode changes.
7. Hold a note and press Space or G0. All sound stops. Hold Space, press another note, release Space while still holding that note: it must remain silent until repressed.
8. Enter help while holding a note. Silence. Hold a note while closing help: no short retrigger or stuck note. Release and press to resume.
9. Press 9: A triggers kick, S snare, D closed hat, F open hat. Toggle back and confirm melodic channel still works.
10. Exercise patch 1/128 wrapping, volume minimum/maximum, velocity minimum/maximum, and octave limits. No invalid MIDI data or unexpected note-on at velocity zero.
11. Cycle reverb using 0; assess on a sustained piano/pad patch. Volume keys should affect both melodic and percussion modes.
12. Hold Shift/Fn while playing and release modifiers before notes: no stranded notes. Document keyboard combinations that hardware cannot scan reliably.
13. Unit MIDI: verify internal synth in both switch positions. In Separate mode confirm outbound notes with an external MIDI monitor. MIDI IN processing is intentionally absent in v0.1.
14. Play for ten minutes on battery, watch for resets, UI stalls or missed releases. Power-cycle and verify documented defaults return.

## Failure report

Include commit, Cardputer variant, attached module, switch position, power source, exact key sequence, expected/actual behavior, and serial output if available. Sound quality, module power draw, output latency and physical rollover remain bench measurements, not claims established by host tests.
