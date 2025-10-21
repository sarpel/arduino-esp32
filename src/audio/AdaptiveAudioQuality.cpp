#include "AdaptiveAudioQuality.h"
#include "../utils/EnhancedLogger.h"

AdaptiveAudioQuality::AdaptiveAudioQuality() 
    : network_manager(nullptr), audio_processor(nullptr),
      mode(AdaptiveQualityMode::AUTOMATIC), current_condition(NetworkCondition::EXCELLENT),
      last_adaptation_time(0), adaptation_count(0), condition_change_count(0),
      enabled(true), initialized(false) {
}

AdaptiveAudioQuality::~AdaptiveAudioQuality() {
    shutdown();
}

bool AdaptiveAudioQuality::initialize(NetworkManager* net_mgr, AudioProcessor* audio_proc) {
    if (!net_mgr || !audio_proc) {
        return false;
    }
    
    network_manager = net_mgr;
    audio_processor = audio_proc;
    
    current_profile.target_quality = AudioQuality::QUALITY_HIGH;
    previous_profile = current_profile;
    
    initialized = true;
    return true;
}

void AdaptiveAudioQuality::shutdown() {
    initialized = false;
}

NetworkCondition AdaptiveAudioQuality::assessNetworkCondition() {
    if (!network_manager) {
        return NetworkCondition::EXCELLENT;
    }
    
    const NetworkQuality& quality = network_manager->getNetworkQuality();
    
    int rssi = quality.rssi;
    float packet_loss = quality.packet_loss;
    int latency = quality.latency_ms;
    float bandwidth = quality.bandwidth_kbps;
    
    int condition_score = 0;
    
    if (rssi >= thresholds.rssi_excellent) {
        condition_score += 0;
    } else if (rssi >= thresholds.rssi_good) {
        condition_score += 1;
    } else if (rssi >= thresholds.rssi_fair) {
        condition_score += 2;
    } else {
        condition_score += 3;
    }
    
    if (packet_loss <= thresholds.packet_loss_excellent) {
        condition_score += 0;
    } else if (packet_loss <= thresholds.packet_loss_good) {
        condition_score += 1;
    } else if (packet_loss <= thresholds.packet_loss_fair) {
        condition_score += 2;
    } else {
        condition_score += 3;
    }
    
    if (latency <= thresholds.latency_excellent) {
        condition_score += 0;
    } else if (latency <= thresholds.latency_good) {
        condition_score += 1;
    } else if (latency <= thresholds.latency_fair) {
        condition_score += 2;
    } else {
        condition_score += 3;
    }
    
    if (bandwidth >= thresholds.bandwidth_excellent) {
        condition_score += 0;
    } else if (bandwidth >= thresholds.bandwidth_good) {
        condition_score += 1;
    } else if (bandwidth >= thresholds.bandwidth_fair) {
        condition_score += 2;
    } else {
        condition_score += 3;
    }
    
    int average_score = condition_score / 4;
    
    if (average_score <= 0) {
        return NetworkCondition::EXCELLENT;
    } else if (average_score == 1) {
        return NetworkCondition::GOOD;
    } else if (average_score == 2) {
        return NetworkCondition::FAIR;
    } else if (average_score == 3) {
        return NetworkCondition::POOR;
    } else {
        return NetworkCondition::CRITICAL;
    }
}

AdaptiveQualityProfile AdaptiveAudioQuality::generateProfileForCondition(NetworkCondition condition) {
    AdaptiveQualityProfile profile;
    
    switch (condition) {
        case NetworkCondition::EXCELLENT:
            profile.target_quality = AudioQuality::QUALITY_ULTRA;
            profile.sample_rate = 32000;
            profile.bit_depth = 16;
            profile.compression_ratio = 1.0f;
            profile.enable_noise_reduction = true;
            profile.enable_agc = true;
            profile.enable_vad = true;
            profile.noise_gate_threshold = -40.0f;
            break;
            
        case NetworkCondition::GOOD:
            profile.target_quality = AudioQuality::QUALITY_HIGH;
            profile.sample_rate = 16000;
            profile.bit_depth = 16;
            profile.compression_ratio = 1.0f;
            profile.enable_noise_reduction = true;
            profile.enable_agc = true;
            profile.enable_vad = true;
            profile.noise_gate_threshold = -40.0f;
            break;
            
        case NetworkCondition::FAIR:
            profile.target_quality = AudioQuality::QUALITY_MEDIUM;
            profile.sample_rate = 16000;
            profile.bit_depth = 8;
            profile.compression_ratio = 2.0f;
            profile.enable_noise_reduction = true;
            profile.enable_agc = true;
            profile.enable_vad = true;
            profile.noise_gate_threshold = -30.0f;
            break;
            
        case NetworkCondition::POOR:
            profile.target_quality = AudioQuality::QUALITY_LOW;
            profile.sample_rate = 8000;
            profile.bit_depth = 8;
            profile.compression_ratio = 4.0f;
            profile.enable_noise_reduction = false;
            profile.enable_agc = true;
            profile.enable_vad = true;
            profile.noise_gate_threshold = -20.0f;
            break;
            
        case NetworkCondition::CRITICAL:
            profile.target_quality = AudioQuality::QUALITY_LOW;
            profile.sample_rate = 8000;
            profile.bit_depth = 8;
            profile.compression_ratio = 8.0f;
            profile.enable_noise_reduction = false;
            profile.enable_agc = true;
            profile.enable_vad = false;
            profile.noise_gate_threshold = -10.0f;
            break;
            
        default:
            profile.target_quality = AudioQuality::QUALITY_HIGH;
            break;
    }
    
    return profile;
}

