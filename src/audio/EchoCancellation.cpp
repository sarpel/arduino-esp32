#include "EchoCancellation.h"
#include <algorithm>
#include <cmath>

EchoCancellation::EchoCancellation()
    : learning_rate(0.01f), step_size(0.001f), buffer_index(0), processing_count(0)
{
}

bool EchoCancellation::initialize(float lr)
{
    learning_rate = lr;

    filter_coefficients.resize(FILTER_ORDER, 0.0f);
    reference_buffer.resize(FFT_SIZE, 0.0f);
    error_buffer.resize(FFT_SIZE, 0.0f);
    fft_buffer.resize(FFT_SIZE, {0.0f, 0.0f});

    return true;
}

void EchoCancellation::updateFilterCoefficients(const float *input, const float *reference, size_t count)
{
    for (size_t i = 0; i < count && i < FILTER_ORDER; i++)
    {
        float error = reference[i];

        for (size_t j = 0; j < FILTER_ORDER && j < reference_buffer.size(); j++)
        {
            size_t idx = (buffer_index + j) % reference_buffer.size();
            error -= filter_coefficients[j] * reference_buffer[idx];
        }

        for (size_t j = 0; j < FILTER_ORDER; j++)
        {
            size_t idx = (buffer_index + j) % reference_buffer.size();
            filter_coefficients[j] += learning_rate * error * reference_buffer[idx];
        }
    }
}

void EchoCancellation::performFrequencyDomainProcessing(std::vector<std::complex<float>> &spectrum)
{
    for (size_t i = 0; i < spectrum.size(); i++)
    {
        float magnitude = abs(spectrum[i]);
        if (magnitude > 1.0f)
        {
            spectrum[i] = spectrum[i] * (1.0f - learning_rate * 0.1f);
        }
    }
}

void EchoCancellation::processAudio(const float *reference, float *echo_signal, size_t count)
{
    if (!reference || !echo_signal)
    {
        return;
    }

    const size_t ring_size = reference_buffer.size();

    for (size_t i = 0; i < count; i++)
    {
        // 1) Write new far-end sample to ring buffer
        reference_buffer[buffer_index] = reference[i];

        // 2) Estimate echo: read from newest to oldest (correct direction)
        float y_hat = 0.0f;
        size_t idx = buffer_index; // Start at newest sample
        for (size_t j = 0; j < FILTER_ORDER; j++)
        {
            y_hat += filter_coefficients[j] * reference_buffer[idx];
            // Move backwards through circular buffer
            idx = (idx == 0) ? (ring_size - 1) : (idx - 1);
        }

        // 3) Calculate error (residual) - only once
        const float e = reference[i] - y_hat;
        echo_signal[i] = e;

        // 4) NLMS update (inline, with proper alignment)
        float power = 1e-6f; // Regularization
        idx = buffer_index;
        for (size_t j = 0; j < FILTER_ORDER; j++)
        {
            const float x = reference_buffer[idx];
            power += x * x;
            idx = (idx == 0) ? (ring_size - 1) : (idx - 1);
        }

        const float mu = learning_rate / power;
        idx = buffer_index;
        for (size_t j = 0; j < FILTER_ORDER; j++)
        {
            filter_coefficients[j] += mu * e * reference_buffer[idx];
            idx = (idx == 0) ? (ring_size - 1) : (idx - 1);
        }

        // 5) Advance buffer index for next sample
        buffer_index = (buffer_index + 1) % ring_size;
    }

    processing_count++;
}

void EchoCancellation::resetFilter()
{
    std::fill(filter_coefficients.begin(), filter_coefficients.end(), 0.0f);
    std::fill(reference_buffer.begin(), reference_buffer.end(), 0.0f);
    std::fill(error_buffer.begin(), error_buffer.end(), 0.0f);
    buffer_index = 0;
}

float EchoCancellation::getAttenuation() const
{
    float total_coefficient = 0.0f;
    for (size_t i = 0; i < filter_coefficients.size(); i++)
    {
        total_coefficient += fabsf(filter_coefficients[i]);
    }
    return 20.0f * log10f(total_coefficient + 0.0001f);
}
