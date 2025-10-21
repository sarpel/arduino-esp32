"""
Unit tests for audio compression module.
"""

import pytest
import numpy as np
from unittest.mock import Mock, patch

# Import the module under test
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from audio_receiver.compression import (
    CompressionType, 
    AudioCompressor, 
    get_compressor,
    compress_audio_chunk,
    decompress_audio_chunk
)


@pytest.mark.unit
@pytest.mark.audio
class TestAudioCompressor:
    """Test cases for AudioCompressor class."""
    
    def setup_method(self):
        """Set up test fixtures."""
        self.compressor = AudioCompressor()
        self.sample_audio = np.random.randn(16000).astype(np.float32)  # 1 second at 16kHz
    
    def test_compressor_initialization(self):
        """Test compressor initialization."""
        assert self.compressor is not None
        assert self.compressor.default_compression == CompressionType.ZLIB
        assert self.compressor.compression_level == 6
        assert len(self.compressor.metrics_history) == 0
    
    def test_zlib_compression(self):
        """Test ZLIB compression and decompression."""
        compressed, metrics = self.compressor.compress_audio(
            self.sample_audio, CompressionType.ZLIB
        )
        
        assert compressed is not None
        assert len(compressed) > 0
        assert metrics.compression_ratio > 1.0
        assert metrics.original_size == self.sample_audio.nbytes
        assert metrics.compressed_size == len(compressed)
        
        # Test decompression
        decompressed = self.compressor.decompress_audio(
            compressed, CompressionType.ZLIB, self.sample_audio.shape
        )
        
        np.testing.assert_array_almost_equal(decompressed, self.sample_audio, decimal=5)
    
    def test_gzip_compression(self):
        """Test GZIP compression and decompression."""
        compressed, metrics = self.compressor.compress_audio(
            self.sample_audio, CompressionType.GZIP
        )
        
        assert compressed is not None
        assert len(compressed) > 0
        assert metrics.compression_ratio > 1.0
        
        # Test decompression
        decompressed = self.compressor.decompress_audio(
            compressed, CompressionType.GZIP, self.sample_audio.shape
        )
        
        np.testing.assert_array_almost_equal(decompressed, self.sample_audio, decimal=5)
    
    def test_adpcm_compression(self):
        """Test ADPCM compression and decompression."""
        # Convert to int16 for ADPCM
        audio_int16 = (self.sample_audio * 32767).astype(np.int16)
        
        compressed, metrics = self.compressor.compress_audio(
            audio_int16, CompressionType.ADPCM
        )
        
        assert compressed is not None
        assert len(compressed) > 0
        assert metrics.compression_ratio > 1.0
        
        # Test decompression
        decompressed = self.compressor.decompress_audio(
            compressed, CompressionType.ADPCM, audio_int16.shape, np.int16
        )
        
        # ADPCM is lossy, so we check similarity rather than exact match
        correlation = np.corrcoef(audio_int16.flatten(), decompressed.flatten())[0, 1]
        assert correlation > 0.9  # High correlation expected
    
    def test_none_compression(self):
        """Test no compression (passthrough)."""
        compressed, metrics = self.compressor.compress_audio(
            self.sample_audio, CompressionType.NONE
        )
        
        assert compressed == self.sample_audio.tobytes()
        assert metrics.compression_ratio == 1.0
        
        # Test decompression
        decompressed = self.compressor.decompress_audio(
            compressed, CompressionType.NONE, self.sample_audio.shape
        )
        
        np.testing.assert_array_equal(decompressed, self.sample_audio)
    
    def test_quality_score_calculation(self):
        """Test audio quality score calculation."""
        compressed, _ = self.compressor.compress_audio(
            self.sample_audio, CompressionType.ZLIB
        )
        
        # Quality should be high for lossless compression
        quality = self.compressor._calculate_quality_score(
            self.sample_audio, compressed, CompressionType.ZLIB
        )
        
        assert 0.0 <= quality <= 1.0
        assert quality > 0.9  # Should be very high for lossless
    
    def test_optimal_compression_selection(self):
        """Test optimal compression type selection."""
        optimal = self.compressor.get_optimal_compression(self.sample_audio)
        
        assert optimal in CompressionType
        assert optimal != CompressionType.NONE  # Should recommend some compression
    
    def test_compression_stats(self):
        """Test compression statistics collection."""
        # Perform some compressions
        for _ in range(5):
            self.compressor.compress_audio(self.sample_audio, CompressionType.ZLIB)
        
        stats = self.compressor.get_compression_stats()
        
        assert 'total_compressions' in stats
        assert 'average_compression_ratio' in stats
        assert 'average_quality_score' in stats
        assert stats['total_compressions'] == 5
        assert stats['average_compression_ratio'] > 1.0
    
    def test_invalid_compression_type(self):
        """Test handling of invalid compression type."""
        with pytest.raises(ValueError):
            self.compressor.compress_audio(self.sample_audio, "invalid_type")
    
    def test_empty_audio_data(self):
        """Test compression of empty audio data."""
        empty_audio = np.array([], dtype=np.float32)
        
        compressed, metrics = self.compressor.compress_audio(
            empty_audio, CompressionType.ZLIB
        )
        
        assert compressed is not None
        assert metrics.original_size == 0
        assert metrics.compressed_size == 0


