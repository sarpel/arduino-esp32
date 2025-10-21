#include "NoiseGate.h"
#include <cmath>
#include <algorithm>

NoiseGate::NoiseGate() 
    : threshold_db(-40.0f), attack_time_ms(10.0f), release_time_ms(100.0f),
      current_gain(0.0f), attack_samples(100), release_samples(1000),
      sample_rate(16000), current_state(GateState::CLOSED),
      state_transition_samples(0), gate_activity_count(0) {
}

bool NoiseGate::initialize(float threshold, float attack, float release, uint32_t sr) {
    threshold_db = threshold;
    attack_time_ms = attack;
    release_time_ms = release;
    sample_rate = sr;
    
    attack_samples = static_cast<uint32_t>((attack_time_ms / 1000.0f) * sample_rate);
    release_samples = static_cast<uint32_t>((release_time_ms / 1000.0f) * sample_rate);
    
    attack_samples = std::max(attack_samples, 1u);
    release_samples = std::max(release_samples, 1u);
    
    current_gain = 0.0f;
    current_state = GateState::CLOSED;
    state_transition_samples = 0;
    
    return true;
}

float NoiseGate::calculateLevel(const float* samples, size_t count) {
    float max_level = 0.0f;
    
    for (size_t i = 0; i < count; i++) {
        float abs_sample = fabsf(samples[i]);
        if (abs_sample > max_level) {
            max_level = abs_sample;
        }
    }
    
    return 20.0f * log10f(max_level + 0.00001f);
}

void NoiseGate::updateGateState(float signal_level) {
    GateState new_state = current_state;
    
    if (signal_level > threshold_db) {
        if (current_state == GateState::CLOSED) {
            new_state = GateState::OPENING;
            state_transition_samples = 0;
        } else if (current_state == GateState::CLOSING) {
            new_state = GateState::OPENING;
            state_transition_samples = 0;
        }
    } else {
        if (current_state == GateState::OPEN) {
            new_state = GateState::CLOSING;
            state_transition_samples = 0;
        } else if (current_state == GateState::OPENING) {
            new_state = GateState::CLOSED;
            state_transition_samples = 0;
        }
    }
    
    if (new_state != current_state) {
        current_state = new_state;
        gate_activity_count++;
    }
}

void NoiseGate::processAudio(float* samples, size_t count) {
    if (!samples) {
        return;
    }
    
    float signal_level = calculateLevel(samples, count);
    updateGateState(signal_level);
    
    for (size_t i = 0; i < count; i++) {
        switch (current_state) {
            case GateState::OPEN:
                current_gain = 1.0f;
                samples[i] *= current_gain;
                break;
                
            case GateState::CLOSED:
                current_gain = 0.0f;
                samples[i] *= current_gain;
                break;
                
            case GateState::OPENING:
                current_gain = static_cast<float>(state_transition_samples) / attack_samples;
                current_gain = std::min(current_gain, 1.0f);
                samples[i] *= current_gain;
                state_transition_samples++;
                
                if (state_transition_samples >= attack_samples) {
                    current_state = GateState::OPEN;
                    current_gain = 1.0f;
                }
                break;
                
            case GateState::CLOSING:
                current_gain = 1.0f - (static_cast<float>(state_transition_samples) / release_samples);
                current_gain = std::max(current_gain, 0.0f);
                samples[i] *= current_gain;
                state_transition_samples++;
                
                if (state_transition_samples >= release_samples) {
                    current_state = GateState::CLOSED;
                    current_gain = 0.0f;
                }
                break;
        }
    }
}

void NoiseGate::setThreshold(float threshold_db_val) {
    threshold_db = threshold_db_val;
}

void NoiseGate::setAttackTime(float attack_ms) {
    attack_time_ms = attack_ms;
    attack_samples = static_cast<uint32_t>((attack_time_ms / 1000.0f) * sample_rate);
    attack_samples = std::max(attack_samples, 1u);
}

void NoiseGate::setReleaseTime(float release_ms) {
    release_time_ms = release_ms;
    release_samples = static_cast<uint32_t>((release_time_ms / 1000.0f) * sample_rate);
    release_samples = std::max(release_samples, 1u);
}
