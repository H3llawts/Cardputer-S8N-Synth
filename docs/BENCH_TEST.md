# S8N Synth hardware checks

## v0.2 groovebox

The ESP32-S3 build and host tests can verify the firmware image and file formats. Listening tests, key rollover and SD write latency require the original Cardputer and the actual module.

1. Boot with one attached unit. Check that Live Keys starts on Saw Lead 082. Hold and release A, then use G0 to silence a chord. Run the original Live controls below.
2. Press Tab then Space. Listen for kick, snare, hats, bass, lead and chord. Select patterns 1 through 4. Adjust tempo and swing, mute the kick, then remove and restore a snare hit with Enter. Keep the transport running while changing pages.
3. On Bass, arm R and play a pattern. Disarm R and listen across two passes. On Sound, change patch, cutoff, reverb and level. Return to Live with the backtick key and check its saved Live patch.
4. On Sampler, audition the four factory sounds. Record a short clap, inspect its waveform and sequence it on Sample A. Capture a full 0.75-second hit and confirm that the mic releases and the speaker resumes.
5. Save a project, reboot and recall it. Check all four patterns, track mutes, patch settings and pad routing. Inspect the pad WAV on a computer. Test a boot without the SD card and verify that the factory synth and pads still play.
6. Start a mic take on Project. Return to Grid and play the hardware synth for at least one minute. Finish with R on Project, then play the take with P. Check the length and listen for dropouts. Repeat after several sample captures to check that the shared audio pins switch reliably.
7. On Unit MIDI, verify internal audio through AUDIO, MIDI OUT in Separate mode, and note monitoring and overdub from MIDI IN in BYPASS. On Unit Synth, listen from its own speaker while a sample lane plays from the Cardputer.
8. Try the highest tempo, dense chords and simultaneous hits. Note the card model, tempo, audio state and module if the transport stutters or the recorder reports an overrun.

## Original v0.1 Live Keys checks

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
13. Unit MIDI: verify Live Keys through the internal synth in both switch positions. In Separate mode confirm outbound notes with an external MIDI monitor. The groovebox pages handle external notes in v0.2.
14. Play for ten minutes on battery, watch for resets, UI stalls or missed releases. Power-cycle and verify documented defaults return.

## Failure report

Include commit, Cardputer variant, attached module, switch position, power source, exact key sequence, expected/actual behavior, and serial output if available. Sound quality, module power draw, output latency and physical rollover remain bench measurements, not claims established by host tests.
