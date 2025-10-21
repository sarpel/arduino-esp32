"""
Audio Compression Module

Handles various audio compression algorithms and optimization strategies
for reducing bandwidth usage while maintaining audio quality.
"""

import zlib
import gzip
import pickle
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass
from enum import Enum
import numpy as np
from scipy import signal
from scipy.fft import dct, idct
from scipy.io import wavfile

from ..core.logger import get_logger
from ..core.config import get_config

logger = get_logger(__name__)
config = get_config()


class CompressionType(Enum):
    """Supported compression types."""
    NONE = "none"
    ZLIB = "zlib"
    GZIP = "gzip"
    ADPCM = "adpcm"
    DCT = "dct"
    LPC = "lpc"


@dataclass
class CompressionMetrics:
    """Compression performance metrics."""
    original_size: int
    compressed_size: int
    compression_ratio: float
    compression_time: float
    decompression_time: float
    quality_score: float


class AudioCompressor:
    """Advanced audio compression system with multiple algorithms."""
    
    def __init__(self):
        self.compression_cache: Dict[str, Any] = {}
        self.metrics_history: List[CompressionMetrics] = []
        
        # Compression settings from config
        self.default_compression = getattr(config, 'DEFAULT_COMPRESSION', CompressionType.ZLIB)
        self.compression_level = getattr(config, 'COMPRESSION_LEVEL', 6)
        self.quality_threshold = getattr(config, 'AUDIO_QUALITY_THRESHOLD', 0.8)
        
        logger.info(f"AudioCompressor initialized with {self.default_compression.value}")
    
    def compress_audio(self, audio_data: np.ndarray, 
                      compression_type: Optional[CompressionType] = None,
                      sample_rate: int = 44100) -> Tuple[bytes, CompressionMetrics]:
        """
        Compress audio data using specified algorithm.
        
        Args:
            audio_data: Raw audio data as numpy array
            compression_type: Type of compression to use
            sample_rate: Audio sample rate
            
        Returns:
            Tuple of (compressed_data, metrics)
        """
        import time
        start_time = time.time()
        
        if compression_type is None:
            compression_type = self.default_compression
        
        original_size = audio_data.nbytes
        
        try:
            if compression_type == CompressionType.NONE:
                compressed_data = audio_data.tobytes()
            elif compression_type == CompressionType.ZLIB:
                compressed_data = self._compress_zlib(audio_data)
            elif compression_type == CompressionType.GZIP:
                compressed_data = self._compress_gzip(audio_data)
            elif compression_type == CompressionType.ADPCM:
                compressed_data = self._compress_adpcm(audio_data)
            elif compression_type == CompressionType.DCT:
                compressed_data = self._compress_dct(audio_data)
            elif compression_type == CompressionType.LPC:
                compressed_data = self._compress_lpc(audio_data)
            else:
                raise ValueError(f"Unsupported compression type: {compression_type}")
            
            compression_time = time.time() - start_time
            compressed_size = len(compressed_data)
            compression_ratio = original_size / compressed_size if compressed_size > 0 else 1.0
            
            # Calculate quality score
            quality_score = self._calculate_quality_score(audio_data, compressed_data, compression_type)
            
            metrics = CompressionMetrics(
                original_size=original_size,
                compressed_size=compressed_size,
                compression_ratio=compression_ratio,
                compression_time=compression_time,
                decompression_time=0.0,  # Will be calculated on decompression
                quality_score=quality_score
            )
            
            self.metrics_history.append(metrics)
            
            logger.debug(f"Audio compressed: {compression_type.value}, ratio: {compression_ratio:.2f}x, quality: {quality_score:.2f}")
            
            return compressed_data, metrics
            
        except Exception as e:
            logger.error(f"Audio compression failed: {e}")
            raise
    
    def decompress_audio(self, compressed_data: bytes, 
                        compression_type: CompressionType,
                        original_shape: Tuple[int, ...],
                        dtype: np.dtype = np.float32) -> np.ndarray:
        """
        Decompress audio data.
        
        Args:
            compressed_data: Compressed audio data
            compression_type: Type of compression used
            original_shape: Original array shape
            dtype: Original data type
            
        Returns:
            Decompressed audio data
        """
        import time
        start_time = time.time()
        
        try:
            if compression_type == CompressionType.NONE:
                audio_data = np.frombuffer(compressed_data, dtype=dtype).reshape(original_shape)
            elif compression_type == CompressionType.ZLIB:
                audio_data = self._decompress_zlib(compressed_data, original_shape, dtype)
            elif compression_type == CompressionType.GZIP:
                audio_data = self._decompress_gzip(compressed_data, original_shape, dtype)
            elif compression_type == CompressionType.ADPCM:
                audio_data = self._decompress_adpcm(compressed_data, original_shape, dtype)
            elif compression_type == CompressionType.DCT:
                audio_data = self._decompress_dct(compressed_data, original_shape, dtype)
            elif compression_type == CompressionType.LPC:
                audio_data = self._decompress_lpc(compressed_data, original_shape, dtype)
            else:
                raise ValueError(f"Unsupported compression type: {compression_type}")
            
            decompression_time = time.time() - start_time
            
            # Update metrics with decompression time
            if self.metrics_history:
                self.metrics_history[-1].decompression_time = decompression_time
            
            logger.debug(f"Audio decompressed in {decompression_time:.3f}s")
            
            return audio_data
            
        except Exception as e:
            logger.error(f"Audio decompression failed: {e}")
            raise
    
    def _compress_zlib(self, audio_data: np.ndarray) -> bytes:
        """Compress using zlib."""
        return zlib.compress(audio_data.tobytes(), level=self.compression_level)
    
    def _decompress_zlib(self, compressed_data: bytes, original_shape: Tuple[int, ...], dtype: np.dtype) -> np.ndarray:
        """Decompress zlib data."""
        decompressed = zlib.decompress(compressed_data)
        return np.frombuffer(decompressed, dtype=dtype).reshape(original_shape)
    
    def _compress_gzip(self, audio_data: np.ndarray) -> bytes:
        """Compress using gzip."""
        return gzip.compress(audio_data.tobytes(), compresslevel=self.compression_level)
    
    def _decompress_gzip(self, compressed_data: bytes, original_shape: Tuple[int, ...], dtype: np.dtype) -> np.ndarray:
        """Decompress gzip data."""
        decompressed = gzip.decompress(compressed_data)
        return np.frombuffer(decompressed, dtype=dtype).reshape(original_shape)
    
    def _compress_adpcm(self, audio_data: np.ndarray) -> bytes:
        """Compress using Adaptive Differential Pulse-Code Modulation."""
        # Simple ADPCM implementation
        if audio_data.dtype != np.int16:
            # Convert to int16 for ADPCM
            audio_data = (audio_data * 32767).astype(np.dtype('int16'))
        
        # ADPCM encoding (simplified)
        encoded = []
        predictor = 0
        step = 1
        
        for sample in audio_data.flatten():
            diff = sample - predictor
            code = int(diff / step)
            code = max(-8, min(7, code))  # 4-bit code
            
            predictor += code * step
            predictor = max(-32768, min(32767, predictor))
            
            # Adaptive step size
            if abs(diff) > step * 2:
                step = min(step * 2, 32768)
            elif abs(diff) < step / 2:
                step = max(step // 2, 1)
            
            encoded.append(code & 0x0F)
        
        # Pack 4-bit codes into bytes
        packed = []
        for i in range(0, len(encoded), 2):
            if i + 1 < len(encoded):
                packed.append((encoded[i] << 4) | encoded[i + 1])
            else:
                packed.append(encoded[i] << 4)
        
        return bytes(packed)
    
    def _decompress_adpcm(self, compressed_data: bytes, original_shape: Tuple[int, ...], dtype: np.dtype) -> np.ndarray:
        """Decompress ADPCM data."""
        # Unpack 4-bit codes
        encoded = []
        for byte in compressed_data:
            encoded.append((byte >> 4) & 0x0F)
            encoded.append(byte & 0x0F)
        
        # ADPCM decoding
        decoded = []
        predictor = 0
        step = 1
        
        for code in encoded[:np.prod(original_shape)]:
            # Convert 4-bit signed code
            if code >= 8:
                code -= 16
            
            sample = predictor + code * step
            sample = max(-32768, min(32767, sample))
            decoded.append(sample)
            
            # Adaptive step size
            if abs(code) > 2:
                step = min(step * 2, 32768)
            elif abs(code) == 0:
                step = max(step // 2, 1)
            
            predictor = sample
        
        audio_data = np.array(decoded, dtype=np.int16).reshape(original_shape)
        
        # Convert back to original dtype
        if dtype == np.float32:
            audio_data = audio_data.astype(np.float32) / 32767.0
        
        return audio_data
    
    def _compress_dct(self, audio_data: np.ndarray) -> bytes:
        """Compress using Discrete Cosine Transform."""
        # Apply DCT
        dct_data = dct(audio_data.flatten(), type=2, norm='ortho')
        
        # Quantize (keep only significant coefficients)
        threshold = np.percentile(np.abs(dct_data), 90)  # Keep top 10%
        dct_data[np.abs(dct_data) < threshold] = 0
        
        # Serialize
        return pickle.dumps(dct_data)
    
    def _decompress_dct(self, compressed_data: bytes, original_shape: Tuple[int, ...], dtype: np.dtype) -> np.ndarray:
        """Decompress DCT data."""
        dct_data = pickle.loads(compressed_data)
        
        # Inverse DCT
        audio_data = idct(dct_data, type=2, norm='ortho')
        audio_data = audio_data.reshape(original_shape).astype(dtype)
        
        return audio_data
    
    def _compress_lpc(self, audio_data: np.ndarray) -> bytes:
        """Compress using Linear Predictive Coding."""
        # Simple LPC implementation
        if len(audio_data.shape) > 1:
            audio_data = audio_data.flatten()
        
        # Calculate LPC coefficients (order 10)
        order = min(10, len(audio_data) // 4)
        if order < 2:
            return audio_data.tobytes()
        
        # Autocorrelation
        autocorr = np.correlate(audio_data, audio_data, mode='full')
        autocorr = autocorr[len(autocorr)//2:]
        
        # Levinson-Durbin algorithm
        lpc_coeffs = self._levinson_durbin(autocorr[:order+1])
        
        # Calculate residual
        residual = signal.lfilter(lpc_coeffs, [1], audio_data)
        
        # Serialize coefficients and residual
        data = {
            'coeffs': lpc_coeffs,
            'residual': residual,
            'shape': audio_data.shape
        }
        
        return pickle.dumps(data)
    
    def _decompress_lpc(self, compressed_data: bytes, original_shape: Tuple[int, ...], dtype: np.dtype) -> np.ndarray:
        """Decompress LPC data."""
        data = pickle.loads(compressed_data)
        lpc_coeffs = data['coeffs']
        residual = data['residual']
        
        # Reconstruct signal
        audio_data = signal.lfilter([1], lpc_coeffs, residual)
        # Ensure we have the right shape for 1D data
        if len(original_shape) == 1:
            audio_data = audio_data[:original_shape[0]]
        else:
            audio_data = audio_data.reshape(original_shape)
        audio_data = audio_data.astype(dtype)
        
        return audio_data
    
    def _levinson_durbin(self, autocorr: np.ndarray) -> np.ndarray:
        """Levinson-Durbin algorithm for LPC coefficient calculation."""
        n = len(autocorr)
        lpc_coeffs = np.zeros(n)
        error = autocorr[0]
        
        for i in range(1, n):
            reflection = -sum(lpc_coeffs[j] * autocorr[i-j] for j in range(i)) / error
            
            lpc_coeffs[i] = reflection
            for j in range(i-1, 0, -1):
                lpc_coeffs[j] += reflection * lpc_coeffs[i-j]
            
            error *= (1 - reflection * reflection)
        
        lpc_coeffs[0] = 1.0
        return lpc_coeffs
    
    def _calculate_quality_score(self, original: np.ndarray, compressed: bytes, 
                                compression_type: CompressionType) -> float:
        """Calculate audio quality score after compression."""
        try:
            # Decompress to compare
            decompressed = self.decompress_audio(compressed, compression_type, original.shape, original.dtype)
            
            # Calculate Signal-to-Noise Ratio
            signal_power = np.mean(original ** 2)
            noise_power = np.mean((original - decompressed) ** 2)
            snr = 10 * np.log10(signal_power / noise_power) if noise_power > 0 else 100.0
            
            # Normalize to 0-1 scale (assuming SNR > 20dB is good quality)
            quality_score = min(1.0, max(0.0, (snr - 20) / 80))
            
            return quality_score
            
        except Exception as e:
            logger.warning(f"Quality calculation failed: {e}")
            return 0.5  # Default medium quality
    
    def get_optimal_compression(self, audio_data: np.ndarray, 
                               target_quality: Optional[float] = None) -> CompressionType:
        """
        Determine optimal compression type based on audio characteristics.
        
        Args:
            audio_data: Audio data to analyze
            target_quality: Minimum acceptable quality (0-1)
            
        Returns:
            Recommended compression type
        """
        if target_quality is None:
            target_quality = self.quality_threshold
        
        # Analyze audio characteristics
        audio_variance = np.var(audio_data)
        audio_energy = np.sum(audio_data ** 2)
        
        # Test compression types
        candidates = [CompressionType.ZLIB, CompressionType.GZIP, CompressionType.ADPCM]
        
        best_compression = CompressionType.ZLIB
        best_ratio = 0
        
        for comp_type in candidates:
            try:
                compressed, metrics = self.compress_audio(audio_data, comp_type)
                
                if metrics.quality_score >= target_quality and metrics.compression_ratio > best_ratio:
                    best_compression = comp_type
                    best_ratio = metrics.compression_ratio
                    
            except Exception as e:
                logger.warning(f"Compression test failed for {comp_type.value}: {e}")
                continue
        
        return best_compression
    
    def get_compression_stats(self) -> Dict[str, Any]:
        """Get compression performance statistics."""
        if not self.metrics_history:
            return {}
        
        recent_metrics = self.metrics_history[-100:]  # Last 100 operations
        
        return {
            'total_compressions': len(self.metrics_history),
            'average_compression_ratio': np.mean([m.compression_ratio for m in recent_metrics]),
            'average_quality_score': np.mean([m.quality_score for m in recent_metrics]),
            'average_compression_time': np.mean([m.compression_time for m in recent_metrics]),
            'average_decompression_time': np.mean([m.decompression_time for m in recent_metrics]),
            'bandwidth_saved': sum(m.original_size - m.compressed_size for m in recent_metrics)
        }


# Global compressor instance
_compressor_instance: Optional[AudioCompressor] = None


def get_compressor() -> AudioCompressor:
    """Get global audio compressor instance."""
    global _compressor_instance
    if _compressor_instance is None:
        _compressor_instance = AudioCompressor()
    return _compressor_instance


def compress_audio_chunk(audio_data: np.ndarray, 
                        compression_type: Optional[CompressionType] = None) -> Tuple[bytes, CompressionMetrics]:
    """Convenience function to compress audio chunk."""
    compressor = get_compressor()
    return compressor.compress_audio(audio_data, compression_type)


def decompress_audio_chunk(compressed_data: bytes, 
                          compression_type: CompressionType,
                          original_shape: Tuple[int, ...]) -> np.ndarray:
    """Convenience function to decompress audio chunk."""
    compressor = get_compressor()
    return compressor.decompress_audio(compressed_data, compression_type, original_shape)