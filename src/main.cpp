#include <Arduino.h>
#include <M5Cardputer.h>
#include "Performance.h"
#include "Patches.h"

// Standard straight-through Grove cable: yellow = GPIO2 -> module RX.
// RX is deliberately disabled: this milestone transmits MIDI only.
HardwareSerial midiPort(1);
class SerialMidi : public s8n::MidiOut {
    void send(uint8_t status, uint8_t a, uint8_t b, uint8_t length) override {
        const uint8_t bytes[] = {status, uint8_t(a & 0x7f), uint8_t(b & 0x7f)};
        midiPort.write(bytes, length);
    }
} midi;
s8n::Performance instrument(midi);
M5Canvas screen(&M5Cardputer.Display);
bool buffered = false, help = false, oldEnter = false, oldPanic = false;
bool previous[128] = {};
uint32_t lastDraw = 0;
constexpr uint16_t green = 0x87e0, crimson = 0xf906, dim = 0x4a69;

void draw() {
    // A sprite prevents flicker; use direct drawing if allocation fails.
    auto render = [](auto& d) {
        d.fillScreen(TFT_BLACK);
        d.setTextSize(2);
        d.setTextColor(green);
        d.setCursor(6, 3); d.print("S8N SYNTH");
        d.setTextSize(1); d.setTextColor(crimson);
        d.setCursor(177, 10); d.print(help ? "CONTROLS" : instrument.drums ? "DRUM / 10" : "PLAY / 01");
        d.drawFastHLine(6, 23, 228, crimson);
        if (help) {
            d.setTextColor(TFT_WHITE);
            d.setCursor(6, 30);
            d.print("1/2 Patch -/+   3/4 Volume -/+\n");
            d.setCursor(6, 43); d.print("5/6 Velocity   7/8 Octave -/+\n");
            d.setCursor(6, 56); d.print("9 Play/Drums   0 Reverb\n");
            d.setCursor(6, 69); d.print("SPACE or G0: silence all notes\n");
            d.setCursor(6, 87); d.print("Notes: A W S E D F T G Y H U J K");
            d.setCursor(6, 102); d.print("C C# D D# E F F# G G# A A# B C");
            d.setTextColor(green); d.setCursor(6, 121); d.print("ENTER: back   Release then replay");
            return;
        }
        d.setTextColor(TFT_WHITE); d.setCursor(6, 31);
        d.printf("%03u  %s", instrument.drums ? 1 : instrument.program + 1,
                 instrument.drums ? "Standard Drums" : patches[instrument.program]);
        d.setTextColor(green); d.setCursor(6, 48);
        d.printf("OCT %u  VEL %03u  VOL %03u", instrument.octave, instrument.velocity, instrument.volume);
        d.setTextColor(dim); d.setCursor(6, 62);
        d.printf("REVERB %02u    UART > UNIT", instrument.reverb);
        for (uint8_t i = 0; i < s8n::keyCount; ++i) {
            int x = 6 + i * 17;
            bool active = instrument.active() & (1u << i);
            d.fillRoundRect(x, 79, 15, 30, 2, active ? crimson : dim);
            d.setTextColor(active ? TFT_WHITE : TFT_BLACK);
            d.setCursor(x + 5, 91); d.write(s8n::pianoKeys[i] - 'a' + 'A');
        }
        d.setTextColor(TFT_WHITE); d.setCursor(6, 120);
        d.print("ENTER Help   SPACE Panic   9 Mode");
    };
    if (buffered) { render(screen); screen.pushSprite(0, 0); }
    else render(M5Cardputer.Display);
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(100);
    screen.setColorDepth(16);
    buffered = screen.createSprite(240, 135) != nullptr;
    midiPort.begin(31250, SERIAL_8N1, -1, 2);
    delay(300); // allow external SAM2695 to finish power-on initialization
    const uint8_t gmReset[] = {0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7};
    midiPort.write(gmReset, sizeof(gmReset));
    midiPort.flush();
    delay(100);
    instrument.silence(0);
    instrument.configure();
    draw();
}

void loop() {
    M5Cardputer.update();
    bool down[128] = {};
    // Read physical unshifted positions; Shift/Caps/Fn cannot strand a note.
    for (const auto& point : M5Cardputer.Keyboard.keyList()) {
        const uint8_t key = M5Cardputer.Keyboard.getKeyValue(point).value_first;
        if (key < 128) down[key] = true;
    }
    uint16_t mask = 0;
    for (uint8_t i = 0; i < s8n::keyCount; ++i)
        if (down[uint8_t(s8n::pianoKeys[i])]) mask |= uint16_t(1u << i);
    auto pressed = [&](char c) { return down[uint8_t(c)] && !previous[uint8_t(c)]; };
    bool enter = M5Cardputer.Keyboard.keysState().enter;
    bool panic = down[' '] || M5Cardputer.BtnA.isPressed();
    if (panic && !oldPanic) instrument.silence(mask);
    if (enter && !oldEnter) { instrument.silence(mask); help = !help; }
    // Block held notes before resuming, without briefly retriggering them.
    if ((!panic && oldPanic) || (!help && enter && !oldEnter)) instrument.silence(mask);
    if (!help && !panic) {
        if (pressed('9')) instrument.toggleDrums(mask);
        if (pressed('7')) instrument.changeOctave(-1, mask);
        if (pressed('8')) instrument.changeOctave(1, mask);
        if (!instrument.drums) {
            if (pressed('1')) instrument.changeProgram(-1, mask);
            if (pressed('2')) instrument.changeProgram(1, mask);
        }
        if (pressed('3')) instrument.changeVolume(-8);
        if (pressed('4')) instrument.changeVolume(8);
        if (pressed('5')) instrument.changeVelocity(-8);
        if (pressed('6')) instrument.changeVelocity(8);
        if (pressed('0')) instrument.cycleReverb();
        instrument.update(mask);
    } else {
        // Keep held/new keys blocked while in help or while panic is held.
        // No repeated MIDI traffic: silence only if any note remains active.
        if (instrument.active()) instrument.silence(mask);
        instrument.update(0);
    }
    for (unsigned i = 0; i < 128; ++i) previous[i] = down[i];
    oldEnter = enter; oldPanic = panic;
    const uint32_t now = millis();
    if (now - lastDraw >= 40) { draw(); lastDraw = now; }
    delay(1);
}
