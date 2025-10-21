#ifndef ECHO_CANCELLATION_H
#define ECHO_CANCELLATION_H

#include <Arduino.h>
#include <vector>
#include <complex>

class EchoCancellation {
private:
    static constexpr size_t FILTER_ORDER = 64;
    static constexpr size_t FFT_SIZE = 512;
    
    std::vector<float> filter_coefficients;
    std::vector<float> reference_buffer;
    std::vector<float> error_buffer;
    std::vector<std::complex<float>> fft_buffer;
    
    float learning_rate;
    float step_size;
    size_t buffer_index;
    uint32_t processing_count;
    
    void updateFilterCoefficients(const float* input, const float* reference, size_t count);
    void performFrequencyDomainProcessing(std::vector<std::complex<float>>& spectrum);
    
public:
    EchoCancellation();
    bool initialize(float learning_rate = 0.01f);
    void processAudio(const float* reference, float* echo_signal, size_t count);
    void resetFilter();
    
    float getAttenuation() const;
    uint32_t getProcessingCount() const { return processing_count; }
};

#endif
