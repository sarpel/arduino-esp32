#ifdef UNIT_TEST

#include <unity.h>
#include "../../src/audio/AudioProcessor.h"
#include <cmath>

void setUp(void) {
}

void tearDown(void) {
}

void test_audio_processor_initialization(void) {
    AudioProcessor processor;
    TEST_ASSERT_FALSE(processor.isInitialized());
    
    bool result = processor.initialize();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(processor.isInitialized());
}

void test_audio_quality_levels(void) {
    AudioProcessor processor;
    processor.initialize();
    
    AudioConfig config;
    
    config.quality = AudioQuality::LOW;
    processor.setConfig(config);
    TEST_ASSERT_EQUAL(AudioQuality::LOW, processor.getConfig().quality);
    
    config.quality = AudioQuality::HIGH;
    processor.setConfig(config);
    TEST_ASSERT_EQUAL(AudioQuality::HIGH, processor.getConfig().quality);
}

void test_noise_reducer_initialization(void) {
    NoiseReducer reducer;
    bool result = reducer.initialize(0.7f);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(reducer.isProfileInitialized());
}

void test_agc_gain_calculation(void) {
    AutomaticGainControl agc;
    agc.initialize(0.3f, 10.0f);
    
    float samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i] = 0.1f;
    }
    
    agc.processAudio(samples, 100);
    float gain = agc.getCurrentGain();
    
    TEST_ASSERT_TRUE(gain > 0.0f);
    TEST_ASSERT_TRUE(gain <= 10.0f);
}

void test_vad_voice_detection(void) {
    VoiceActivityDetector vad;
    vad.initialize(0.1f);
    
    float silent_samples[100];
    for (int i = 0; i < 100; i++) {
        silent_samples[i] = 0.001f;
    }
    
    bool voice_detected = vad.detectVoiceActivity(silent_samples, 100);
    TEST_ASSERT_FALSE(voice_detected);
    
    float loud_samples[100];
    for (int i = 0; i < 100; i++) {
        loud_samples[i] = 0.5f;
    }
    
    voice_detected = vad.detectVoiceActivity(loud_samples, 100);
    TEST_ASSERT_TRUE(voice_detected);
}

void test_audio_buffer_write_read(void) {
    AudioBuffer buffer(1000);
    
    float write_data[10];
    for (int i = 0; i < 10; i++) {
        write_data[i] = static_cast<float>(i);
    }
    
    bool result = buffer.write(write_data, 10);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(10, buffer.available());
    
    float read_data[10];
    result = buffer.read(read_data, 10);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(0, buffer.available());
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_FLOAT(write_data[i], read_data[i]);
    }
}

void test_audio_buffer_overflow_protection(void) {
    AudioBuffer buffer(10);
    
    float data[20];
    for (int i = 0; i < 20; i++) {
        data[i] = static_cast<float>(i);
    }
    
    bool result = buffer.write(data, 20);
    TEST_ASSERT_FALSE(result);
}

void test_rms_calculation(void) {
    float samples[4] = {1.0f, -1.0f, 1.0f, -1.0f};
    float rms = AudioProcessor::calculateRMS(samples, 4);
    
    float expected_rms = 1.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_rms, rms);
}

void test_peak_calculation(void) {
    float samples[5] = {0.5f, 0.3f, -0.8f, 0.2f, -0.1f};
    float peak = AudioProcessor::calculatePeak(samples, 5);
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.8f, peak);
}

void test_audio_feature_enabling(void) {
    AudioProcessor processor;
    processor.initialize();
    
    processor.enableFeature(AudioFeature::NOISE_REDUCTION, false);
    TEST_ASSERT_FALSE(processor.isFeatureEnabled(AudioFeature::NOISE_REDUCTION));
    
    processor.enableFeature(AudioFeature::AUTOMATIC_GAIN_CONTROL, true);
    TEST_ASSERT_TRUE(processor.isFeatureEnabled(AudioFeature::AUTOMATIC_GAIN_CONTROL));
}

void test_safe_mode_toggling(void) {
    AudioProcessor processor;
    processor.initialize();
    
    processor.setSafeMode(true);
    TEST_ASSERT_TRUE(processor.isSafeMode());
    
    processor.setSafeMode(false);
    TEST_ASSERT_FALSE(processor.isSafeMode());
}

void test_processing_control(void) {
    AudioProcessor processor;
    processor.initialize();
    
    TEST_ASSERT_TRUE(processor.isProcessingEnabled());
    
    processor.enableProcessing(false);
    TEST_ASSERT_FALSE(processor.isProcessingEnabled());
    
    processor.enableProcessing(true);
    TEST_ASSERT_TRUE(processor.isProcessingEnabled());
}

void test_statistics_reset(void) {
    AudioProcessor processor;
    processor.initialize();
    
    const AudioStats& stats = processor.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(0, stats.samples_processed);
    
    processor.resetStatistics();
    TEST_ASSERT_EQUAL_UINT32(0, processor.getStatistics().samples_processed);
}

#endif