void AdaptiveAudioQuality::applyQualityProfile(const AdaptiveQualityProfile& profile) {
    if (!audio_processor) {
        return;
    }
    
    AudioConfig config = audio_processor->getConfig();
    
    config.quality = profile.target_quality;
    config.sample_rate = profile.sample_rate;
    config.bit_depth = profile.bit_depth;
    config.compression_ratio = profile.compression_ratio;
    config.enable_noise_reduction = profile.enable_noise_reduction;
    config.enable_agc = profile.enable_agc;
    config.enable_vad = profile.enable_vad;
    config.noise_gate_threshold = profile.noise_gate_threshold;
    
    audio_processor->setConfig(config);
}

bool AdaptiveAudioQuality::shouldAdapt() {
    unsigned long current_time = millis();
    return (current_time - last_adaptation_time) >= ADAPTATION_INTERVAL;
}

void AdaptiveAudioQuality::updateStatistics() {
    adaptation_count++;
}

void AdaptiveAudioQuality::update() {
    if (!enabled || !initialized) {
        return;
    }
    
    if (mode == AdaptiveQualityMode::MANUAL) {
        return;
    }
    
    if (!shouldAdapt()) {
        return;
    }
    
    NetworkCondition new_condition = assessNetworkCondition();
    
    if (new_condition != current_condition) {
        condition_change_count++;
        current_condition = new_condition;
    }
    
    AdaptiveQualityProfile new_profile = generateProfileForCondition(current_condition);
    
    if (new_profile.target_quality != current_profile.target_quality ||
        new_profile.sample_rate != current_profile.sample_rate) {
        
        previous_profile = current_profile;
        current_profile = new_profile;
        
        applyQualityProfile(current_profile);
        updateStatistics();
    }
    
    last_adaptation_time = millis();
}

void AdaptiveAudioQuality::forceAdaptation() {
    last_adaptation_time = 0;
    update();
}

const char* AdaptiveAudioQuality::getConditionName(NetworkCondition condition) const {
    switch (condition) {
        case NetworkCondition::EXCELLENT:
            return "EXCELLENT";
        case NetworkCondition::GOOD:
            return "GOOD";
        case NetworkCondition::FAIR:
            return "FAIR";
        case NetworkCondition::POOR:
            return "POOR";
        case NetworkCondition::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

void AdaptiveAudioQuality::printCurrentStatus() const {
    EnhancedLogger& logger = EnhancedLogger::getInstance();
    
    logger.log(LogLevel::LOG_INFO, "=== Adaptive Audio Quality Status ===");
    logger.log(LogLevel::LOG_INFO, "Enabled: %s", enabled ? "Yes" : "No");
    logger.log(LogLevel::LOG_INFO, "Mode: %d", static_cast<int>(mode));
    logger.log(LogLevel::LOG_INFO, "Current Condition: %s", getConditionName(current_condition));
    logger.log(LogLevel::LOG_INFO, "Quality Level: %d", static_cast<int>(current_profile.target_quality));
    logger.log(LogLevel::LOG_INFO, "Sample Rate: %u Hz", current_profile.sample_rate);
    logger.log(LogLevel::LOG_INFO, "Bit Depth: %u bits", current_profile.bit_depth);
    logger.log(LogLevel::LOG_INFO, "Compression Ratio: %.1f:1", current_profile.compression_ratio);
    logger.log(LogLevel::LOG_INFO, "Adaptations: %u", adaptation_count);
    logger.log(LogLevel::LOG_INFO, "Condition Changes: %u", condition_change_count);
}
