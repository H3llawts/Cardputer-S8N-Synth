#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <algorithm>
#include "Performance.h"
#include "Patches.h"
#include "Engine.h"
#include "Sampler.h"
#include "Screen.h"
#include "Storage.h"
#include "Takes.h"

namespace {
HardwareSerial midiPort(1);
class SerialMidi : public s8n::MidiOut {
    void send(uint8_t status, uint8_t a, uint8_t b, uint8_t length) override {
        const uint8_t bytes[] = {status, a, b};
        midiPort.write(bytes,length);
    }
} midi;
s8n::Performance live(midi);
Engine engine;
Sampler sampler;
Screen screen;
Storage storage;
Takes takes;
Page page = Page::Live;
uint8_t stepIndex = 0, field = 0, slot = 0, projectSlot = 0, octave = 4;
uint8_t keyNotes[s8n::keyCount]{};
uint16_t held = 0, blocked = 0;
bool previous[128]{}, help = false, card = false, dirty = true, oldPanic = false;
uint32_t noticeAt = 0, lastDraw = 0;
char notice[56]{};

struct Keys {
    bool down[128]{};
    uint16_t notes = 0;
    bool shift, fn;
    bool edge(uint8_t key) const { return down[key] && !previous[key]; }
    int pair(char minus, char plus) const { return int(edge(plus))-int(edge(minus)); }
};

Keys scan() {
    Keys keys{};
    for (const auto& point : M5Cardputer.Keyboard.keyList()) {
        const uint8_t key = M5Cardputer.Keyboard.getKeyValue(point).value_first;
        if (key < 128) keys.down[key] = true;
    }
    keys.shift = M5Cardputer.Keyboard.keysState().shift;
    keys.fn = M5Cardputer.Keyboard.keysState().fn;
    for (unsigned i=0; i<s8n::keyCount; ++i)
        if (keys.down[(uint8_t)s8n::pianoKeys[i]]) keys.notes |= 1u<<i;
    return keys;
}

int limit(int value, int low, int high) { return std::max(low,std::min(value,high)); }
int wrap(int value, int count) { return (value+count)%count; }
void say(const char* message) {
    snprintf(notice,sizeof(notice),"%s",message);
    noticeAt = millis(); dirty = true;
}
void silenceKeys(uint16_t down) {
    for (unsigned i=0; i<s8n::keyCount; ++i)
        if (held & (1u<<i)) engine.noteOff(keyNotes[i]);
    held = 0; blocked = down;
}
void trigger(uint8_t which, uint8_t velocity) { sampler.trigger(which,velocity); }
Step& cell() { return engine.project.steps[engine.pattern][engine.track][stepIndex]; }

void switchPage(Page dest, uint16_t down) {
    if (page == Page::Live) {
        live.silence(down);
        engine.applyProject();
    }
    if (dest == Page::Live) {
        if (sampler.capturing()) { say("Finish sampler recording first"); return; }
        if (takes.recording()) { say("Stop mic take on Project first"); return; }
        engine.stop(); sampler.stop();
        if (takes.playing()) takes.stop();
        live.silence(down); live.configure();
        const uint8_t neutralCutoff[] = {0xb0,99,1, 0xb0,98,0x20, 0xb0,6,64};
        midiPort.write(neutralCutoff,sizeof(neutralCutoff));
    }
    silenceKeys(down); page = dest; help = false;
    notice[0] = 0; dirty = true;
}

void liveKeys(const Keys& k) {
    if (k.edge('9')) live.toggleDrums(k.notes);
    int d = k.pair('7','8'); if (d) live.changeOctave(d,k.notes);
    d = k.pair('1','2'); if (d && !live.drums) live.changeProgram(d,k.notes);
    d = k.pair('3','4'); if (d) live.changeVolume(8*d);
    d = k.pair('5','6'); if (d) live.changeVelocity(8*d);
    if (k.edge('0')) live.cycleReverb();
    live.update(k.notes);
}

void grooveKeys(const Keys& k) {
    blocked &= k.notes;
    for (unsigned i=0; i<s8n::keyCount; ++i) {
        const unsigned bit = 1u<<i;
        if ((held & bit) && !(k.notes & bit)) {
            engine.noteOff(keyNotes[i]); held &= ~bit;
        }
    }
    for (unsigned i=0; i<s8n::keyCount; ++i) {
        const unsigned bit = 1u<<i;
        if (!(k.notes & bit) || ((held|blocked)&bit)) continue;
        const int base = engine.track < 3 ? Engine::defaultNote(engine.track) : 12*(octave+1);
        const int note = base+i;
        if (note>127) continue;
        keyNotes[i] = note; held |= bit;
        engine.noteOn(note,96,micros());
        if (k.fn && page == Page::Grid && !engine.playing) {
            cell().note = note;
            if (!cell().velocity) cell().velocity = 96;
        }
    }
}

void gridKeys(const Keys& k) {
    int d = k.pair(';','.');
    if (d) {
        silenceKeys(k.notes); engine.track = wrap(engine.track+d,8); field = 0;
    }
    stepIndex = wrap(stepIndex+k.pair(',','/'),16);
    if (k.edge(KEY_ENTER)) cell().velocity = cell().velocity ? 0 : 96;
    if (k.edge('m')) engine.setMute(engine.track,!engine.project.tracks[engine.track].muted);
    for (unsigned i=0; i<4; ++i) if (k.edge('1'+i)) engine.selectPattern(i);
    if (k.edge('r')) {
        engine.recording = !engine.recording;
        say(engine.recording ? (engine.playing ? "Overdub on" : "Armed / SPACE to roll") : "Overdub off");
    }
    cell().note = limit(cell().note+k.pair('[',']'),0,127);
    d = k.pair('5','6');
    if (d) cell().velocity = limit((cell().velocity ? cell().velocity : 96)+d*8,1,127);
    cell().gate = limit(cell().gate+k.pair('7','8')*5,1,100);
    d = k.pair('9','0');
    if (d) {
        silenceKeys(k.notes);
        octave = limit(octave+d,0,8);
        char msg[30]; snprintf(msg,sizeof(msg),"Keyboard octave %u",octave); say(msg);
    }
    engine.project.bpm = limit(engine.project.bpm+k.pair('-','='),40,240);
    d = k.pair('z','x');
    if (d) {
        engine.project.swing = limit(engine.project.swing+d,50,75);
        char msg[30]; snprintf(msg,sizeof(msg),"Swing %u%%",engine.project.swing); say(msg);
    }
    grooveKeys(k);
}

void soundKeys(const Keys& k) {
    auto& t = engine.project.tracks[engine.track];
    bool drum = engine.track < 3, sample = engine.track >= 6;
    field = wrap(field+k.pair(';','.'),drum || sample ? 2 : 4);
    int d = k.pair(',','/');
    if (d) {
        int coarse = k.fn ? 8*d : d;
        if (drum && !field) cell().note = limit(cell().note+coarse,0,127);
        else if (sample && !field) t.sampleSlot = wrap(t.sampleSlot+d,4);
        else if (drum || sample || field==3) t.volume = limit(t.volume+coarse,0,127);
        else if (field==2) t.reverb = limit(t.reverb+coarse,0,127);
        else if (field==1) t.cutoff = limit(t.cutoff+coarse,0,127);
        else t.program = wrap(t.program+coarse,128);
        engine.applyTrack(engine.track);
    }
    grooveKeys(k);
}

void sampleKeys(const Keys& k) {
    if (sampler.capturing()) {
        if (k.edge('r')) sampler.stopCapture();
        return;
    }
    slot = wrap(slot+k.pair(',','/'),Sampler::Slots);
    if (k.edge('r')) {
        if (takes.recording() || takes.playing()) say("Stop Project take first");
        else { sampler.startCapture(slot); say(sampler.status()); }
    } else if (k.edge(KEY_ENTER)) {
        if (!sampler.trigger(slot,120)) say(takes.recording() || takes.playing() ? "Take owns local audio" : sampler.status());
    } else if (k.edge('l') || k.edge('s')) {
        if (takes.recording() || takes.playing()) { say("Stop Project take first"); return; }
        if (k.edge('l')) sampler.load(slot); else sampler.save(slot);
        say(sampler.status());
    }
}

void projectKeys(const Keys& k) {
    projectSlot = wrap(projectSlot+k.pair(',','/'),8);
    if (k.edge('r') || k.edge('p')) {
        if (takes.recording() || takes.playing()) takes.stop();
        else if (k.edge('r')) takes.record(); else takes.playLatest();
        say(takes.status());
        return;
    }
    if (!k.edge('s') && !k.edge('l')) return;
    if (!card) { say("Insert microSD and reboot"); return; }
    if (takes.recording() || takes.playing() || sampler.capturing()) { say("Finish audio operation first"); return; }
    if (k.edge('s')) say(storage.save(projectSlot,engine.project) ? "Project saved" : "Project save failed");
    else if (storage.load(projectSlot,engine.project)) {
        sampler.stop(); engine.applyProject(); engine.selectPattern(0); say("Project loaded");
    } else say("Slot empty or unreadable");
}

void handle(const Keys& k) {
    const bool livePage = page == Page::Live;
    const bool panic = M5Cardputer.BtnA.isPressed() || (livePage && k.down[' ']);
    if (panic || oldPanic) {
        if (panic != oldPanic) {
            if (livePage) live.silence(k.notes);
            else if (panic) {
                engine.stop(); sampler.stop();
                if (takes.playing()) takes.stop();
            }
            silenceKeys(k.notes);
            if (panic) say("Panic / voices released");
        }
        oldPanic = panic;
        return;
    }
    if ((livePage && k.edge(KEY_ENTER)) || (k.shift && k.edge('/'))) {
        if (livePage) live.silence(k.notes); else silenceKeys(k.notes);
        help = !help; dirty = true; return;
    }
    if (help) return;
    if (k.edge(KEY_TAB)) {
        switchPage(page == Page::Project ? Page::Grid : Page(uint8_t(page)+1),k.notes);
        return;
    }
    if (!livePage && k.edge(96)) { switchPage(Page::Live,k.notes); return; }
    if (livePage) { liveKeys(k); return; }
    if (k.edge(' ')) {
        if (engine.playing) { engine.stop(); sampler.stop(); silenceKeys(k.notes); }
        else engine.start(micros());
    }
    switch (page) {
    case Page::Grid: gridKeys(k); break;
    case Page::Sound: soundKeys(k); break;
    case Page::Sample: sampleKeys(k); break;
    case Page::Project: projectKeys(k); break;
    default: break;
    }
}

constexpr const char* guides[][8] = {
    {"A W S E D F T G Y H U J K  play","1/2 patch    3/4 volume","5/6 velocity    7/8 octave",
     "9 keys/drums   0 reverb","SPACE or G0  panic","TAB  enter the groovebox"},
    {", / step    ; . lane    ENTER toggle","1..4 pattern    M mute    R overdub","A W S E D F T G Y H U J K  play",
     "[ ] pitch    5/6 velocity   7/8 gate","9/0 octave   FN+note write stopped","- = tempo    Z/X swing",
     "SPACE run/stop    G0 panic","TAB page    ~ key returns to Live"},
    {"Select your lane on the Grid","; . select field    , / adjust","FN + , / adjust by 8",
     "Piano keys audition the lane","Drum NOTE edits the selected step","SLOT routes a sample lane to pad 1-4",
     "SPACE run/stop    G0 panic","TAB page    ~ key returns to Live"},
    {", / choose one of four sample slots","ENTER audition    R record / finish","Mic replaces the slot, up to 0.75 s",
     "S save WAV    L reload WAV","Mono PCM16 / 16 kHz on microSD","Use Sound to assign a slot to a lane",
     "Short takes auto-save with microSD","TAB page    ~ key returns to Live"},
    {", / project slot 1-8    S save","L recall patterns and track settings","R start / finish microphone take",
     "P play latest take / stop","WAV takes in /S8N/TAKES","Local samples pause during a take",
     "SPACE run/stop    G0 panic","TAB grid    ~ key returns to Live"}
};

void draw() {
    ScreenState s{};
    s.page = page; s.bpm = engine.project.bpm; s.swing = engine.project.swing;
    s.running = engine.playing; s.armed = engine.recording; s.sdReady = card;
    s.pattern = engine.pattern; s.track = engine.track; s.step = stepIndex;
    s.playhead = s.running ? engine.playhead : 255;
    const Track& t = engine.project.tracks[s.track];
    s.note = cell().note; s.velocity = cell().velocity; s.gate = cell().gate;
    s.patch = t.program; s.cutoff = t.cutoff; s.reverb = t.reverb; s.volume = t.volume;
    s.soundField = field; s.slot = slot; s.laneSlot = t.sampleSlot; s.projectSlot = projectSlot;
    if (page == Page::Live) {
        s.held = live.active(); s.drums = live.drums; s.octave = live.octave;
        s.patch = live.program; s.velocity = live.velocity; s.volume = live.volume; s.reverb = live.reverb;
    }
    s.patchName = patches[s.patch];
    for (unsigned i=0; i<8; ++i) {
        if (engine.project.tracks[i].muted) s.muteMask |= 1u<<i;
        for (unsigned j=0; j<16; ++j) {
            const auto v = engine.project.steps[s.pattern][i][j].velocity;
            if (v) s.steps[i] |= 1u<<j;
            if (v>=104) s.accents[i] |= 1u<<j;
        }
    }
    s.capturing = sampler.capturing();
    if (page == Page::Sample) {
        s.sampleMs = sampler.frames(slot)*1000UL / Sampler::Rate;
        s.captureMs = sampler.capturedFrames()*1000UL / Sampler::Rate;
        sampler.waveform(slot,s.wave);
    }
    s.takeRecording = takes.recording(); s.takePlaying = takes.playing(); s.takeSeconds = takes.seconds();
    if (help) memcpy(s.helpLines,guides[uint8_t(page)],sizeof(s.helpLines));
    else if (notice[0] && millis()-noticeAt < 2200) s.notice = notice;
    screen.draw(s);
}
} // namespace

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg,true);
    M5Cardputer.Display.setRotation(1); M5Cardputer.Display.setBrightness(105);
    screen.begin();
    midiPort.begin(31250,SERIAL_8N1,1,2);
    gpio_set_pull_mode(GPIO_NUM_1, GPIO_PULLUP_ONLY);
    SPI.begin(40,39,14,12);
    card = SD.begin(12,SPI,25000000) && SD.cardType() != CARD_NONE;
    sampler.begin(card); takes.begin(sampler,card);
    M5Cardputer.Speaker.setVolume(150);
    delay(300);
    const uint8_t reset[] = {0xf0,0x7e,0x7f,0x09,0x01,0xf7};
    midiPort.write(reset,sizeof(reset)); midiPort.flush(); delay(100);
    engine.begin(midiPort,trigger); live.configure();
    draw();
}

void loop() {
    bool wasSampling = sampler.capturing(), wasTaking = takes.recording() || takes.playing();
    sampler.service(); takes.service();
    if (wasSampling && !sampler.capturing()) say(sampler.status());
    if (wasTaking && !takes.recording() && !takes.playing()) say(takes.status());
    M5Cardputer.update();
    Keys k = scan();
    handle(k);
    if (memcmp(k.down,previous,sizeof(previous))) dirty = true;
    memcpy(previous,k.down,sizeof(previous));
    if (page != Page::Live && !oldPanic) engine.tick(micros());
    else for (int i=0; i<64 && midiPort.available(); ++i) midiPort.read();
    uint32_t now = millis();
    if ((dirty && now-lastDraw>=20) || now-lastDraw>=65) {
        draw(); lastDraw=now; dirty=false;
    }
    delay(1);
}
