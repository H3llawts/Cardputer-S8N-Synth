#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

class Sampler {
public:
    static constexpr uint8_t Slots = 4;
    static constexpr uint16_t Rate = 16000;
    static constexpr uint16_t MaxFrames = 12000;

    void begin(bool sdReady);
    void service();
    bool startCapture(uint8_t slot);
    void stopCapture();
    bool capturing() const { return recordingSlot_ < Slots; }
    uint16_t capturedFrames() const { return completed_.load(); }
    bool trigger(uint8_t slot, uint8_t velocity = 100);
    void stop();
    void suspendAudio();
    void resumeAudio();

    bool load(uint8_t slot);
    bool save(uint8_t slot);
    uint16_t frames(uint8_t slot) const { return slot < Slots ? lengths_[slot] : 0; }
    uint8_t peak(uint8_t slot, uint8_t column, uint8_t columns) const;
    void waveform(uint8_t slot, uint8_t out[64]) const;
    const char* status() const { return message_; }

private:
    using Audio = std::unique_ptr<int16_t[]>;
    Audio sounds_[Slots];
    uint16_t lengths_[Slots] = {};
    Audio scratch_;
    uint8_t recordingSlot_ = Slots;
    bool sdReady_ = false;
    bool suspended_ = false;
    const char* message_ = "Ready";
    std::atomic<uint16_t> completed_{0};
    std::atomic<bool> stopRequested_{false}, captureDone_{false}, captureError_{false};
    uint16_t lastProgress_ = 0;
    uint32_t lastProgressMs_ = 0;

    void factorySound(uint8_t slot);
    bool fail(const char* message);
    void releaseMic();
    static void released(void* context, void* data, size_t length);
};
