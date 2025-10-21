#include "EchoCancellation.h"
#include <cstring>

EchoCancellation::EchoCancellation() 
    : learning_rate(0.01f), step_size(0.001f), buffer_index(0), processing_count(0) {
}

bool EchoCancellation::initialize(float lr) {
    learning_rate = lr;
    
    filter_coefficients.resize(FILTER_ORDER, 0.0f);
    reference_buffer.resize(FFT_SIZE, 0.0f);
    error_buffer.resize(FFT_SIZE, 0.0f);
    fft_buffer.resize(FFT_SIZE, {0.0f, 0.0f});
    
    return true;
}

void EchoCancellation::updateFilterCoefficients(const float* input, const float* reference, size_t count) {
    for (size_t i = 0; i < count && i < FILTER_ORDER; i++) {
        float error = reference[i];
        
        for (size_t j = 0; j < FILTER_ORDER && j < reference_buffer.size(); j++) {
            size_t idx = (buffer_index + j) % reference_buffer.size();
            error -= filter_coefficients[j] * reference_buffer[idx];
        }
        
        for (size_t j = 0; j < FILTER_ORDER; j++) {
            size_t idx = (buffer_index + j) % reference_buffer.size();
            filter_coefficients[j] += learning_rate * error * reference_buffer[idx];
        }
    }
}

void EchoCancellation::performFrequencyDomainProcessing(std::vector<std::complex<float>>& spectrum) {
    for (size_t i = 0; i < spectrum.size(); i++) {
        float magnitude = abs(spectrum[i]);
        if (magnitude > 1.0f) {
            spectrum[i] = spectrum[i] * (1.0f - learning_rate * 0.1f);
        }
    }
}

void EchoCancellation::processAudio(const float* reference, float* echo_signal, size_t count) {
    if (!reference || !echo_signal) {
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        float estimated_echo = 0.0f;
        
        for (size_t j = 0; j < FILTER_ORDER && j < reference_buffer.size(); j++) {
            size_t idx = (buffer_index + j) % reference_buffer.size();
            estimated_echo += filter_coefficients[j] * reference_buffer[idx];
        }
        
        echo_signal[i] = reference[i] - estimated_echo;
        
        reference_buffer[buffer_index] = reference[i];
        buffer_index = (buffer_index + 1) % reference_buffer.size();
    }
    
    updateFilterCoefficients(reference, echo_signal, count);
    processing_count++;
}

void EchoCancellation::resetFilter() {
    std::fill(filter_coefficients.begin(), filter_coefficients.end(), 0.0f);
    std::fill(reference_buffer.begin(), reference_buffer.end(), 0.0f);
    std::fill(error_buffer.begin(), error_buffer.end(), 0.0f);
    buffer_index = 0;
}

float EchoCancellation::getAttenuation() const {
    float total_coefficient = 0.0f;
    for (size_t i = 0; i < filter_coefficients.size(); i++) {
        total_coefficient += fabsf(filter_coefficients[i]);
    }
    return 20.0f * log10f(total_coefficient + 0.0001f);
}
