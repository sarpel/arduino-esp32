#ifndef EQUALIZER_H
#define EQUALIZER_H

#include <Arduino.h>
#include <vector>

enum class EQPreset {
    FLAT = 0,
    VOICE_ENHANCEMENT = 1,
    BASS_BOOST = 2,
    TREBLE_BOOST = 3,
    CUSTOM = 4
};

struct BandGain {
    float frequency;
    float gain_db;
    float q_factor;
};

class Equalizer {
private:
    static constexpr size_t NUM_BANDS = 5;
    std::vector<BandGain> bands;
    EQPreset current_preset;
    
    static constexpr float BANDS_FREQ[NUM_BANDS] = {100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f};
    
    void applyBiquadFilter(float* samples, size_t count, const BandGain& band);
    void initializePreset(EQPreset preset);
    
public:
    Equalizer();
    bool initialize(EQPreset preset = EQPreset::FLAT);
    
    void processAudio(float* samples, size_t count);
    void setPreset(EQPreset preset);
    void setBandGain(size_t band_index, float gain_db);
    float getBandGain(size_t band_index) const;
    
    EQPreset getCurrentPreset() const { return current_preset; }
    size_t getNumBands() const { return NUM_BANDS; }
    float getBandFrequency(size_t index) const { return index < NUM_BANDS ? BANDS_FREQ[index] : 0.0f; }
};

#endif
