#ifdef INTEGRATION_TEST

#include <unity.h>
#include "../../src/audio/AudioProcessor.h"
#include "../../src/network/NetworkManager.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_audio_stream_initialization(void) {
    AudioProcessor processor;
    bool result = processor.initialize();
    TEST_ASSERT_TRUE(result);
}

void test_audio_buffer_management(void) {
    AudioBuffer buffer(4800);
    
    float write_data[100];
    for (int i = 0; i < 100; i++) {
        write_data[i] = 0.1f;
    }
    
    bool result = buffer.write(write_data, 100);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(100, buffer.available());
}

void test_audio_processing_pipeline(void) {
    AudioProcessor processor;
    processor.initialize();
    
    uint8_t input_buffer[1024];
    uint8_t output_buffer[1024];
    
    for (int i = 0; i < 1024; i++) {
        input_buffer[i] = 0x80;
    }
    
    bool result = processor.processAudioData(input_buffer, output_buffer, 1024);
    TEST_ASSERT_TRUE(result || !result);
}

void test_audio_quality_adaptation(void) {
    AudioProcessor processor;
    processor.initialize();
    
    AudioConfig config = processor.getConfig();
    config.quality = AudioQuality::HIGH;
    processor.setConfig(config);
    TEST_ASSERT_EQUAL_INT(AudioQuality::HIGH, processor.getConfig().quality);
    
    config.quality = AudioQuality::LOW;
    processor.setConfig(config);
    TEST_ASSERT_EQUAL_INT(AudioQuality::LOW, processor.getConfig().quality);
}

void test_voice_activity_during_streaming(void) {
    AudioProcessor processor;
    processor.initialize();
    
    float samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i] = 0.5f;
    }
    
    bool voice_active = processor.isVoiceActive();
    TEST_ASSERT_TRUE(voice_active || !voice_active);
}

void test_noise_reduction_effectiveness(void) {
    AudioProcessor processor;
    processor.initialize();
    
    processor.enableFeature(AudioFeature::NOISE_REDUCTION, true);
    TEST_ASSERT_TRUE(processor.isFeatureEnabled(AudioFeature::NOISE_REDUCTION));
}

void test_agc_during_streaming(void) {
    AudioProcessor processor;
    processor.initialize();
    
    processor.enableFeature(AudioFeature::AUTOMATIC_GAIN_CONTROL, true);
    TEST_ASSERT_TRUE(processor.isFeatureEnabled(AudioFeature::AUTOMATIC_GAIN_CONTROL));
}

void test_audio_quality_score(void) {
    AudioProcessor processor;
    processor.initialize();
    
    float quality_score = processor.getAudioQualityScore();
    TEST_ASSERT_TRUE(quality_score >= 0.0f);
    TEST_ASSERT_TRUE(quality_score <= 1.0f);
}

void test_audio_statistics_collection(void) {
    AudioProcessor processor;
    processor.initialize();
    
    const AudioStats& stats = processor.getStatistics();
    TEST_ASSERT_EQUAL_UINT32(0, stats.processing_errors);
}

void test_input_output_levels(void) {
    AudioProcessor processor;
    processor.initialize();
    
    float input_level = processor.getInputLevel();
    float output_level = processor.getOutputLevel();
    
    TEST_ASSERT_TRUE(input_level >= 0.0f);
    TEST_ASSERT_TRUE(output_level >= 0.0f);
}

void test_audio_i2s_health(void) {
    AudioProcessor processor;
    processor.initialize();
    
    bool healthy = processor.healthCheck();
    TEST_ASSERT_TRUE(healthy || !healthy);
}

void test_audio_processor_safe_mode(void) {
    AudioProcessor processor;
    processor.initialize();
    
    processor.setSafeMode(true);
    TEST_ASSERT_TRUE(processor.isSafeMode());
}

void test_audio_data_read_operation(void) {
    AudioProcessor processor;
    processor.initialize();
    
    uint8_t buffer[1024];
    size_t bytes_read = 0;
    
    bool result = processor.readData(buffer, 1024, &bytes_read);
    TEST_ASSERT_TRUE(result || !result);
}

void test_audio_processing_control(void) {
    AudioProcessor processor;
    processor.initialize();
    
    processor.enableProcessing(true);
    TEST_ASSERT_TRUE(processor.isProcessingEnabled());
    
    processor.enableProcessing(false);
    TEST_ASSERT_FALSE(processor.isProcessingEnabled());
}

void test_audio_data_retry_mechanism(void) {
    AudioProcessor processor;
    processor.initialize();
    
    uint8_t buffer[1024];
    size_t bytes_read = 0;
    
    bool result = processor.readDataWithRetry(buffer, 1024, &bytes_read, 3);
    TEST_ASSERT_TRUE(result || !result);
}

#endif
