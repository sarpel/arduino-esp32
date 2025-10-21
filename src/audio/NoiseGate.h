#ifndef NOISE_GATE_H
#define NOISE_GATE_H

#include <Arduino.h>
#include <vector>

class NoiseGate {
private:
    float threshold_db;
    float attack_time_ms;
    float release_time_ms;
    float current_gain;
    
    uint32_t attack_samples;
    uint32_t release_samples;
    uint32_t sample_rate;
    
    enum class GateState {
        CLOSED,
        OPENING,
        OPEN,
        CLOSING
    };
    
    GateState current_state;
    uint32_t state_transition_samples;
    uint32_t gate_activity_count;
    
    float calculateLevel(const float* samples, size_t count);
    void updateGateState(float signal_level);
    
public:
    NoiseGate();
    bool initialize(float threshold = -40.0f, float attack = 10.0f, float release = 100.0f, uint32_t sample_rate = 16000);
    
    void processAudio(float* samples, size_t count);
    void setThreshold(float threshold_db);
    void setAttackTime(float attack_ms);
    void setReleaseTime(float release_ms);
    
    float getThreshold() const { return threshold_db; }
    float getAttackTime() const { return attack_time_ms; }
    float getReleaseTime() const { return release_time_ms; }
    float getCurrentGain() const { return current_gain; }
    bool isGateOpen() const { return current_state == GateState::OPEN || current_state == GateState::OPENING; }
    
    uint32_t getGateActivityCount() const { return gate_activity_count; }
};

#endif
