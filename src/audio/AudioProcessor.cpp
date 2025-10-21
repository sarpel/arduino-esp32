#include "AudioProcessor.h"
#include "../core/SystemManager.h"
#include "../i2s_audio.h"
#include <cmath>
#include <algorithm>

// NoiseReducer implementation
NoiseReducer::NoiseReducer() 
    : noise_reduction_level(0.7f), noise_profile_initialized(false) {
    fft_buffer.resize(FFT_SIZE);
    noise_profile.resize(FFT_SIZE / 2 + 1);
    window_function.resize(FFT_SIZE);
}

bool NoiseReducer::initialize(float reduction_level) {
    noise_reduction_level = std::max(0.0f, std::min(1.0f, reduction_level));
    initializeWindowFunction();
    resetNoiseProfile();
    return true;
}

void NoiseReducer::initializeWindowFunction() {
    // Hann window
    for (size_t i = 0; i < FFT_SIZE; i++) {
        window_function[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }
}

void NoiseReducer::resetNoiseProfile() {
    std::fill(noise_profile.begin(), noise_profile.end(), 0.0f);
    noise_profile_initialized = false;
}

void NoiseReducer::performFFT(std::vector<std::complex<float>>& data) {
    // Simple FFT implementation for demonstration
    // In production, use a proper FFT library like KissFFT or ARM CMSIS
    size_t n = data.size();
    if (n <= 1) return;
    
    // Bit reversal
    size_t j = 0;
    for (size_t i = 1; i < n; i++) {
        size_t bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    
    // Cooley-Tukey FFT
    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = 2 * M_PI / len;
        std::complex<float> wlen(cosf(ang), sinf(ang));
        
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; j++) {
                std::complex<float> u = data[i + j];
                std::complex<float> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void NoiseReducer::performIFFT(std::vector<std::complex<float>>& data) {
    // Conjugate, FFT, conjugate, scale
    for (auto& sample : data) {
        sample = std::conj(sample);
    }
    performFFT(data);
    for (auto& sample : data) {
        sample = std::conj(sample) / static_cast<float>(data.size());
    }
}

void NoiseReducer::updateNoiseProfile(const std::vector<float>& spectrum) {
    if (!noise_profile_initialized) {
        noise_profile = spectrum;
        noise_profile_initialized = true;
    } else {
        // Update noise profile with exponential smoothing
        for (size_t i = 0; i < noise_profile.size(); i++) {
            noise_profile[i] = 0.9f * noise_profile[i] + 0.1f * spectrum[i];
        }
    }
}

void NoiseReducer::processAudio(float* samples, size_t count) {
    if (!noise_profile_initialized) {
        // Initialize noise profile with first frame
        std::vector<float> spectrum(FFT_SIZE / 2 + 1);
        for (size_t i = 0; i < count && i < FFT_SIZE / 2 + 1; i++) {
            spectrum[i] = std::abs(samples[i * 2]);
        }
        updateNoiseProfile(spectrum);
        return;
    }
    
    // Process in overlapping windows
    for (size_t i = 0; i < count; i += FFT_SIZE / OVERLAP) {
        size_t window_size = std::min(FFT_SIZE, count - i);
        
        // Apply window and prepare FFT buffer
        for (size_t j = 0; j < window_size; j++) {
            fft_buffer[j] = samples[i + j] * window_function[j];
        }
        for (size_t j = window_size; j < FFT_SIZE; j++) {
            fft_buffer[j] = 0.0f;
        }
        
        // Perform FFT
        performFFT(fft_buffer);
        
        // Apply spectral subtraction
        for (size_t j = 0; j < FFT_SIZE / 2 + 1; j++) {
            float magnitude = std::abs(fft_buffer[j]);
            float noise_magnitude = noise_profile[j];
            
            // Spectral subtraction
            float clean_magnitude = magnitude - noise_reduction_level * noise_magnitude;
            if (clean_magnitude < 0) clean_magnitude = 0;
            
            // Preserve phase
            float phase = std::arg(fft_buffer[j]);
            fft_buffer[j] = std::polar(clean_magnitude, phase);
        }
        
        // Mirror for full spectrum
        for (size_t j = FFT_SIZE / 2 + 1; j < FFT_SIZE; j++) {
            fft_buffer[j] = std::conj(fft_buffer[FFT_SIZE - j]);
        }
        
        // Perform inverse FFT
        performIFFT(fft_buffer);
        
        // Apply window and overlap-add
        for (size_t j = 0; j < window_size; j++) {
            samples[i + j] = std::real(fft_buffer[j]) * window_function[j];
        }
    }
}

// AutomaticGainControl implementation
AutomaticGainControl::AutomaticGainControl()
    : target_level(0.3f), current_gain(1.0f), max_gain(10.0f),
      attack_rate(0.01f), release_rate(0.001f), envelope(0.0f) {}

bool AutomaticGainControl::initialize(float target, float max_gain_val) {
    target_level = std::max(0.01f, std::min(1.0f, target));
    max_gain = std::max(1.0f, std::min(20.0f, max_gain_val));
    current_gain = 1.0f;
    envelope = 0.0f;
    return true;
}

float AutomaticGainControl::calculateRMS(const float* samples, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += samples[i] * samples[i];
    }
    return sqrtf(sum / count);
}

float AutomaticGainControl::calculatePeak(const float* samples, size_t count) {
    float peak = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float abs_sample = std::abs(samples[i]);
        if (abs_sample > peak) peak = abs_sample;
    }
    return peak;
}

void AutomaticGainControl::processAudio(float* samples, size_t count) {
    // Calculate input level
    float input_rms = calculateRMS(samples, count);
    float input_peak = calculatePeak(samples, count);
    
    // Update envelope detector
    float target_envelope = std::max(input_rms, input_peak * 0.5f);
    if (target_envelope > envelope) {
        envelope += attack_rate * (target_envelope - envelope);
    } else {
        envelope += release_rate * (target_envelope - envelope);
    }
    
    // Calculate desired gain
    float desired_gain = target_level / (envelope + 0.001f);  // Avoid division by zero
    desired_gain = std::min(desired_gain, max_gain);
    
    // Smooth gain changes
    float gain_diff = desired_gain - current_gain;
    current_gain += release_rate * gain_diff;
    
    // Apply gain
    for (size_t i = 0; i < count; i++) {
        samples[i] *= current_gain;
        
        // Soft clipping
        if (samples[i] > 0.95f) {
            samples[i] = 0.95f + 0.05f * tanhf((samples[i] - 0.95f) / 0.05f);
        } else if (samples[i] < -0.95f) {
            samples[i] = -0.95f - 0.05f * tanhf((-samples[i] - 0.95f) / 0.05f);
        }
    }
}

void AutomaticGainControl::reset() {
    current_gain = 1.0f;
    envelope = 0.0f;
}

void AutomaticGainControl::setTargetLevel(float target) {
    target_level = std::max(0.01f, std::min(1.0f, target));
}

// VoiceActivityDetector implementation
VoiceActivityDetector::VoiceActivityDetector()
    : energy_threshold(0.1f), noise_floor(0.01f), history_index(0),
      voice_detected(false), consecutive_voice_frames(0), consecutive_silence_frames(0) {
    energy_history.resize(ENERGY_HISTORY_SIZE);
    std::fill(energy_history.begin(), energy_history.end(), 0.0f);
}

bool VoiceActivityDetector::initialize(float threshold) {
    energy_threshold = std::max(0.001f, std::min(1.0f, threshold));
    noise_floor = 0.01f;
    history_index = 0;
    voice_detected = false;
    consecutive_voice_frames = 0;
    consecutive_silence_frames = 0;
    std::fill(energy_history.begin(), energy_history.end(), 0.0f);
    return true;
}

float VoiceActivityDetector::calculateEnergy(const float* samples, size_t count) {
    float energy = 0.0f;
    for (size_t i = 0; i < count; i++) {
        energy += samples[i] * samples[i];
    }
    return energy / count;
}

void VoiceActivityDetector::updateNoiseFloor(float energy) {
    // Update noise floor with exponential smoothing
    noise_floor = 0.95f * noise_floor + 0.05f * energy;
}

bool VoiceActivityDetector::detectVoiceActivity(const float* samples, size_t count) {
    float energy = calculateEnergy(samples, count);
    
    // Update energy history
    energy_history[history_index] = energy;
    history_index = (history_index + 1) % ENERGY_HISTORY_SIZE;
    
    // Update noise floor if no voice detected
    if (!voice_detected) {
        updateNoiseFloor(energy);
    }
    
    // Adaptive threshold based on noise floor
    float adaptive_threshold = noise_floor + energy_threshold;
    
    // Voice activity detection with hysteresis
    if (energy > adaptive_threshold * 2.0f) {
        consecutive_voice_frames++;
        consecutive_silence_frames = 0;
        
        if (consecutive_voice_frames > 3) {  // 3 frames hysteresis
            voice_detected = true;
        }
    } else if (energy < adaptive_threshold * 0.5f) {
        consecutive_silence_frames++;
        consecutive_voice_frames = 0;
        
        if (consecutive_silence_frames > 10) {  // 10 frames hysteresis
            voice_detected = false;
        }
    }
    
    return voice_detected;
}

void VoiceActivityDetector::reset() {
    voice_detected = false;
    consecutive_voice_frames = 0;
    consecutive_silence_frames = 0;
    std::fill(energy_history.begin(), energy_history.end(), 0.0f);
    noise_floor = 0.01f;
}

// AudioBuffer implementation
AudioBuffer::AudioBuffer(size_t size) 
    : buffer(size), write_pos(0), read_pos(0), available_samples(0) {}

bool AudioBuffer::write(const float* samples, size_t count) {
    if (available_samples + count > buffer.size()) {
        return false;  // Buffer overflow
    }
    
    for (size_t i = 0; i < count; i++) {
        buffer[write_pos] = samples[i];
        write_pos = (write_pos + 1) % buffer.size();
    }
    
    available_samples += count;
    return true;
}

bool AudioBuffer::read(float* samples, size_t count) {
    if (available_samples < count) {
        return false;  // Buffer underrun
    }
    
    for (size_t i = 0; i < count; i++) {
        samples[i] = buffer[read_pos];
        read_pos = (read_pos + 1) % buffer.size();
    }
    
    available_samples -= count;
    return true;
}

void AudioBuffer::clear() {
    write_pos = 0;
    read_pos = 0;
    available_samples = 0;
}

// AudioProcessor implementation
AudioProcessor::AudioProcessor()
    : initialized(false), safe_mode(false), i2s_initialized(false),
      i2s_errors(0), processing_buffer(nullptr), processing_buffer_size(0),
      processing_enabled(true) {
    
    // Set default configuration
    config.quality = AudioQuality::HIGH;
    config.enable_noise_reduction = true;
    config.enable_agc = true;
    config.enable_vad = true;
    config.sample_rate = I2S_SAMPLE_RATE;
    config.bit_depth = 16;
    config.channels = 1;
}

AudioProcessor::~AudioProcessor() {
    shutdown();
}

bool AudioProcessor::initialize() {
    if (initialized) {
        return true;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "AudioProcessor", "Initializing AudioProcessor");
    }
    
    // Initialize I2S
    if (!initializeI2S()) {
        if (logger) {
            logger->log(LOG_ERROR, "AudioProcessor", "I2S initialization failed");
        }
        return false;
    }
    
    // Allocate processing buffer
    processing_buffer_size = I2S_BUFFER_SIZE / 2;  // 16-bit samples
    processing_buffer = new float[processing_buffer_size];
    if (!processing_buffer) {
        if (logger) {
            logger->log(LOG_ERROR, "AudioProcessor", "Failed to allocate processing buffer");
        }
        return false;
    }
    
    // Initialize processing components
    if (config.enable_noise_reduction) {
        noise_reducer = std::make_unique<NoiseReducer>();
        noise_reducer->initialize(config.noise_reduction_level);
    }
    
    if (config.enable_agc) {
        agc = std::make_unique<AutomaticGainControl>();
        agc->initialize(config.agc_target_level, config.agc_max_gain);
    }
    
    if (config.enable_vad) {
        vad = std::make_unique<VoiceActivityDetector>();
        vad->initialize();
    }
    
    // Initialize audio buffers
    input_buffer = std::make_unique<AudioBuffer>(processing_buffer_size * 4);
    output_buffer = std::make_unique<AudioBuffer>(processing_buffer_size * 4);
    
    initialized = true;
    processing_enabled = true;
    
    if (logger) {
        logger->log(LOG_INFO, "AudioProcessor", "AudioProcessor initialized successfully");
        logger->log(LOG_INFO, "AudioProcessor", "Sample rate: %u Hz, Bit depth: %u, Channels: %u",
                   config.sample_rate, config.bit_depth, config.channels);
        logger->log(LOG_INFO, "AudioProcessor", "Processing features: NR=%s, AGC=%s, VAD=%s",
                   config.enable_noise_reduction ? "yes" : "no",
                   config.enable_agc ? "yes" : "no",
                   config.enable_vad ? "yes" : "no");
    }
    
    return true;
}

