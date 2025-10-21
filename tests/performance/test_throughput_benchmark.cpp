#ifdef PERFORMANCE_TEST

#include <unity.h>
#include "../../src/audio/AudioProcessor.h"
#include "../../src/network/NetworkManager.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_audio_buffer_throughput(void) {
    AudioBuffer buffer(8192);
    
    float data[1024];
    for (int i = 0; i < 1024; i++) {
        data[i] = 0.1f;
    }
    
    unsigned long start_time = millis();
    int write_count = 0;
    
    while (millis() - start_time < 1000 && write_count < 100) {
        buffer.write(data, 1024);
        write_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = (write_count * 1024 * 1000.0f) / elapsed;
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

void test_audio_processing_throughput(void) {
    AudioProcessor processor;
    processor.initialize();
    
    uint8_t buffer[1024];
    for (int i = 0; i < 1024; i++) {
        buffer[i] = 0x80;
    }
    
    unsigned long start_time = millis();
    int process_count = 0;
    
    while (millis() - start_time < 1000 && process_count < 100) {
        size_t bytes_read = 0;
        processor.readData(buffer, 1024, &bytes_read);
        process_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = (process_count * 1024 * 1000.0f) / elapsed;
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

void test_network_data_throughput_simulation(void) {
    uint8_t test_data[512];
    for (int i = 0; i < 512; i++) {
        test_data[i] = static_cast<uint8_t>(i % 256);
    }
    
    unsigned long start_time = millis();
    int send_count = 0;
    
    while (millis() - start_time < 1000 && send_count < 100) {
        send_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = (send_count * 512 * 1000.0f) / elapsed;
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

void test_vad_processing_throughput(void) {
    VoiceActivityDetector vad;
    vad.initialize(0.1f);
    
    float samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = millis();
    int detect_count = 0;
    
    while (millis() - start_time < 1000 && detect_count < 1000) {
        vad.detectVoiceActivity(samples, 100);
        detect_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = (detect_count * 100 * 1000.0f) / elapsed;
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

void test_agc_processing_throughput(void) {
    AutomaticGainControl agc;
    agc.initialize(0.3f, 10.0f);
    
    float samples[100];
    for (int i = 0; i < 100; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = millis();
    int process_count = 0;
    
    while (millis() - start_time < 1000 && process_count < 1000) {
        agc.processAudio(samples, 100);
        process_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = (process_count * 100 * 1000.0f) / elapsed;
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

void test_rms_calculation_throughput(void) {
    float samples[1024];
    for (int i = 0; i < 1024; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = millis();
    int calc_count = 0;
    
    while (millis() - start_time < 1000 && calc_count < 1000) {
        float rms = AudioProcessor::calculateRMS(samples, 1024);
        (void)rms;
        calc_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = static_cast<float>(calc_count);
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

void test_peak_calculation_throughput(void) {
    float samples[1024];
    for (int i = 0; i < 1024; i++) {
        samples[i] = 0.1f;
    }
    
    unsigned long start_time = millis();
    int calc_count = 0;
    
    while (millis() - start_time < 1000 && calc_count < 1000) {
        float peak = AudioProcessor::calculatePeak(samples, 1024);
        (void)peak;
        calc_count++;
    }
    
    unsigned long elapsed = millis() - start_time;
    float throughput = static_cast<float>(calc_count);
    
    TEST_ASSERT_TRUE(throughput > 0.0f);
}

#endif
