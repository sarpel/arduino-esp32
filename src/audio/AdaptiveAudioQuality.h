#ifndef ADAPTIVE_AUDIO_QUALITY_H
#define ADAPTIVE_AUDIO_QUALITY_H

#include <Arduino.h>
#include <memory>
#include "../network/NetworkManager.h"
#include "AudioProcessor.h"

enum class AdaptiveQualityMode {
    MANUAL = 0,
    AUTOMATIC = 1,
    AGGRESSIVE = 2
};

struct QualityAdaptationThresholds {
    int rssi_excellent;     // >= -50 dBm
    int rssi_good;          // >= -67 dBm
    int rssi_fair;          // >= -70 dBm
    int rssi_poor;          // < -70 dBm
    
    float packet_loss_excellent;    // < 1%
    float packet_loss_good;         // < 5%
    float packet_loss_fair;         // < 10%
    float packet_loss_poor;         // >= 10%
    
    int latency_excellent;   // < 50ms
    int latency_good;        // < 100ms
    int latency_fair;        // < 150ms
    int latency_poor;        // >= 150ms
    
    float bandwidth_excellent;      // > 1000 kbps
    float bandwidth_good;           // > 500 kbps
    float bandwidth_fair;           // > 200 kbps
    float bandwidth_poor;           // <= 200 kbps
    
    QualityAdaptationThresholds() : 
        rssi_excellent(-50), rssi_good(-67), rssi_fair(-70),
        packet_loss_excellent(0.01f), packet_loss_good(0.05f), packet_loss_fair(0.1f),
        latency_excellent(50), latency_good(100), latency_fair(150),
        bandwidth_excellent(1000.0f), bandwidth_good(500.0f), 
        bandwidth_fair(200.0f), bandwidth_poor(200.0f) {}
};

enum class NetworkCondition {
    EXCELLENT = 0,
    GOOD = 1,
    FAIR = 2,
    POOR = 3,
    CRITICAL = 4
};

struct AdaptiveQualityProfile {
    AudioQuality target_quality;
    uint32_t sample_rate;
    uint8_t bit_depth;
    float compression_ratio;
    bool enable_noise_reduction;
    bool enable_agc;
    bool enable_vad;
    float noise_gate_threshold;
    
    AdaptiveQualityProfile() : 
        target_quality(AudioQuality::HIGH), sample_rate(16000),
        bit_depth(16), compression_ratio(1.0f),
        enable_noise_reduction(true), enable_agc(true),
        enable_vad(true), noise_gate_threshold(-40.0f) {}
};

class AdaptiveAudioQuality {
private:
    NetworkManager* network_manager;
    AudioProcessor* audio_processor;
    
    AdaptiveQualityMode mode;
    QualityAdaptationThresholds thresholds;
    
    NetworkCondition current_condition;
    AdaptiveQualityProfile current_profile;
    AdaptiveQualityProfile previous_profile;
    
    unsigned long last_adaptation_time;
    static constexpr unsigned long ADAPTATION_INTERVAL = 5000;
    
    uint32_t adaptation_count;
    uint32_t condition_change_count;
    
    bool enabled;
    bool initialized;
    
    NetworkCondition assessNetworkCondition();
    AdaptiveQualityProfile generateProfileForCondition(NetworkCondition condition);
    void applyQualityProfile(const AdaptiveQualityProfile& profile);
    bool shouldAdapt();
    void updateStatistics();
    
public:
    AdaptiveAudioQuality();
    ~AdaptiveAudioQuality();
    
    bool initialize(NetworkManager* net_mgr, AudioProcessor* audio_proc);
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    void setMode(AdaptiveQualityMode new_mode) { mode = new_mode; }
    AdaptiveQualityMode getMode() const { return mode; }
    
    void update();
    void forceAdaptation();
    
    NetworkCondition getCurrentCondition() const { return current_condition; }
    const AdaptiveQualityProfile& getCurrentProfile() const { return current_profile; }
    const AdaptiveQualityProfile& getPreviousProfile() const { return previous_profile; }
    
    void setThresholds(const QualityAdaptationThresholds& new_thresholds) { thresholds = new_thresholds; }
    const QualityAdaptationThresholds& getThresholds() const { return thresholds; }
    
    void enable(bool state) { enabled = state; }
    bool isEnabled() const { return enabled; }
    
    uint32_t getAdaptationCount() const { return adaptation_count; }
    uint32_t getConditionChangeCount() const { return condition_change_count; }
    
    const char* getConditionName(NetworkCondition condition) const;
    void printCurrentStatus() const;
};

#endif
