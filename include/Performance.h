#pragma once
#include <stdint.h>

#include <initializer_list>
namespace s8n {
constexpr char pianoKeys[] = "awsedftgyhujk";
constexpr uint8_t keyCount = 13;
constexpr uint8_t drumNotes[keyCount] = {36, 37, 38, 39, 42, 46, 44, 41, 43, 45, 49, 51, 56};

struct MidiOut {
    virtual ~MidiOut() = default;
    virtual void send(uint8_t status, uint8_t a, uint8_t b, uint8_t length = 3) = 0;
};

// Pure state machine: hardware scanning and drawing stay outside the note engine.
class Performance {
public:
    explicit Performance(MidiOut& out) : out_(out) {}
    uint8_t octave = 4;  // C4 = MIDI 60 (scientific pitch notation)
    uint8_t program = 81; // zero-based GM saw lead
    uint8_t velocity = 96;
    uint8_t volume = 80;
    uint8_t reverb = 24;
    bool drums = false;

    void configure() {
        for (uint8_t ch : {uint8_t(0), uint8_t(9)}) {
            cc(ch, 0, 0);
            out_.send(0xc0 | ch, ch == 9 ? 0 : program, 0, 2);
            cc(ch, 7, volume);
            cc(ch, 11, 127);
            cc(ch, 10, 64);
            cc(ch, 91, reverb);
        }
    }
    void update(uint16_t down) {
        // Release first, even when one held key is exchanged for another.
        blocked_ &= down;
        for (uint8_t i = 0; i < keyCount; ++i) {
            const auto bit = uint16_t(1u << i);
            if ((active_ & bit) && !(down & bit)) {
                out_.send(0x80 | channels_[i], notes_[i], 0);
                active_ &= ~bit;
            }
        }
        for (uint8_t i = 0; i < keyCount; ++i) {
            const auto bit = uint16_t(1u << i);
            if ((down & bit) && !(active_ & bit) && !(blocked_ & bit)) {
                notes_[i] = drums ? drumNotes[i] : uint8_t((octave + 1) * 12 + i);
                channels_[i] = drums ? 9 : 0;
                out_.send(0x90 | channels_[i], notes_[i], velocity);
                active_ |= bit;
            }
        }
    }
    void silence(uint16_t down) {
        for (uint8_t i = 0; i < keyCount; ++i)
            if (active_ & (1u << i)) out_.send(0x80 | channels_[i], notes_[i], 0);
        // Sustain off + all sound off + all notes off on every MIDI channel.
        for (uint8_t ch = 0; ch < 16; ++ch) {
            cc(ch, 64, 0); cc(ch, 120, 0); cc(ch, 123, 0);
        }
        active_ = 0;
        blocked_ = down; // require release before retrigger after panic/mode changes
    }
    void changeOctave(int delta, uint16_t down) {
        const int next = int(octave) + delta;
        if (next < 0 || next > 8) return;
        silence(down); octave = uint8_t(next);
    }
    void changeProgram(int delta, uint16_t down) {
        silence(down);
        program = uint8_t((int(program) + delta + 128) % 128);
        out_.send(0xc0, program, 0, 2);
    }
    void toggleDrums(uint16_t down) { silence(down); drums = !drums; }
    void changeVolume(int delta) {
        volume = clamp(int(volume) + delta, 0, 127);
        cc(0, 7, volume); cc(9, 7, volume);
    }
    void changeVelocity(int delta) { velocity = clamp(int(velocity) + delta, 1, 127); }
    void cycleReverb() {
        reverb = reverb == 0 ? 24 : reverb == 24 ? 64 : 0;
        cc(0, 91, reverb); cc(9, 91, reverb);
    }
    uint16_t active() const { return active_; }
private:
    MidiOut& out_;
    uint16_t active_ = 0, blocked_ = 0;
    uint8_t notes_[keyCount] = {}, channels_[keyCount] = {};
    void cc(uint8_t ch, uint8_t control, uint8_t value) { out_.send(0xb0 | ch, control, value); }
    static uint8_t clamp(int n, int lo, int hi) { return uint8_t(n < lo ? lo : n > hi ? hi : n); }
};
} // namespace s8n
