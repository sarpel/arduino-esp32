#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <memory>
#include <complex>
#include "../core/SystemManager.h"
#include "../config.h"

// Audio processing quality levels
enum class AudioQuality {
    LOW = 0,      // 8kHz, 8-bit, compressed
    MEDIUM = 1,   // 16kHz, 8-bit, light processing
    HIGH = 2,     // 16kHz, 16-bit, full processing
    ULTRA = 3     // 32kHz, 16-bit, maximum quality
};

// Audio processing features
enum class AudioFeature {
    NOISE_REDUCTION = 0,
    AUTOMATIC_GAIN_CONTROL = 1,
    VOICE_ACTIVITY_DETECTION = 2,
    ECHO_CANCELLATION = 3,
    BASS_BOOST = 4,
    TREBLE_ENHANCEMENT = 5,
    DYNAMIC_RANGE_COMPRESSION = 6,
    EQUALIZATION = 7,
    NOISE_GATE = 8,
    COMPRESSION = 9
};

// Audio processing statistics
struct AudioStats {
    uint32_t samples_processed;
    uint32_t noise_reduction_applied;
    uint32_t agc_adjustments;
    uint32_t voice_activity_detected;
    uint32_t silent_frames;
    uint32_t clipping_events;
    uint32_t processing_errors;
    float average_input_level;
    float average_output_level;
    float noise_floor_level;
    float current_gain;
    uint32_t buffer_underruns;
    uint32_t buffer_overruns;
    
    AudioStats() : samples_processed(0), noise_reduction_applied(0), agc_adjustments(0),
                   voice_activity_detected(0), silent_frames(0), clipping_events(0),
                   processing_errors(0), average_input_level(0.0f), average_output_level(0.0f),
                   noise_floor_level(0.0f), current_gain(1.0f), buffer_underruns(0),
                   buffer_overruns(0) {}
};

// Audio configuration
struct AudioConfig {
    AudioQuality quality;
    bool enable_noise_reduction;
    bool enable_agc;
    bool enable_vad;
    bool enable_echo_cancellation;
    bool enable_compression;
    float noise_reduction_level;  // 0.0 to 1.0
    float agc_target_level;       // Target RMS level
    float agc_max_gain;           // Maximum gain multiplier
    float compression_ratio;      // Compression ratio
    float noise_gate_threshold;   // Gate threshold in dB
    uint32_t sample_rate;
    uint8_t bit_depth;
    uint8_t channels;
    
    AudioConfig() : quality(AudioQuality::HIGH), enable_noise_reduction(true),
                    enable_agc(true), enable_vad(true), enable_echo_cancellation(false),
                    enable_compression(false), noise_reduction_level(0.7f),
                    agc_target_level(0.3f), agc_max_gain(10.0f), compression_ratio(4.0f),
                    noise_gate_threshold(-40.0f), sample_rate(16000), bit_depth(16), channels(1) {}
};

// Noise reduction using spectral subtraction
class NoiseReducer {
private:
    static constexpr size_t FFT_SIZE = 256;
    static constexpr size_t OVERLAP = 4;
    
    std::vector<std::complex<float>> fft_buffer;
    std::vector<float> noise_profile;
    std::vector<float> window_function;
    float noise_reduction_level;
    bool noise_profile_initialized;
    
    void initializeWindowFunction();
    void performFFT(std::vector<std::complex<float>>& data);
    void performIFFT(std::vector<std::complex<float>>& data);
    void updateNoiseProfile(const std::vector<float>& spectrum);
    
public:
    NoiseReducer();
    bool initialize(float reduction_level = 0.7f);
    void processAudio(float* samples, size_t count);
    void resetNoiseProfile();
    bool isProfileInitialized() const { return noise_profile_initialized; }
};

// Automatic Gain Control
class AutomaticGainControl {
private:
    float target_level;
    float current_gain;
    float max_gain;
    float attack_rate;
    float release_rate;
    float envelope;
    
    float calculateRMS(const float* samples, size_t count);
    float calculatePeak(const float* samples, size_t count);
    
public:
    AutomaticGainControl();
    bool initialize(float target = 0.3f, float max_gain_val = 10.0f);
    void processAudio(float* samples, size_t count);
    float getCurrentGain() const { return current_gain; }
    void setTargetLevel(float target) { target_level = target; }
    void reset();
};