bool AudioProcessor::initializeI2S() {
    // Use existing I2S initialization from the original code
    i2s_initialized = I2SAudio::initialize();
    return i2s_initialized;
}

void AudioProcessor::shutdown() {
    if (!initialized) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "AudioProcessor", "Shutting down AudioProcessor");
        printStatistics();
    }
    
    // Clean up I2S
    if (i2s_initialized) {
        I2SAudio::cleanup();
        i2s_initialized = false;
    }
    
    // Clean up processing buffer
    if (processing_buffer) {
        delete[] processing_buffer;
        processing_buffer = nullptr;
    }
    
    // Reset components
    noise_reducer.reset();
    agc.reset();
    vad.reset();
    input_buffer.reset();
    output_buffer.reset();
    
    initialized = false;
}

void AudioProcessor::setConfig(const AudioConfig& new_config) {
    config = new_config;
    
    // Reinitialize components if needed
    if (initialized) {
        if (config.enable_noise_reduction && !noise_reducer) {
            noise_reducer = std::make_unique<NoiseReducer>();
            noise_reducer->initialize(config.noise_reduction_level);
        } else if (!config.enable_noise_reduction && noise_reducer) {
            noise_reducer.reset();
        }
        
        if (config.enable_agc && !agc) {
            agc = std::make_unique<AutomaticGainControl>();
            agc->initialize(config.agc_target_level, config.agc_max_gain);
        } else if (!config.enable_agc && agc) {
            agc.reset();
        }
        
        if (config.enable_vad && !vad) {
            vad = std::make_unique<VoiceActivityDetector>();
            vad->initialize();
        } else if (!config.enable_vad && vad) {
            vad.reset();
        }
    }
}

