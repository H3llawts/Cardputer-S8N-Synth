#include "Screen.h"
#include <M5Cardputer.h>
#include <stdio.h>
#include <string.h>

namespace {
M5Canvas canvas;
bool ready = false;
using Ink = uint8_t;
constexpr Ink Back = 0, Surface = 1, GridA = 2, GridB = 3, Edge = 4, Quiet = 5;
constexpr Ink Text = 6, Acid = 7, Moss = 8, Pink = 9, Aqua = 10, Amber = 11;
constexpr uint32_t colors[] = {
    0x0c1010, 0x181f20, 0x263032, 0x303c3e, 0x475755, 0xa0adaa,
    0xf3f4ea, 0xc8fa56, 0x86a65c, 0xff78aa, 0x7de4cc, 0xffc16b};
constexpr const char* pageNames[] = {"LIVE KEYS", "STEP GRID", "SOUND", "SAMPLER", "PROJECT"};
constexpr const char* lanes[] = {"KICK", "SNARE", "HAT", "BASS", "LEAD", "CHORD", "SMP A", "SMP B"};
constexpr const char* hints[] = {
    "TAB GROOVE   ENTER HELP   SPACE PANIC",
    "ENTER STEP   SPACE PLAY   TAB PAGE  ?",
    ";. FIELD   ,/ VALUE   TAB PAGE  ?",
    ",/ SLOT   R REC   ENTER PLAY   ?",
    ",/ SLOT   S SAVE   L LOAD   R REC  ?"};

void label(int x, int y, const char* value, Ink ink, uint8_t size = 1) {
  canvas.setTextSize(size);
  canvas.setTextColor(static_cast<uint8_t>(ink));
  canvas.drawString(value, x, y);
}

void pitchName(uint8_t midi, char* out, size_t capacity) {
  static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  snprintf(out, capacity, "%s%d", names[midi % 12], midi / 12 - 1);
}

void header(const ScreenState& s) {
  canvas.fillRect(0, 0, 45, 20, Acid);
  label(5, 2, "S8N", Back, 2);
  label(53, 6, s.helpLines[0] ? "KEY GUIDE" : pageNames[static_cast<uint8_t>(s.page)], Text);
  char bpm[12];
  snprintf(bpm, sizeof bpm, "%u BPM", unsigned(s.bpm));
  label(140, 6, bpm, Text);
  canvas.fillRect(193, 5, 1, 10, Edge);
  if (s.armed || s.capturing || s.takeRecording) {
    canvas.fillCircle(205, 10, 4, Pink);
  } else {
    canvas.drawCircle(205, 10, 3, Edge);
  }
  if (s.running) canvas.fillTriangle(220, 4, 220, 15, 230, 10, Acid);
  else canvas.fillRect(221, 6, 8, 8, Quiet);
  canvas.fillRect(0, 20, 240, 1, Edge);
}

void live(const ScreenState& s) {
  char buf[52];
  snprintf(buf, sizeof buf, "PATCH %03u   /   CH 01", unsigned(s.patch + 1));
  label(5, 26, s.drums ? "KIT 01   /   CH 10" : buf, Quiet);
  label(5, 38, s.drums ? "Standard Drums" : (s.patchName ? s.patchName : "GM instrument"), Text, 2);
  snprintf(buf, sizeof buf, "OCT %u    VEL %u    VOL %u    REV %u",
           unsigned(s.octave), unsigned(s.velocity), unsigned(s.volume), unsigned(s.reverb));
  label(5, 58, buf, Quiet);
  constexpr const char* keys = "AWSEDFTGYHUJK";
  if (s.drums) {
    static const char* names[] = {"BD","RIM","SN","CLP","CH","OH","PH","LFT","HFT","LT","CR","RD","CB"};
    for (int i = 0; i < 13; ++i) {
      const int x = 5 + (i % 7) * 33, y = 71 + (i / 7) * 23;
      const bool down = (s.held >> i) & 1;
      canvas.fillRect(x, y, 30, 20, down ? Pink : GridA);
      snprintf(buf, sizeof buf, "%c", keys[i]);
      label(x + 3, y + 2, buf, down ? Back : Acid);
      label(x + 9, y + 10, names[i], down ? Back : Text);
    }
    return;
  }
  static const uint8_t whiteNotes[] = {0, 2, 4, 5, 7, 9, 11, 12};
  for (int i = 0; i < 8; ++i) {
    const int note = whiteNotes[i], x = 5 + 29 * i;
    const bool down = (s.held >> note) & 1;
    canvas.fillRect(x, 72, 27, 44, down ? Acid : Text);
    snprintf(buf, sizeof buf, "%c", keys[note]);
    label(x + 10, 104, buf, Back);
  }
  // Black keys straddle the adjacent white keys.
  for (int i = 0; i < 7; ++i) {
    const int note = whiteNotes[i] + 1;
    if (note == whiteNotes[i + 1]) continue;
    const int x = 24 + 29 * i;
    const bool down = (s.held >> note) & 1;
    canvas.fillRect(x, 72, 17, 26, down ? Pink : GridA);
    snprintf(buf, sizeof buf, "%c", keys[note]);
    label(x + 6, 82, buf, down ? Back : Text);
  }
}

int cellX(int step) { return 38 + 12 * step + 2 * (step / 4); }

void grid(const ScreenState& s) {
  char buf[40];
  snprintf(buf, sizeof buf, "PAT %c", 'A' + s.pattern);
  label(4, 24, buf, Acid);
  for (int beat = 0; beat < 4; ++beat) {
    const int x = cellX(4 * beat);
    snprintf(buf, sizeof buf, "%d", beat + 1);
    label(x, 24, buf, Quiet);
    canvas.fillRect(x + 9, 28, 36, 1, GridA);
  }
  if (s.running && s.playhead < 16) {
    const int x = cellX(s.playhead) + 4;
    canvas.fillTriangle(x - 3, 29, x + 3, 29, x, 32, Pink);
  }
  for (int lane = 0; lane < 8; ++lane) {
    const int y = 34 + 9 * lane;
    const bool selected = s.track == lane;
    const bool muted = (s.muteMask >> lane) & 1;
    if (selected) canvas.fillRect(0, y - 1, 35, 9, Acid);
    label(3, y - 1, lanes[lane], selected ? Back : (muted ? Edge : Quiet));
    if (muted) canvas.fillRect(2, y + 2, 31, 1, selected ? Back : Quiet);
    for (int step = 0; step < 16; ++step) {
      const bool active = (s.steps[lane] >> step) & 1;
      const bool accent = (s.accents[lane] >> step) & 1;
      const int x = cellX(step);
      Ink ink = step / 4 % 2 == 0 ? GridA : GridB;
      if (active) ink = muted ? Edge : (accent ? Acid : Moss);
      canvas.fillRect(x, y, 10, 6, ink);
      if (active && !muted) canvas.fillRect(x, y, 2, 6, accent ? Text : Acid);
      if (s.running && step == s.playhead) canvas.fillRect(x + 4, y, 2, 6, Pink);
      if (selected && s.step == step) canvas.drawRect(x - 1, y - 1, 12, 8, Text);
    }
  }
  canvas.fillRect(0, 107, 240, 14, Surface);
  char pitch[6];
  pitchName(s.note, pitch, sizeof pitch);
  snprintf(buf, sizeof buf, "%02u/16   %-3s   VEL %03u   GATE %u%%",
           unsigned(s.step + 1), pitch, unsigned(s.velocity), unsigned(s.gate));
  label(5, 110, buf, Text);
}

void control(int x, int y, const char* name, uint8_t value, uint8_t maximum, bool selected) {
  constexpr int width = 111;
  canvas.fillRect(x, y, width, 25, Surface);
  if (selected) canvas.fillRect(x, y, 2, 25, Acid);
  label(x + 6, y + 4, name, selected ? Acid : Quiet);
  char buf[6];
  snprintf(buf, sizeof buf, "%3u", unsigned(value));
  label(x + 86, y + 4, buf, Text);
  canvas.fillRect(x + 6, y + 17, 98, 3, GridB);
  const int fill = 98 * value / maximum;
  if (fill > 0) canvas.fillRect(x + 6, y + 17, fill, 3, selected ? Acid : Aqua);
}

void sound(const ScreenState& s) {
  const bool drum = s.track < 3;
  const bool sampler = s.track >= 6;
  const int lane = s.track & 7;
  char buf[40];
  snprintf(buf, sizeof buf, "%s  /  %s", lanes[lane], drum ? "DRUMS" : (sampler ? "SAMPLER" : "UNIT SYNTH"));
  label(5, 26, buf, Quiet);
  if (drum) {
    char pitch[6];
    pitchName(s.note, pitch, sizeof pitch);
    snprintf(buf, sizeof buf, "Drum %u / %s", unsigned(s.note), pitch);
  } else if (sampler) {
    snprintf(buf, sizeof buf, "Sample slot %u", unsigned(s.laneSlot + 1));
  } else {
    snprintf(buf, sizeof buf, "%s", s.patchName ? s.patchName : "GM instrument");
  }
  label(4, 38, buf, Text, 2);
  if (drum || sampler) {
    control(5, 62, drum ? "NOTE" : "SLOT", drum ? s.note : s.laneSlot + 1, drum ? 127 : 4, s.soundField == 0);
    control(124, 62, "LEVEL", s.volume, 127, s.soundField == 1);
    label(6, 101, drum ? "OUTPUT  /  UNIT DRUM KIT" : "OUTPUT  /  CARDPUTER SPEAKER", Quiet);
  } else {
    control(5, 62, "PATCH", s.patch + 1, 128, s.soundField == 0);
    control(124, 62, "CUTOFF", s.cutoff, 127, s.soundField == 1);
    control(5, 91, "REVERB", s.reverb, 127, s.soundField == 2);
    control(124, 91, "LEVEL", s.volume, 127, s.soundField == 3);
  }
}

void sample(const ScreenState& s) {
  char buf[40];
  label(5, 29, "SLOT", Quiet);
  for (int i = 0; i < 4; ++i) {
    const int x = 37 + 20 * i;
    canvas.fillRect(x, 25, 16, 14, i == s.slot ? Acid : Surface);
    snprintf(buf, sizeof buf, "%d", i + 1);
    label(x + 5, 28, buf, i == s.slot ? Back : Quiet);
  }
  const uint16_t ms = s.capturing ? s.captureMs : s.sampleMs;
  snprintf(buf, sizeof buf, "%u.%03u SEC", unsigned(ms / 1000), unsigned(ms % 1000));
  label(175, 29, buf, Text);
  canvas.fillRect(5, 45, 230, 53, Surface);
  for (int tick = 0; tick <= 8; ++tick) canvas.fillRect(9 + tick * 27, 49, 1, 45, GridA);
  canvas.fillRect(9, 71, 221, 1, Edge);
  for (int i = 0; i < 64; ++i) {
    const int peak = 21 * (s.wave[i] & 127) / 127;
    if (peak) canvas.fillRect(10 + i * 220 / 64, 71 - peak, 3, peak * 2 + 1, s.capturing ? Pink : Aqua);
  }
  if (!s.capturing && !s.sampleMs) {
    canvas.fillRect(73, 65, 94, 13, Surface);
    label(78, 68, "EMPTY SAMPLE", Quiet);
  }
  label(5, 106, s.capturing ? "CAPTURING" : "PCM / MONO", s.capturing ? Pink : Quiet);
  label(180, 106, "16 kHz", Quiet);
}

void project(const ScreenState& s) {
  char buf[42];
  label(5, 27, "PROJECT SLOT", Quiet);
  snprintf(buf, sizeof buf, "%02u", unsigned(s.projectSlot + 1));
  label(5, 39, buf, Text, 2);
  for (int i = 0; i < 8; ++i) {
    int x = 44 + i * 14;
    canvas.fillRect(x, 46, 10, 5, i == s.projectSlot ? Acid : GridB);
  }
  label(180, 43, s.sdReady ? "SD READY" : "NO CARD", s.sdReady ? Aqua : Amber);
  canvas.fillRect(5, 62, 230, 21, Surface);
  snprintf(buf, sizeof buf, "PAT %c    %u BPM    SWING %u%%", 'A' + s.pattern, unsigned(s.bpm), unsigned(s.swing));
  label(11, 69, buf, Text);
  if (s.takeRecording) {
    canvas.fillCircle(10, 99, 3, Pink);
    const unsigned long secs = static_cast<unsigned long>(s.takeSeconds);
    snprintf(buf, sizeof buf, "MIC TAKE   %02lu:%02lu", secs / 60, secs % 60);
    label(20, 96, buf, Pink);
    label(5, 108, "R Stop and save", Text);
  } else if (s.takePlaying) {
    label(5, 92, "PLAYING MIC TAKE", Aqua);
    label(5, 105, "OUTPUT  /  CARDPUTER SPEAKER", Quiet);
  } else {
    label(5, 92, "MIC TAKE", Quiet);
    label(5, 105, "R Record mic    P Play latest", Text);
  }
}

void help(const ScreenState& s) {
  const unsigned page = static_cast<unsigned>(s.page);
  label(5, 24, pageNames[page], Acid);
  for (int i = 0; i < 8; ++i) {
    if (s.helpLines[i]) label(5, 37 + i * 10, s.helpLines[i], i % 2 ? Quiet : Text);
  }
}

// The standard Cardputer has no PSRAM. If the sprite cannot be allocated,
// keep all pages and controls usable and repaint only changed text rows.
void drawDirect(const ScreenState& s) {
  static char previous[10][40] = {};
  static Ink previousInk[10] = {};
  char rows[10][40] = {};
  Ink inks[] = {Acid, Quiet, Text, Text, Text, Text, Quiet, Quiet, Quiet, Quiet};
  const auto page = static_cast<unsigned>(s.page);
  snprintf(rows[0], sizeof rows[0], "S8N  /  %s", s.helpLines[0] ? "KEY GUIDE" : pageNames[page]);
  if (s.helpLines[0]) {
    for (int i = 0; i < 8; ++i)
      snprintf(rows[i + 1], sizeof rows[0], "%s", s.helpLines[i] ? s.helpLines[i] : "");
    snprintf(rows[9], sizeof rows[9], "%s", s.page == Page::Live ? "ENTER Close" : "? Close");
  } else {
    snprintf(rows[1], sizeof rows[1], "%u BPM  /  %s%s", unsigned(s.bpm),
             s.running ? "PLAY" : "STOP", s.armed ? "  /  ARMED" : "");
    switch (s.page) {
      case Page::Live:
        snprintf(rows[3], sizeof rows[3], "%s", s.drums ? "Standard Drums" : (s.patchName ? s.patchName : "GM instrument"));
        snprintf(rows[4], sizeof rows[4], "OCT %u    VEL %u    VOL %u",
                 unsigned(s.octave), unsigned(s.velocity), unsigned(s.volume));
        snprintf(rows[6], sizeof rows[6], "A W S E D F T G Y H U J K");
        for (int i = 0; i < 13; ++i) {
          rows[7][i * 2] = (s.held & (1u << i)) ? '^' : '.';
          rows[7][i * 2 + 1] = ' ';
        }
        break;
      case Page::Grid:
        snprintf(rows[2], sizeof rows[2], "PAT %c    %s    STEP %02u", 'A' + s.pattern,
                 lanes[s.track & 7], unsigned(s.step + 1));
        for (int i = 0; i < 16; ++i) {
          rows[4][i * 2] = (s.steps[s.track & 7] & (1u << i)) ? 'X' : '.';
          rows[4][i * 2 + 1] = rows[5][i * 2 + 1] = ' ';
          rows[5][i * 2] = i == s.step ? '^' : (s.running && s.playhead == i ? '|' : ' ');
        }
        snprintf(rows[7], sizeof rows[7], "NOTE %u    VEL %u    GATE %u%%",
                 unsigned(s.note), unsigned(s.velocity), unsigned(s.gate));
        break;
      case Page::Sound: {
        snprintf(rows[2], sizeof rows[2], "%s", lanes[s.track & 7]);
        const bool melodic = s.track >= 3 && s.track < 6;
        if (melodic) {
          snprintf(rows[3], sizeof rows[3], "%s", s.patchName ? s.patchName : "GM instrument");
          snprintf(rows[4], sizeof rows[4], "PATCH %u", unsigned(s.patch + 1));
          snprintf(rows[5], sizeof rows[5], "CUTOFF %u", unsigned(s.cutoff));
          snprintf(rows[6], sizeof rows[6], "REVERB %u", unsigned(s.reverb));
          snprintf(rows[7], sizeof rows[7], "LEVEL %u", unsigned(s.volume));
        } else {
          if (s.track < 3) snprintf(rows[4], sizeof rows[4], "NOTE %u", unsigned(s.note));
          else snprintf(rows[4], sizeof rows[4], "SLOT %u", unsigned(s.laneSlot + 1));
          snprintf(rows[5], sizeof rows[5], "LEVEL %u", unsigned(s.volume));
        }
        inks[4 + (s.soundField % (melodic ? 4 : 2))] = Acid;
        break;
      }
      case Page::Sample:
        snprintf(rows[3], sizeof rows[3], "SAMPLE SLOT %u", unsigned(s.slot + 1));
        snprintf(rows[4], sizeof rows[4], "%s    %u ms", s.capturing ? "CAPTURING" : "PCM 16 kHz",
                 unsigned(s.capturing ? s.captureMs : s.sampleMs));
        break;
      case Page::Project:
        snprintf(rows[3], sizeof rows[3], "PROJECT %02u    %s", unsigned(s.projectSlot + 1),
                 s.sdReady ? "SD READY" : "NO CARD");
        snprintf(rows[4], sizeof rows[4], "PAT %c    SWING %u%%", 'A' + s.pattern, unsigned(s.swing));
        if (s.takeRecording) snprintf(rows[6], sizeof rows[6], "MIC TAKE %lus    R Stop and save", static_cast<unsigned long>(s.takeSeconds));
        else snprintf(rows[6], sizeof rows[6], "%s", s.takePlaying ? "PLAYING TAKE    P Stop" : "R Record mic    P Play latest");
        break;
    }
    const bool notice = s.notice && s.notice[0];
    snprintf(rows[9], sizeof rows[9], "%s", notice ? s.notice : (s.hint ? s.hint : hints[page]));
    if (notice) inks[9] = Amber;
  }
  auto& lcd = M5Cardputer.Display;
  for (int i = 0; i < 10; ++i) {
    if (previousInk[i] == inks[i] && strcmp(rows[i], previous[i]) == 0) continue;
    const int y = i * 13;
    lcd.fillRect(0, y, 240, 13, colors[Back]);
    lcd.setTextColor(colors[inks[i]]);
    lcd.drawString(rows[i], 3, y + 3);
    memcpy(previous[i], rows[i], sizeof rows[i]);
    previousInk[i] = inks[i];
  }
}
} // namespace

