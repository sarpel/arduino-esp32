#include "AudioFormat.h"
#include "../utils/EnhancedLogger.h"
#include <cstring>
#include <algorithm>

AudioFormatConverter::AudioFormatConverter()
    : input_format(AudioFormatType::RAW_PCM),
      output_format(AudioFormatType::RAW_PCM),
      initialized(false) {
}

bool AudioFormatConverter::initialize() {
    initialized = true;
    return true;
}

void AudioFormatConverter::shutdown() {
    initialized = false;
}

bool AudioFormatConverter::validateWAVHeader(const WAVHeader& header) {
    if (header.riff[0] != 'R' || header.riff[1] != 'I' || 
        header.riff[2] != 'F' || header.riff[3] != 'F') {
        return false;
    }
    
    if (header.wave[0] != 'W' || header.wave[1] != 'A' || 
        header.wave[2] != 'V' || header.wave[3] != 'E') {
        return false;
    }
    
    if (header.audio_format != WAVHeader::FORMAT_PCM) {
        return false;
    }
    
    return true;
}

void AudioFormatConverter::buildWAVHeader(WAVHeader& header, uint32_t sample_rate, 
                                        uint8_t channels, uint32_t data_size) {
    header.riff[0] = 'R';
    header.riff[1] = 'I';
    header.riff[2] = 'F';
    header.riff[3] = 'F';
    
    header.file_size = 36 + data_size;
    
    header.wave[0] = 'W';
    header.wave[1] = 'A';
    header.wave[2] = 'V';
    header.wave[3] = 'E';
    
    header.fmt[0] = 'f';
    header.fmt[1] = 'm';
    header.fmt[2] = 't';
    header.fmt[3] = ' ';
    
    header.fmt_size = 16;
    header.audio_format = WAVHeader::FORMAT_PCM;
    header.num_channels = channels;
    header.sample_rate = sample_rate;
    header.bits_per_sample = 16;
    header.block_align = (header.bits_per_sample / 8) * channels;
    header.byte_rate = sample_rate * header.block_align;
    
    header.data[0] = 'd';
    header.data[1] = 'a';
    header.data[2] = 't';
    header.data[3] = 'a';
    header.data_size = data_size;
}

bool AudioFormatConverter::convertWAVToRaw(const uint8_t* wav_data, size_t wav_size,
                                          uint8_t* raw_data, size_t& raw_size) {
    if (wav_size < WAVHeader::HEADER_SIZE) {
        return false;
    }
    
    WAVHeader header;
    memcpy(&header, wav_data, WAVHeader::HEADER_SIZE);
    
    if (!validateWAVHeader(header)) {
        return false;
    }
    
    size_t audio_data_offset = WAVHeader::HEADER_SIZE;
    size_t audio_data_size = wav_size - audio_data_offset;
    
    if (raw_size < audio_data_size) {
        return false;
    }
    
    memcpy(raw_data, wav_data + audio_data_offset, audio_data_size);
    raw_size = audio_data_size;
    
    return true;
}

bool AudioFormatConverter::convertRawToWAV(const uint8_t* raw_data, size_t raw_size,
                                          uint32_t sample_rate, uint8_t channels,
                                          uint8_t* wav_data, size_t& wav_size) {
    if (wav_size < (WAVHeader::HEADER_SIZE + raw_size)) {
        return false;
    }
    
    WAVHeader header;
    buildWAVHeader(header, sample_rate, channels, raw_size);
    
    memcpy(wav_data, &header, WAVHeader::HEADER_SIZE);
    memcpy(wav_data + WAVHeader::HEADER_SIZE, raw_data, raw_size);
    
    wav_size = WAVHeader::HEADER_SIZE + raw_size;
    
    return true;
}

bool AudioFormatConverter::convert(const uint8_t* input, size_t input_size,
                                  uint8_t* output, size_t& output_size) {
    if (!initialized || !input || !output) {
        return false;
    }
    
    if (input_format == output_format) {
        if (output_size < input_size) {
            return false;
        }
        memcpy(output, input, input_size);
        output_size = input_size;
        return true;
    }
    
    if (input_format == AudioFormatType::WAV && output_format == AudioFormatType::RAW_PCM) {
        return convertWAVToRaw(input, input_size, output, output_size);
    }
    
    return false;
}

bool AudioFormatConverter::decodeWAV(const uint8_t* wav_data, size_t wav_size,
                                    uint8_t* pcm_data, size_t& pcm_size,
                                    uint32_t& sample_rate, uint8_t& channels) {
    if (wav_size < WAVHeader::HEADER_SIZE) {
        return false;
    }
    
    WAVHeader header;
    memcpy(&header, wav_data, WAVHeader::HEADER_SIZE);
    
    if (!validateWAVHeader(header)) {
        return false;
    }
    
    sample_rate = header.sample_rate;
    channels = header.num_channels;
    
    size_t audio_data_offset = WAVHeader::HEADER_SIZE;
    size_t audio_data_size = header.data_size;
    
    if (audio_data_offset + audio_data_size > wav_size) {
        return false;
    }
    
    if (pcm_size < audio_data_size) {
        return false;
    }
    
    memcpy(pcm_data, wav_data + audio_data_offset, audio_data_size);
    pcm_size = audio_data_size;
    
    return true;
}

bool AudioFormatConverter::encodeWAV(const uint8_t* pcm_data, size_t pcm_size,
                                   uint32_t sample_rate, uint8_t channels,
                                   uint8_t* wav_data, size_t& wav_size) {
    return convertRawToWAV(pcm_data, pcm_size, sample_rate, channels, wav_data, wav_size);
}

bool AudioFormatConverter::isOpusFrame(const uint8_t* data, size_t size) {
    if (size < 1) {
        return false;
    }
    
    uint8_t toc = data[0];
    uint8_t config = (toc >> 3) & 0x1F;
    
    return (config >= 0 && config <= 31);
}

bool AudioFormatConverter::parseOpusHeader(const uint8_t* data, size_t size, OpusFrameHeader& header) {
    if (size < 1) {
        return false;
    }
    
    header.toc = data[0];
    header.has_padding = (data[0] & 0x04) != 0;
    
    uint8_t config = (data[0] >> 3) & 0x1F;
    
    if (config < 12) {
        header.frame_size = (config & 3) * 10 + 10;
    } else if (config < 16) {
        header.frame_size = (config & 1) * 20 + 20;
    } else {
        header.frame_size = (config & 3) * 60 + 60;
    }
    
    return true;
}

AudioStreamWriter::AudioStreamWriter()
    : format(AudioFormatType::RAW_PCM), file_handle(nullptr),
      samples_written(0), total_bytes(0), file_open(false) {
}

AudioStreamWriter::~AudioStreamWriter() {
    if (file_open) {
        closeFile();
    }
}

bool AudioStreamWriter::openFile(const char* filename, AudioFormatType fmt,
                               uint32_t sample_rate, uint8_t channels, uint8_t bit_depth) {
    format = fmt;
    samples_written = 0;
    total_bytes = 0;
    
    file_open = true;
    return true;
}

void AudioStreamWriter::closeFile() {
    if (file_open && file_handle) {
        fclose(file_handle);
        file_handle = nullptr;
    }
    file_open = false;
}

bool AudioStreamWriter::writeAudioData(const uint8_t* data, size_t size) {
    if (!file_open || !data) {
        return false;
    }
    
    total_bytes += size;
    samples_written += (size / 2);
    
    return true;
}

bool AudioStreamWriter::finalizeFile() {
    if (!file_open) {
        return false;
    }
    
    closeFile();
    return true;
}