@pytest.mark.unit
@pytest.mark.audio
class TestCompressionUtilities:
    """Test cases for compression utility functions."""
    
    def test_get_compressor_singleton(self):
        """Test that get_compressor returns the same instance."""
        compressor1 = get_compressor()
        compressor2 = get_compressor()
        
        assert compressor1 is compressor2
    
    def test_compress_audio_chunk_function(self):
        """Test compress_audio_chunk convenience function."""
        audio_data = np.random.randn(8000).astype(np.float32)
        
        compressed, metrics = compress_audio_chunk(audio_data, CompressionType.ZLIB)
        
        assert compressed is not None
        assert metrics is not None
        assert isinstance(metrics, object)
    
    def test_decompress_audio_chunk_function(self):
        """Test decompress_audio_chunk convenience function."""
        audio_data = np.random.randn(8000).astype(np.float32)
        
        # First compress
        compressed, _ = compress_audio_chunk(audio_data, CompressionType.ZLIB)
        
        # Then decompress
        decompressed = decompress_audio_chunk(
            compressed, CompressionType.ZLIB, audio_data.shape
        )
        
        np.testing.assert_array_almost_equal(decompressed, audio_data, decimal=5)


@pytest.mark.unit
@pytest.mark.audio
class TestCompressionPerformance:
    """Performance tests for audio compression."""
    
    def setup_method(self):
        """Set up performance test fixtures."""
        self.compressor = AudioCompressor()
        # Larger audio sample for performance testing
        self.large_audio = np.random.randn(160000).astype(np.float32)  # 10 seconds
    
    @pytest.mark.slow
    def test_compression_speed(self):
        """Test compression performance."""
        import time
        
        start_time = time.time()
        compressed, metrics = self.compressor.compress_audio(
            self.large_audio, CompressionType.ZLIB
        )
        compression_time = time.time() - start_time
        
        # Should compress within reasonable time (adjust threshold as needed)
        assert compression_time < 1.0  # 1 second max for 10 seconds of audio
        assert metrics.compression_time == compression_time
    
    @pytest.mark.slow
    def test_decompression_speed(self):
        """Test decompression performance."""
        import time
        
        # First compress
        compressed, _ = self.compressor.compress_audio(
            self.large_audio, CompressionType.ZLIB
        )
        
        # Then time decompression
        start_time = time.time()
        decompressed = self.compressor.decompress_audio(
            compressed, CompressionType.ZLIB, self.large_audio.shape
        )
        decompression_time = time.time() - start_time
        
        # Should decompress within reasonable time
        assert decompression_time < 0.5  # 0.5 seconds max
    
    def test_memory_usage(self):
        """Test memory efficiency of compression."""
        import psutil
        import os
        
        process = psutil.Process(os.getpid())
        initial_memory = process.memory_info().rss
        
        # Perform multiple compressions
        for _ in range(10):
            compressed, _ = self.compressor.compress_audio(
                self.large_audio, CompressionType.ZLIB
            )
            del compressed  # Explicit cleanup
        
        final_memory = process.memory_info().rss
        memory_increase = final_memory - initial_memory
        
        # Memory increase should be reasonable (less than 100MB)
        assert memory_increase < 100 * 1024 * 1024


@pytest.mark.unit
@pytest.mark.audio
class TestCompressionEdgeCases:
    """Test edge cases and error handling."""
    
    def setup_method(self):
        """Set up edge case test fixtures."""
        self.compressor = AudioCompressor()
    
    def test_single_sample_audio(self):
        """Test compression of single sample."""
        single_sample = np.array([0.5], dtype=np.float32)
        
        compressed, metrics = self.compressor.compress_audio(
            single_sample, CompressionType.ZLIB
        )
        
        assert compressed is not None
        assert metrics.original_size == single_sample.nbytes
    
    def test_constant_audio(self):
        """Test compression of constant (silent) audio."""
        silent_audio = np.zeros(16000, dtype=np.float32)
        
        compressed, metrics = self.compressor.compress_audio(
            silent_audio, CompressionType.ZLIB
        )
        
        # Should compress very well
        assert metrics.compression_ratio > 10.0
    
    def test_max_amplitude_audio(self):
        """Test compression of maximum amplitude audio."""
        max_audio = np.ones(16000, dtype=np.float32)
        
        compressed, metrics = self.compressor.compress_audio(
            max_audio, CompressionType.ZLIB
        )
        
        assert compressed is not None
        assert metrics.compression_ratio > 1.0
    
    def test_nan_inf_audio(self):
        """Test handling of NaN and infinite values."""
        problematic_audio = np.array([1.0, np.nan, np.inf, -np.inf, 0.0], dtype=np.float32)
        
        # Should handle gracefully without crashing
        try:
            compressed, metrics = self.compressor.compress_audio(
                problematic_audio, CompressionType.ZLIB
            )
            # If successful, check basic properties
            assert compressed is not None
        except (ValueError, OverflowError):
            # Expected to fail with NaN/Inf values
            pass
    
    def test_different_dtypes(self):
        """Test compression with different audio data types."""
        dtypes = [np.float32, np.float64, np.int16, np.int32]
        
        for dtype in dtypes:
            if dtype in [np.int16, np.int32]:
                audio_data = (np.random.randn(16000) * 32767).astype(dtype)
            else:
                audio_data = np.random.randn(16000).astype(dtype)
            
            compressed, metrics = self.compressor.compress_audio(
                audio_data, CompressionType.ZLIB
            )
            
            assert compressed is not None
            assert metrics.compression_ratio > 1.0


if __name__ == "__main__":
    pytest.main([__file__])