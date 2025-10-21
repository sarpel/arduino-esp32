#include "Equalizer.h"
#include <cmath>

Equalizer::Equalizer() : current_preset(EQPreset::FLAT) {
    bands.resize(NUM_BANDS);
    for (size_t i = 0; i < NUM_BANDS; i++) {
        bands[i].frequency = BANDS_FREQ[i];
        bands[i].gain_db = 0.0f;
        bands[i].q_factor = 0.707f;
    }
}

bool Equalizer::initialize(EQPreset preset) {
    initializePreset(preset);
    return true;
}

void Equalizer::initializePreset(EQPreset preset) {
    current_preset = preset;
    
    switch (preset) {
        case EQPreset::FLAT:
            for (size_t i = 0; i < NUM_BANDS; i++) {
                bands[i].gain_db = 0.0f;
            }
            break;
            
        case EQPreset::VOICE_ENHANCEMENT:
            bands[0].gain_db = -5.0f;
            bands[1].gain_db = 2.0f;
            bands[2].gain_db = 5.0f;
            bands[3].gain_db = 3.0f;
            bands[4].gain_db = -3.0f;
            break;
            
        case EQPreset::BASS_BOOST:
            bands[0].gain_db = 8.0f;
            bands[1].gain_db = 4.0f;
            bands[2].gain_db = 0.0f;
            bands[3].gain_db = -2.0f;
            bands[4].gain_db = -4.0f;
            break;
            
        case EQPreset::TREBLE_BOOST:
            bands[0].gain_db = -4.0f;
            bands[1].gain_db = -2.0f;
            bands[2].gain_db = 0.0f;
            bands[3].gain_db = 4.0f;
            bands[4].gain_db = 8.0f;
            break;
            
        case EQPreset::CUSTOM:
            break;
    }
}

void Equalizer::applyBiquadFilter(float* samples, size_t count, const BandGain& band) {
    float gain_linear = pow(10.0f, band.gain_db / 20.0f);
    
    for (size_t i = 0; i < count; i++) {
        samples[i] *= gain_linear;
    }
}

void Equalizer::processAudio(float* samples, size_t count) {
    if (!samples) {
        return;
    }
    
    for (size_t i = 0; i < NUM_BANDS; i++) {
        if (fabsf(bands[i].gain_db) > 0.1f) {
            applyBiquadFilter(samples, count, bands[i]);
        }
    }
}

void Equalizer::setPreset(EQPreset preset) {
    initializePreset(preset);
}

void Equalizer::setBandGain(size_t band_index, float gain_db) {
    if (band_index < NUM_BANDS) {
        bands[band_index].gain_db = gain_db;
        current_preset = EQPreset::CUSTOM;
    }
}

float Equalizer::getBandGain(size_t band_index) const {
    if (band_index < NUM_BANDS) {
        return bands[band_index].gain_db;
    }
    return 0.0f;
}
