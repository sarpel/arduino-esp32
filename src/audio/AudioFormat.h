#ifndef AUDIO_FORMAT_H
#define AUDIO_FORMAT_H

#include <Arduino.h>
#include <cstdint>
#include <memory>
#include <vector>

enum class AudioFormatType {
    RAW_PCM = 0,
    WAV = 1,
    OPUS = 2,
    MP3 = 3,
    FLAC = 4
};

struct WAVHeader {
    uint8_t riff[4];
    uint32_t file_size;
    uint8_t wave[4];
    uint8_t fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint8_t data[4];
    uint32_t data_size;
    
    static constexpr uint16_t FORMAT_PCM = 1;
    static constexpr size_t HEADER_SIZE = 44;
};

struct OpusFrameHeader {
    uint8_t toc;
    uint16_t frame_size;
    uint8_t packet_count;
    bool has_padding;
    uint16_t padding_size;
};

class AudioFormatConverter {
private:
    AudioFormatType input_format;
    AudioFormatType output_format;
    
    bool initialized;
    
    bool convertWAVToRaw(const uint8_t* wav_data, size_t wav_size, 
                        uint8_t* raw_data, size_t& raw_size);
    bool convertRawToWAV(const uint8_t* raw_data, size_t raw_size,
                        uint32_t sample_rate, uint8_t channels,
                        uint8_t* wav_data, size_t& wav_size);
    
    bool validateWAVHeader(const WAVHeader& header);
    void buildWAVHeader(WAVHeader& header, uint32_t sample_rate, 
                       uint8_t channels, uint32_t data_size);
    
public:
    AudioFormatConverter();
    
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    void setInputFormat(AudioFormatType format) { input_format = format; }
    void setOutputFormat(AudioFormatType format) { output_format = format; }
    
    bool convert(const uint8_t* input, size_t input_size,
                uint8_t* output, size_t& output_size);
    
    bool decodeWAV(const uint8_t* wav_data, size_t wav_size,
                  uint8_t* pcm_data, size_t& pcm_size,
                  uint32_t& sample_rate, uint8_t& channels);
    
    bool encodeWAV(const uint8_t* pcm_data, size_t pcm_size,
                  uint32_t sample_rate, uint8_t channels,
                  uint8_t* wav_data, size_t& wav_size);
    
    bool isOpusFrame(const uint8_t* data, size_t size);
    bool parseOpusHeader(const uint8_t* data, size_t size, OpusFrameHeader& header);
};

class AudioStreamWriter {
private:
    AudioFormatType format;
    FILE* file_handle;
    uint32_t samples_written;
    uint32_t total_bytes;
    bool file_open;
    
public:
    AudioStreamWriter();
    ~AudioStreamWriter();
    
    bool openFile(const char* filename, AudioFormatType fmt, 
                 uint32_t sample_rate, uint8_t channels, uint8_t bit_depth);
    void closeFile();
    
    bool writeAudioData(const uint8_t* data, size_t size);
    bool finalizeFile();
    
    uint32_t getSamplesWritten() const { return samples_written; }
    uint32_t getTotalBytes() const { return total_bytes; }
    bool isOpen() const { return file_open; }
};

#endif
