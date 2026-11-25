#include "Sampler.h"
#include "M5Cardputer.h"
#include "SD.h"
#include <cassert>
#include <cstdio>

int main(){
    Sampler s;
    s.begin(true);
    for(uint8_t i=0;i<4;i++){assert(s.frames(i)>0 && s.trigger(i)); assert(M5Cardputer.Speaker.last==i);}
    assert(!s.trigger(0,0) && !s.trigger(4));
    auto& mic = M5Cardputer.Mic;
    assert(s.startCapture(2)); assert(!s.trigger(1) && !s.load(2));
    for(int i=0;i<3;++i){mic.pump();s.service();}
    s.stopCapture();mic.pump();s.service();
    assert(!s.capturing() && s.frames(2)==4*320);
    auto original = SD.files.at("/S8N/S03.WAV");
    assert(original.size()==44+1280*2 && original[44]==1 && original[46]==2);
    uint8_t waveform[64]={};s.waveform(2,waveform);assert(waveform[63]==5);

    auto &file = SD.files["/S8N/S03.WAV"];
    file.insert(file.begin()+12,{'J','U','N','K',3,0,0,0,6,5,4,0});
    auto n=file.size()-8; for(int i=0;i<4;++i)file[4+i]=uint8_t(n>>(8*i));
    assert(s.load(2)); assert(s.frames(2)==1280);
    file[25]=0;
    assert(!s.load(2) && s.frames(2)==1280 && s.trigger(2));
    file=original;
    SD.badRename="/S8N/S03.TMP";
    assert(!s.save(2)); assert(SD.files.at("/S8N/S03.WAV")==original);
    SD.badRename.clear();
    assert(SD.rename("/S8N/S03.WAV","/S8N/S03.BAK"));
    assert(s.load(2) && SD.exists("/S8N/S03.WAV"));

    assert(s.startCapture(1)); mic.broken=true; mic.pump(); s.service();
    assert(!s.capturing() && s.frames(1)==3200);
    mic.broken=false;assert(s.startCapture(0)); nowMs+=1501; s.service();
    assert(!s.capturing() && s.frames(0)==4000);
    assert(s.startCapture(3));while(mic.dst)mic.pump();s.service();
    assert(s.frames(3)==12000 && SD.files.at("/S8N/S04.WAV").size()==24044);
    s.suspendAudio();assert(!s.trigger(1) && !s.startCapture(2));s.resumeAudio();assert(s.trigger(1));
    Sampler ram;ram.begin(false);assert(ram.startCapture(0));mic.pump();ram.stopCapture();mic.pump();ram.service();
    assert(ram.frames(0)==640 && !ram.save(0) && ram.trigger(0));
    puts("Sampler checks passed: factory voices, callback capture, stop & full length, WAV chunks, staging, restore, failures, suspension, RAM-only.");
}