void AudioProcessor::setQuality(AudioQuality quality) {
    config.quality = quality;
    
    // Adjust parameters based on quality
    switch (quality) {
        case AudioQuality::LOW:
            config.sample_rate = 8000;
            config.bit_depth = 8;
            config.enable_noise_reduction = false;
            config.enable_agc = true;
            config.enable_vad = false;
            break;
            
        case AudioQuality::MEDIUM:
            config.sample_rate = 16000;
            config.bit_depth = 8;
            config.enable_noise_reduction = true;
            config.enable_agc = true;
            config.enable_vad = false;
            break;
            
        case AudioQuality::HIGH:
            config.sample_rate = 16000;
            config.bit_depth = 16;
            config.enable_noise_reduction = true;
            config.enable_agc = true;
            config.enable_vad = true;
            break;
            
        case AudioQuality::ULTRA:
            config.sample_rate = 32000;
            config.bit_depth = 16;
            config.enable_noise_reduction = true;
            config.enable_agc = true;
            config.enable_vad = true;
            break;
    }
}

void AudioProcessor::enableFeature(AudioFeature feature, bool enable) {
    switch (feature) {
        case AudioFeature::NOISE_REDUCTION:
            config.enable_noise_reduction = enable;
            break;
        case AudioFeature::AUTOMATIC_GAIN_CONTROL:
            config.enable_agc = enable;
            break;
        case AudioFeature::VOICE_ACTIVITY_DETECTION:
            config.enable_vad = enable;
            break;
        case AudioFeature::ECHO_CANCELLATION:
            config.enable_echo_cancellation = enable;
            break;
        case AudioFeature::COMPRESSION:
            config.enable_compression = enable;
            break;
    }
}