// Voice Activity Detection
class VoiceActivityDetector {
private:
    static constexpr size_t ENERGY_HISTORY_SIZE = 10;
    
    std::vector<float> energy_history;
    float energy_threshold;
    float noise_floor;
    size_t history_index;
    bool voice_detected;
    uint32_t consecutive_voice_frames;
    uint32_t consecutive_silence_frames;
    
    float calculateEnergy(const float* samples, size_t count);
    void updateNoiseFloor(float energy);
    
public:
    VoiceActivityDetector();
    bool initialize(float threshold = 0.1f);
    bool detectVoiceActivity(const float* samples, size_t count);
    bool isVoiceDetected() const { return voice_detected; }
    float getNoiseFloor() const { return noise_floor; }
    void reset();
};

// Audio buffer management
class AudioBuffer {
private:
    std::vector<float> buffer;
    size_t write_pos;
    size_t read_pos;
    size_t available_samples;
    
public:
    AudioBuffer(size_t size);
    bool write(const float* samples, size_t count);
    bool read(float* samples, size_t count);
    size_t available() const { return available_samples; }
    size_t capacity() const { return buffer.size(); }
    void clear();
    bool isEmpty() const { return available_samples == 0; }
    bool isFull() const { return available_samples >= buffer.size(); }
};

class AudioProcessor {
private:
    // Configuration
    AudioConfig config;
    bool initialized;
    bool safe_mode;
    
    // Processing components
    std::unique_ptr<NoiseReducer> noise_reducer;
    std::unique_ptr<AutomaticGainControl> agc;
    std::unique_ptr<VoiceActivityDetector> vad;
    std::unique_ptr<AudioBuffer> input_buffer;
    std::unique_ptr<AudioBuffer> output_buffer;
    
    // Statistics
    AudioStats stats;
    
    // Processing state
    float* processing_buffer;
    size_t processing_buffer_size;
    bool processing_enabled;
    
    // I2S integration
    bool i2s_initialized;
    uint32_t i2s_errors;
    
    // Internal methods
    bool initializeI2S();
    void processAudioFrame(float* samples, size_t count);
    void applyNoiseReduction(float* samples, size_t count);
    void applyAGC(float* samples, size_t count);
    bool detectVoiceActivity(const float* samples, size_t count);
    void convertToFloat(const uint8_t* input, float* output, size_t count);
    void convertFromFloat(const float* input, uint8_t* output, size_t count);
    void updateStatistics(const float* input, const float* output, size_t count);
    void handleProcessingError(const char* error);
    
public:
    AudioProcessor();
    ~AudioProcessor();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // Configuration
    void setConfig(const AudioConfig& new_config);
    const AudioConfig& getConfig() const { return config; }
    void setQuality(AudioQuality quality);
    void enableFeature(AudioFeature feature, bool enable);
    bool isFeatureEnabled(AudioFeature feature) const;
    
    // Audio processing
    bool readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read);
    bool readDataWithRetry(uint8_t* buffer, size_t buffer_size, size_t* bytes_read, int max_retries = 3);
    bool processAudioData(const uint8_t* input, uint8_t* output, size_t size);
    
    // I2S management
    bool reinitialize();
    bool healthCheck();
    uint32_t getI2SErrorCount() const { return i2s_errors; }
    
    // Statistics
    const AudioStats& getStatistics() const { return stats; }
    void resetStatistics();
    void printStatistics() const;
    
    // Quality assessment
    float getAudioQualityScore() const;
    float getInputLevel() const { return stats.average_input_level; }
    float getOutputLevel() const { return stats.average_output_level; }
    bool isVoiceActive() const;
    
    // Safe mode
    void setSafeMode(bool enable) { safe_mode = enable; }
    bool isSafeMode() const { return safe_mode; }
    
    // Processing control
    void enableProcessing(bool enable) { processing_enabled = enable; }
    bool isProcessingEnabled() const { return processing_enabled; }
    
    // Utility
    static float calculateRMS(const float* samples, size_t count);
    static float calculatePeak(const float* samples, size_t count);
    static float calculateSNR(const float* signal, const float* noise, size_t count);
    static void applyHighPassFilter(float* samples, size_t count, float cutoff_freq, float sample_rate);
    static void applyLowPassFilter(float* samples, size_t count, float cutoff_freq, float sample_rate);
};

#endif // AUDIO_PROCESSOR_H