#ifdef PERFORMANCE_TEST

#include <unity.h>
#include "../../src/audio/AudioProcessor.h"
#include <chrono>

void setUp(void) {
}

void tearDown(void) {
}

void test_audio_processing_latency(void) {
    AudioProcessor processor;
    processor.initialize();
    
    uint8_t input_buffer[1024];
    uint8_t output_buffer[1024];
    
    for (int i = 0; i < 1024; i++) {
        input_buffer[i] = 0x80;
    }
    
    unsigned long start_time = micros();
    
    processor.processAudioData(input_buffer, output_buffer, 1024);
    
    unsigned long end_time = micros();
    unsigned long processing_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(processing_time < 100000);
}

void test_vad_detection_latency(void) {
    VoiceActivityDetector vad;
    vad.initialize(0.1f);
    
    float samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = micros();
    
    vad.detectVoiceActivity(samples, 100);
    
    unsigned long end_time = micros();
    unsigned long detection_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(detection_time < 50000);
}

void test_agc_processing_latency(void) {
    AutomaticGainControl agc;
    agc.initialize(0.3f, 10.0f);
    
    float samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = micros();
    
    agc.processAudio(samples, 100);
    
    unsigned long end_time = micros();
    unsigned long agc_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(agc_time < 50000);
}

void test_noise_reduction_latency(void) {
    NoiseReducer reducer;
    reducer.initialize(0.7f);
    
    float samples[256];
    for (int i = 0; i < 256; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = micros();
    
    reducer.processAudio(samples, 256);
    
    unsigned long end_time = micros();
    unsigned long nr_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(nr_time < 200000);
}

void test_buffer_read_latency(void) {
    AudioBuffer buffer(1024);
    
    float write_data[100];
    for (int i = 0; i < 100; i++) {
        write_data[i] = 0.1f;
    }
    
    buffer.write(write_data, 100);
    
    float read_data[100];
    
    unsigned long start_time = micros();
    
    buffer.read(read_data, 100);
    
    unsigned long end_time = micros();
    unsigned long read_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(read_time < 10000);
}

void test_buffer_write_latency(void) {
    AudioBuffer buffer(1024);
    
    float data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = 0.1f;
    }
    
    unsigned long start_time = micros();
    
    buffer.write(data, 100);
    
    unsigned long end_time = micros();
    unsigned long write_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(write_time < 10000);
}

void test_rms_calculation_latency(void) {
    float samples[1024];
    for (int i = 0; i < 1024; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = micros();
    
    float rms = AudioProcessor::calculateRMS(samples, 1024);
    (void)rms;
    
    unsigned long end_time = micros();
    unsigned long calc_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(calc_time < 50000);
}

void test_peak_calculation_latency(void) {
    float samples[1024];
    for (int i = 0; i < 1024; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = micros();
    
    float peak = AudioProcessor::calculatePeak(samples, 1024);
    (void)peak;
    
    unsigned long end_time = micros();
    unsigned long calc_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(calc_time < 50000);
}

void test_quality_score_calculation_latency(void) {
    AudioProcessor processor;
    processor.initialize();
    
    unsigned long start_time = micros();
    
    float score = processor.getAudioQualityScore();
    (void)score;
    
    unsigned long end_time = micros();
    unsigned long calc_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(calc_time < 50000);
}

void test_statistics_retrieval_latency(void) {
    AudioProcessor processor;
    processor.initialize();
    
    unsigned long start_time = micros();
    
    const AudioStats& stats = processor.getStatistics();
    (void)stats;
    
    unsigned long end_time = micros();
    unsigned long retrieval_time = end_time - start_time;
    
    TEST_ASSERT_TRUE(retrieval_time < 10000);
}

#endif