bool AudioProcessor::isFeatureEnabled(AudioFeature feature) const {
    switch (feature) {
        case AudioFeature::NOISE_REDUCTION:
            return config.enable_noise_reduction;
        case AudioFeature::AUTOMATIC_GAIN_CONTROL:
            return config.enable_agc;
        case AudioFeature::VOICE_ACTIVITY_DETECTION:
            return config.enable_vad;
        case AudioFeature::ECHO_CANCELLATION:
            return config.enable_echo_cancellation;
        case AudioFeature::COMPRESSION:
            return config.enable_compression;
        default:
            return false;
    }
}

bool AudioProcessor::readData(uint8_t* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!initialized || !i2s_initialized) {
        return false;
    }
    
    // Read raw data from I2S
    size_t raw_bytes_read = 0;
    if (!I2SAudio::readData(buffer, buffer_size, &raw_bytes_read)) {
        i2s_errors++;
        stats.processing_errors++;
        return false;
    }
    
    if (raw_bytes_read == 0) {
        *bytes_read = 0;
        return true;  // No data available, but not an error
    }
    
    // Process audio if enabled
    if (processing_enabled && !safe_mode) {
        size_t sample_count = raw_bytes_read / 2;  // 16-bit samples
        
        // Convert to float for processing
        convertToFloat(buffer, processing_buffer, sample_count);
        
        // Process audio
        processAudioFrame(processing_buffer, sample_count);
        
        // Convert back to 16-bit
        convertFromFloat(processing_buffer, buffer, sample_count);
        
        *bytes_read = sample_count * 2;
    } else {
        *bytes_read = raw_bytes_read;
    }
    
    stats.samples_processed += *bytes_read / 2;
    
    return true;
}