bool Screen::begin() {
  canvas.setPsram(false);
  canvas.setColorDepth(4);
  if (!canvas.createSprite(240, 135)) {
    auto& lcd = M5Cardputer.Display;
    lcd.setTextSize(1);
    lcd.fillScreen(colors[Back]);
    return false;
  }
  for (unsigned i = 0; i < sizeof colors / sizeof colors[0]; ++i) canvas.setPaletteColor(i, colors[i]);
  ready = true;
  return true;
}

void Screen::draw(const ScreenState& state) {
  if (!ready) { drawDirect(state); return; }
  canvas.fillScreen(Back);
  header(state);
  if (state.helpLines[0]) {
    help(state);
  } else {
    switch (state.page) {
      case Page::Live: live(state); break;
      case Page::Grid: grid(state); break;
      case Page::Sound: sound(state); break;
      case Page::Sample: sample(state); break;
      case Page::Project: project(state); break;
    }
  }
  canvas.fillRect(0, 121, 240, 1, Edge);
  const char* hint = state.hint ? state.hint : hints[static_cast<uint8_t>(state.page)];
  if (state.helpLines[0]) hint = state.page == Page::Live ? "ENTER CLOSE" : "? CLOSE";
  if (state.notice && state.notice[0]) {
    canvas.fillRect(0, 122, 3, 13, Amber);
    label(6, 125, state.notice, Amber);
  } else {
    label(5, 125, hint, Quiet);
  }
  canvas.pushSprite(&M5Cardputer.Display, 0, 0);
}
