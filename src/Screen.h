#pragma once
#include <stdint.h>

enum class Page : uint8_t { Live, Grid, Sound, Sample, Project };

// All selections are 0-based. Cell 0 is the low bit of each lane mask.
// The sampler envelope contains magnitudes from 0..127. Text pointers only
// need to remain valid for the duration of draw().
struct ScreenState {
  Page page;
  bool running, armed, sdReady, capturing, takeRecording, takePlaying, drums;
  uint8_t pattern, track, step, playhead, patch, cutoff, reverb, volume;
  uint8_t soundField, slot, laneSlot, projectSlot, swing, velocity, gate, note, octave;
  uint16_t bpm, sampleMs, captureMs, held;
  uint32_t takeSeconds;
  uint16_t steps[8], accents[8];
  uint8_t muteMask, wave[64];
  const char *notice, *hint, *patchName;
  // Set helpLines[0] to show a help overlay for the currently selected page.
  const char *helpLines[8];
};

class Screen {
public:
  // Returns whether the framebuffer was allocated. draw() also works
  // without it, using a compact direct-to-display view.
  bool begin();
  void draw(const ScreenState& state);
};