bool AudioProcessor::readDataWithRetry(uint8_t* buffer, size_t buffer_size, size_t* bytes_read, int max_retries) {
    for (int retry = 0; retry < max_retries; retry++) {
        if (readData(buffer, buffer_size, bytes_read)) {
            return true;
        }
        
        if (retry < max_retries - 1) {
            delay(10);  // Small delay before retry
        }
    }
    
    return false;
}

void AudioProcessor::processAudioFrame(float* samples, size_t count) {
    // Update statistics
    float input_level = calculateRMS(samples, count);
    stats.average_input_level = 0.95f * stats.average_input_level + 0.05f * input_level;
    
    // Apply processing chain
    if (config.enable_noise_reduction && noise_reducer) {
        noise_reducer->processAudio(samples, count);
        stats.noise_reduction_applied++;
    }
    
    if (config.enable_agc && agc) {
        agc->processAudio(samples, count);
        stats.agc_adjustments++;
        stats.current_gain = agc->getCurrentGain();
    }
    
    if (config.enable_vad && vad) {
        bool voice_detected = vad->detectVoiceActivity(samples, count);
        if (voice_detected) {
            stats.voice_activity_detected++;
        }
    }
    
    // Check for clipping
    float peak_level = calculatePeak(samples, count);
    if (peak_level > 0.95f) {
        stats.clipping_events++;
    }
    
    // Update output statistics
    stats.average_output_level = 0.95f * stats.average_output_level + 0.05f * calculateRMS(samples, count);
}

void AudioProcessor::convertToFloat(const uint8_t* input, float* output, size_t count) {
    const int16_t* input_samples = reinterpret_cast<const int16_t*>(input);
    for (size_t i = 0; i < count; i++) {
        output[i] = input_samples[i] / 32768.0f;  // Normalize to [-1, 1]
    }
}

void AudioProcessor::convertFromFloat(const float* input, uint8_t* output, size_t count) {
    int16_t* output_samples = reinterpret_cast<int16_t*>(output);
    for (size_t i = 0; i < count; i++) {
        float sample = std::max(-1.0f, std::min(1.0f, input[i]));
        output_samples[i] = static_cast<int16_t>(sample * 32767.0f);
    }
}

bool AudioProcessor::reinitialize() {
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LOG_INFO, "AudioProcessor", "Reinitializing audio system");
    }
    
    shutdown();
    return initialize();
}

bool AudioProcessor::healthCheck() {
    if (!initialized) {
        return false;
    }
    
    // Check I2S health
    if (i2s_errors > 100) {
        return false;
    }
    
    // Check processing errors
    if (stats.processing_errors > 50) {
        return false;
    }
    
    return true;
}

void AudioProcessor::resetStatistics() {
    stats = AudioStats();
    i2s_errors = 0;
}

