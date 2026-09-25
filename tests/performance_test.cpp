#include <cassert>
#include <cstdio>
#include <vector>
#include "Performance.h"
#include "Patches.h"
struct Event { uint8_t status, a, b, length; };
struct Capture : s8n::MidiOut {
    std::vector<Event> events;
    void send(uint8_t s, uint8_t a, uint8_t b, uint8_t n) override {
        assert(s >= 0x80 && a < 128 && b < 128);
        assert(n == ((s & 0xf0) == 0xc0 ? 2 : 3));
        events.push_back({s,a,b,n});
    }
    void clear() { events.clear(); }
};
int main() {
    Capture midi;
    s8n::Performance p(midi);
    p.configure();
    assert(midi.events[1].status == 0xc0 && midi.events[1].a == 81);
    midi.clear();
    p.update(1); p.update(1); // held notes must not retrigger
    assert(midi.events.size() == 1 && midi.events[0].a == 60);
    p.update(2); // same number held, different physical key
    assert(midi.events.size() == 3);
    assert(midi.events[1].status == 0x80 && midi.events[1].a == 60);
    assert(midi.events[2].status == 0x90 && midi.events[2].a == 61);
    p.update(0);
    assert(midi.events.back().status == 0x80 && midi.events.back().a == 61);
    midi.clear();
    p.update((1<<0) | (1<<4) | (1<<7)); // C/E/G
    assert(midi.events.size() == 3);
    assert(midi.events[0].a == 60 && midi.events[1].a == 64 && midi.events[2].a == 67);
    p.changeOctave(1, (1<<0) | (1<<4) | (1<<7));
    assert(p.active() == 0 && p.octave == 5);
    midi.clear(); p.update((1<<0) | (1<<4) | (1<<7));
    assert(midi.events.empty()); // no restart until key released
    p.update(0); p.update(1);
    assert(midi.events.back().a == 72);
    p.toggleDrums(1); midi.clear(); p.update(1);
    assert(midi.events.empty());
    p.update(0); p.update(1);
    assert(midi.events.back().status == 0x99 && midi.events.back().a == 36);
    p.update(0);
    assert(midi.events.back().status == 0x89 && midi.events.back().a == 36);
    p.update(1); midi.clear(); p.silence(1);
    assert(midi.events.size() == 49); // explicit off + 3 CCs on 16 channels
    assert(midi.events.front().status == 0x89);
    for (unsigned i=0; i<16; ++i) {
        assert(midi.events[1+i*3].status == (0xb0 | i));
        assert(midi.events[1+i*3].a == 64);
        assert(midi.events[2+i*3].a == 120);
        assert(midi.events[3+i*3].a == 123);
    }
    midi.clear(); p.update(1); assert(midi.events.empty());
    p.update(0); p.update(1); assert(midi.events.size() == 1);
    p.silence(0); p.drums = false;
    p.program = 0; p.changeProgram(-1, 0); assert(p.program == 127);
    p.changeProgram(1, 0); assert(p.program == 0 && midi.events.back().length == 2);
    for(int i=0;i<30;++i) { p.changeOctave(1,0); p.changeVelocity(8); p.changeVolume(8); }
    assert(p.octave == 8 && p.velocity == 127 && p.volume == 127);
    midi.clear(); p.update(1<<12); assert(midi.events.back().a == 120);
    p.update(0);
    for(int i=0;i<30;++i) { p.changeOctave(-1,0); p.changeVelocity(-8); p.changeVolume(-8); }
    assert(p.octave == 0 && p.velocity == 1 && p.volume == 0);
    p.reverb = 0; p.cycleReverb(); assert(p.reverb == 24);
    p.cycleReverb(); assert(p.reverb == 64);
    p.cycleReverb(); assert(p.reverb == 0);
    puts("PASS: note transitions, chords, held-key suppression, panic, drums, program wrapping, MIDI bounds");
}