void AudioProcessor::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LOG_INFO, "AudioProcessor", "=== Audio Processor Statistics ===");
    logger->log(LOG_INFO, "AudioProcessor", "Samples processed: %u", stats.samples_processed);
    logger->log(LOG_INFO, "AudioProcessor", "Noise reduction applied: %u times", stats.noise_reduction_applied);
    logger->log(LOG_INFO, "AudioProcessor", "AGC adjustments: %u", stats.agc_adjustments);
    logger->log(LOG_INFO, "AudioProcessor", "Voice activity detected: %u times", stats.voice_activity_detected);
    logger->log(LOG_INFO, "AudioProcessor", "Silent frames: %u", stats.silent_frames);
    logger->log(LOG_INFO, "AudioProcessor", "Clipping events: %u", stats.clipping_events);
    logger->log(LOG_INFO, "AudioProcessor", "Processing errors: %u", stats.processing_errors);
    logger->log(LOG_INFO, "AudioProcessor", "I2S errors: %u", i2s_errors);
    logger->log(LOG_INFO, "AudioProcessor", "Input level: %.2f dB", 20.0f * log10f(stats.average_input_level + 0.001f));
    logger->log(LOG_INFO, "AudioProcessor", "Output level: %.2f dB", 20.0f * log10f(stats.average_output_level + 0.001f));
    logger->log(LOG_INFO, "AudioProcessor", "Current gain: %.2f", stats.current_gain);
    logger->log(LOG_INFO, "AudioProcessor", "Buffer underruns: %u", stats.buffer_underruns);
    logger->log(LOG_INFO, "AudioProcessor", "Buffer overruns: %u", stats.buffer_overruns);
    logger->log(LOG_INFO, "AudioProcessor", "==================================");
}

float AudioProcessor::getAudioQualityScore() const {
    float score = 1.0f;
    
    // Penalize clipping events
    if (stats.clipping_events > 0) {
        score *= 0.9f;
    }
    
    // Penalize processing errors
    if (stats.processing_errors > 10) {
        score *= 0.8f;
    }
    
    // Penalize low output levels
    if (stats.average_output_level < 0.1f) {
        score *= 0.95f;
    }
    
    // Reward voice activity detection
    if (stats.voice_activity_detected > 0) {
        score *= 1.05f;
    }
    
    return std::max(0.0f, std::min(1.0f, score));
}

bool AudioProcessor::isVoiceActive() const {
    return vad && vad->isVoiceDetected();
}

// Static utility methods
float AudioProcessor::calculateRMS(const float* samples, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += samples[i] * samples[i];
    }
    return sqrtf(sum / count);
}

float AudioProcessor::calculatePeak(const float* samples, size_t count) {
    float peak = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float abs_sample = std::abs(samples[i]);
        if (abs_sample > peak) peak = abs_sample;
    }
    return peak;
}

float AudioProcessor::calculateSNR(const float* signal, const float* noise, size_t count) {
    float signal_power = 0.0f;
    float noise_power = 0.0f;
    
    for (size_t i = 0; i < count; i++) {
        signal_power += signal[i] * signal[i];
        noise_power += noise[i] * noise[i];
    }
    
    signal_power /= count;
    noise_power /= count;
    
    if (noise_power == 0.0f) return 100.0f;  // Perfect SNR
    return 10.0f * log10f(signal_power / noise_power);
}

void AudioProcessor::applyHighPassFilter(float* samples, size_t count, float cutoff_freq, float sample_rate) {
    // Simple first-order high-pass filter
    float rc = 1.0f / (2.0f * M_PI * cutoff_freq);
    float dt = 1.0f / sample_rate;
    float alpha = rc / (rc + dt);
    
    float prev_input = samples[0];
    float prev_output = samples[0];
    
    for (size_t i = 1; i < count; i++) {
        float input = samples[i];
        float output = alpha * (prev_output + input - prev_input);
        samples[i] = output;
        prev_input = input;
        prev_output = output;
    }
}

void AudioProcessor::applyLowPassFilter(float* samples, size_t count, float cutoff_freq, float sample_rate) {
    // Simple first-order low-pass filter
    float rc = 1.0f / (2.0f * M_PI * cutoff_freq);
    float dt = 1.0f / sample_rate;
    float alpha = dt / (rc + dt);
    
    float prev_output = samples[0];
    
    for (size_t i = 1; i < count; i++) {
        float input = samples[i];
        float output = prev_output + alpha * (input - prev_output);
        samples[i] = output;
        prev_output = output;
    }
}