"""
Audio processing pipeline for real-time audio enhancement and analysis.
"""

import numpy as np
import threading
import time
from datetime import datetime
from typing import Dict, List, Optional, Callable, Any, Tuple
from dataclasses import dataclass, field
from enum import Enum
import json

from .server import AudioChunk, AudioFormat
from ..core.logger import get_logger, LogContext
from ..core.events import get_event_bus
from ..core.config import get_config


class ProcessingStage(Enum):
    """Audio processing stages."""
    INPUT = "input"
    FILTERING = "filtering"
    ENHANCEMENT = "enhancement"
    ANALYSIS = "analysis"
    OUTPUT = "output"


@dataclass
class AudioMetrics:
    """Audio quality metrics."""
    rms_level: float = 0.0
    peak_level: float = 0.0
    zero_crossing_rate: float = 0.0
    spectral_centroid: float = 0.0
    spectral_rolloff: float = 0.0
    mfcc_features: List[float] = field(default_factory=list)
    snr_estimate: float = 0.0
    voice_activity_probability: float = 0.0
    timestamp: float = field(default_factory=time.time)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            'rms_level': self.rms_level,
            'peak_level': self.peak_level,
            'zero_crossing_rate': self.zero_crossing_rate,
            'spectral_centroid': self.spectral_centroid,
            'spectral_rolloff': self.spectral_rolloff,
            'mfcc_features': self.mfcc_features,
            'snr_estimate': self.snr_estimate,
            'voice_activity_probability': self.voice_activity_probability,
            'timestamp': self.timestamp
        }


@dataclass
class ProcessingResult:
    """Result of audio processing."""
    device_id: str
    original_chunk: AudioChunk
    processed_data: Optional[bytes] = None
    metrics: AudioMetrics = field(default_factory=AudioMetrics)
    processing_time: float = 0.0
    stages_completed: List[ProcessingStage] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            'device_id': self.device_id,
            'chunk_size': len(self.original_chunk.data) if self.original_chunk.data else 0,
            'processed_size': len(self.processed_data) if self.processed_data else 0,
            'metrics': self.metrics.to_dict(),
            'processing_time_ms': self.processing_time * 1000,
            'stages_completed': [stage.value for stage in self.stages_completed],
            'errors': self.errors,
            'metadata': self.metadata
        }


class AudioFilter:
    """Base class for audio filters."""
    
    def __init__(self, name: str):
        self.name = name
        self.enabled = True
        self.logger = get_logger(f"audio.filter.{name}")
    
    def process(self, audio_data: np.ndarray, sample_rate: int) -> np.ndarray:
        """
        Process audio data.
        
        Args:
            audio_data: Audio data as numpy array
            sample_rate: Sample rate
            
        Returns:
            Processed audio data
        """
        if not self.enabled:
            return audio_data
        
        return self._process(audio_data, sample_rate)
    
    def _process(self, audio_data: np.ndarray, sample_rate: int) -> np.ndarray:
        """Override in subclasses."""
        return audio_data


class HighPassFilter(AudioFilter):
    """High-pass filter to remove low-frequency noise."""
    
    def __init__(self, cutoff_freq: float = 80.0, order: int = 5):
        super().__init__("high_pass")
        self.cutoff_freq = cutoff_freq
        self.order = order
        self._coefficients = None
    
    def _process(self, audio_data: np.ndarray, sample_rate: int) -> np.ndarray:
        """Apply high-pass filter."""
        try:
            from scipy import signal
            
            # Calculate filter coefficients if not cached
            if self._coefficients is None:
                nyquist = sample_rate / 2
                normalized_cutoff = self.cutoff_freq / nyquist
                self._coefficients = signal.butter(
                    self.order, normalized_cutoff, btype='high'
                )
            
            # Apply filter
            b, a = self._coefficients
            filtered_data = signal.filtfilt(b, a, audio_data)
            
            return filtered_data
            
        except ImportError:
            self.logger.warning("scipy not available, skipping high-pass filter")
            return audio_data
        except Exception as e:
            self.logger.error(f"High-pass filter failed: {e}")
            return audio_data


class LowPassFilter(AudioFilter):
    """Low-pass filter to remove high-frequency noise."""
    
    def __init__(self, cutoff_freq: float = 8000.0, order: int = 5):
        super().__init__("low_pass")
        self.cutoff_freq = cutoff_freq
        self.order = order
        self._coefficients = None
    
    def _process(self, audio_data: np.ndarray, sample_rate: int) -> np.ndarray:
        """Apply low-pass filter."""
        try:
            from scipy import signal
            
            # Calculate filter coefficients if not cached
            if self._coefficients is None:
                nyquist = sample_rate / 2
                normalized_cutoff = self.cutoff_freq / nyquist
                self._coefficients = signal.butter(
                    self.order, normalized_cutoff, btype='low'
                )
            
            # Apply filter
            b, a = self._coefficients
            filtered_data = signal.filtfilt(b, a, audio_data)
            
            return filtered_data
            
        except ImportError:
            self.logger.warning("scipy not available, skipping low-pass filter")
            return audio_data
        except Exception as e:
            self.logger.error(f"Low-pass filter failed: {e}")
            return audio_data


class NoiseGate(AudioFilter):
    """Noise gate to suppress low-level noise."""
    
    def __init__(self, threshold: float = 0.01, ratio: float = 10.0, 
                 attack_time: float = 0.01, release_time: float = 0.1):
        super().__init__("noise_gate")
        self.threshold = threshold
        self.ratio = ratio
        self.attack_time = attack_time
        self.release_time = release_time
        self._envelope = 0.0
    
    def _process(self, audio_data: np.ndarray, sample_rate: int) -> np.ndarray:
        """Apply noise gate."""
        try:
            # Calculate attack and release coefficients
            attack_coeff = np.exp(-1.0 / (self.attack_time * sample_rate))
            release_coeff = np.exp(-1.0 / (self.release_time * sample_rate))
            
            processed_data = np.zeros_like(audio_data)
            
            for i, sample in enumerate(audio_data):
                # Update envelope
                sample_abs = abs(sample)
                if sample_abs > self._envelope:
                    self._envelope = attack_coeff * self._envelope + (1 - attack_coeff) * sample_abs
                else:
                    self._envelope = release_coeff * self._envelope
                
                # Apply gain reduction with epsilon protection to prevent division by zero
                if self._envelope < self.threshold:
                    denom = max(self._envelope, 1e-8)  # Prevent division by zero
                    gain = 1.0 / (1.0 + self.ratio * (self.threshold - self._envelope) / denom)
                else:
                    gain = 1.0
                
                processed_data[i] = sample * gain
            
            return processed_data
            
        except Exception as e:
            self.logger.error(f"Noise gate failed: {e}")
            return audio_data


class AudioAnalyzer:
    """Audio analysis and feature extraction."""
    
    def __init__(self):
        self.logger = get_logger("audio.analyzer")
    
    def analyze(self, audio_data: np.ndarray, sample_rate: int) -> AudioMetrics:
        """
        Analyze audio data and extract metrics.
        
        Args:
            audio_data: Audio data as numpy array
            sample_rate: Sample rate
            
        Returns:
            Audio metrics
        """
        metrics = AudioMetrics()
        
        try:
            # Basic metrics
            metrics.rms_level = np.sqrt(np.mean(audio_data ** 2))
            metrics.peak_level = np.max(np.abs(audio_data))
            
            # Zero crossing rate
            zero_crossings = np.where(np.diff(np.sign(audio_data)))[0]
            metrics.zero_crossing_rate = len(zero_crossings) / len(audio_data)
            
            # Spectral features
            metrics.spectral_centroid = self._calculate_spectral_centroid(audio_data, sample_rate)
            metrics.spectral_rolloff = self._calculate_spectral_rolloff(audio_data, sample_rate)
            
            # SNR estimation
            metrics.snr_estimate = self._estimate_snr(audio_data)
            
            # Voice activity detection
            metrics.voice_activity_probability = self._detect_voice_activity(audio_data, sample_rate)
            
            # MFCC features (if available)
            try:
                metrics.mfcc_features = self._extract_mfcc(audio_data, sample_rate)
            except Exception:
                pass  # MFCC extraction is optional
            
        except Exception as e:
            self.logger.error(f"Audio analysis failed: {e}")
        
        return metrics
    
    def _calculate_spectral_centroid(self, audio_data: np.ndarray, sample_rate: int) -> float:
        """Calculate spectral centroid."""
        try:
            fft = np.fft.fft(audio_data)
            magnitude = np.abs(fft[:len(fft)//2])
            frequencies = np.linspace(0, sample_rate/2, len(magnitude))
            
            if np.sum(magnitude) > 0:
                return np.sum(frequencies * magnitude) / np.sum(magnitude)
            else:
                return 0.0
        except Exception:
            return 0.0
    
    def _calculate_spectral_rolloff(self, audio_data: np.ndarray, sample_rate: int) -> float:
        """Calculate spectral rolloff (frequency below which 85% of energy is contained)."""
        try:
            fft = np.fft.fft(audio_data)
            magnitude = np.abs(fft[:len(fft)//2])
            frequencies = np.linspace(0, sample_rate/2, len(magnitude))
            
            cumulative_energy = np.cumsum(magnitude)
            total_energy = cumulative_energy[-1]
            
            if total_energy > 0:
                rolloff_point = 0.85 * total_energy
                rolloff_index = np.where(cumulative_energy >= rolloff_point)[0]
                if len(rolloff_index) > 0:
                    return frequencies[rolloff_index[0]]
            
            return sample_rate / 2
        except Exception:
            return sample_rate / 2
    
    def _estimate_snr(self, audio_data: np.ndarray) -> float:
        """Estimate signal-to-noise ratio."""
        try:
            # Simple SNR estimation using signal vs noise floor
            signal_level = np.percentile(np.abs(audio_data), 90)
            noise_level = np.percentile(np.abs(audio_data), 10)
            
            if noise_level > 0:
                return 20 * np.log10(signal_level / noise_level)
            else:
                return 60.0  # High SNR for very quiet noise floor
        except Exception:
            return 0.0
    
    def _detect_voice_activity(self, audio_data: np.ndarray, sample_rate: int) -> float:
        """Detect voice activity probability."""
        try:
            # Simple VAD based on energy and zero crossing rate
            energy = np.sum(audio_data ** 2)
            zcr = self._calculate_zero_crossing_rate(audio_data)
            
            # Typical speech characteristics
            energy_threshold = 0.001
            zcr_min, zcr_max = 0.1, 0.5
            
            # Calculate probabilities
            energy_prob = 1.0 if energy > energy_threshold else energy / energy_threshold
            zcr_prob = 1.0 if zcr_min <= zcr <= zcr_max else 0.0
            
            # Combined probability
            return (energy_prob + zcr_prob) / 2.0
        except Exception:
            return 0.0
    
    def _calculate_zero_crossing_rate(self, audio_data: np.ndarray) -> float:
        """Calculate zero crossing rate."""
        zero_crossings = np.where(np.diff(np.sign(audio_data)))[0]
        return len(zero_crossings) / len(audio_data)
    
    def _extract_mfcc(self, audio_data: np.ndarray, sample_rate: int, n_mfcc: int = 13) -> List[float]:
        """Extract MFCC features."""
        try:
            import librosa
            
            # Extract MFCC features
            mfccs = librosa.feature.mfcc(y=audio_data, sr=sample_rate, n_mfcc=n_mfcc)
            
            # Return mean of MFCC coefficients
            return mfccs.mean(axis=1).tolist()
        except ImportError:
            # librosa not available, return empty list
            return []
        except Exception:
            return []


class AudioProcessor:
    """
    Main audio processing pipeline with filters, enhancement, and analysis.
    """
    
    def __init__(self):
        self.logger = get_logger("audio.processor")
        self.event_bus = get_event_bus()
        self.config = get_config()
        
        # Processing components
        self.filters: List[AudioFilter] = []
        self.analyzer = AudioAnalyzer()
        
        # Processing statistics
        self.stats = {
            'chunks_processed': 0,
            'total_processing_time': 0.0,
            'average_processing_time': 0.0,
            'errors': 0
        }
        
        # Thread safety
        self._lock = threading.Lock()
        
        # Initialize default filters
        self._initialize_default_filters()
        
        self.logger.info("AudioProcessor initialized")
    
    def _initialize_default_filters(self) -> None:
        """Initialize default audio filters."""
        try:
            # High-pass filter to remove DC offset and low-frequency noise
            self.filters.append(HighPassFilter(cutoff_freq=80.0))
            
            # Low-pass filter to remove high-frequency noise
            self.filters.append(LowPassFilter(cutoff_freq=8000.0))
            
            # Noise gate
            self.filters.append(NoiseGate(threshold=0.01, ratio=10.0))
            
            self.logger.info(f"Default filters initialized: {[f.name for f in self.filters]}")
            
        except Exception as e:
            self.logger.error(f"Failed to initialize default filters: {e}")
    
    def add_filter(self, audio_filter: AudioFilter) -> None:
        """
        Add audio filter to processing pipeline.
        
        Args:
            audio_filter: Audio filter to add
        """
        with self._lock:
            self.filters.append(audio_filter)
            self.logger.info(f"Audio filter added: {audio_filter.name}")
    
    def remove_filter(self, filter_name: str) -> bool:
        """
        Remove audio filter by name.
        
        Args:
            filter_name: Name of filter to remove
            
        Returns:
            True if filter was removed
        """
        with self._lock:
            for i, filter_obj in enumerate(self.filters):
                if filter_obj.name == filter_name:
                    removed_filter = self.filters.pop(i)
                    self.logger.info(f"Audio filter removed: {filter_name}")
                    return True
        return False
    
    def get_filter(self, filter_name: str) -> Optional[AudioFilter]:
        """
        Get audio filter by name.
        
        Args:
            filter_name: Name of filter
            
        Returns:
            Audio filter or None
        """
        with self._lock:
            for filter_obj in self.filters:
                if filter_obj.name == filter_name:
                    return filter_obj
        return None
    
    def enable_filter(self, filter_name: str, enabled: bool = True) -> bool:
        """
        Enable or disable audio filter.
        
        Args:
            filter_name: Name of filter
            enabled: Enable state
            
        Returns:
            True if filter state was changed
        """
        filter_obj = self.get_filter(filter_name)
        if filter_obj:
            filter_obj.enabled = enabled
            self.logger.info(f"Audio filter '{filter_name}' {'enabled' if enabled else 'disabled'}")
            return True
        return False
    
    def process_chunk(self, chunk: AudioChunk) -> ProcessingResult:
        """
        Process audio chunk through the pipeline.
        
        Args:
            chunk: Audio chunk to process
            
        Returns:
            Processing result
        """
        start_time = time.time()
        
        result = ProcessingResult(
            device_id=chunk.device_id,
            original_chunk=chunk
        )
        
        try:
            # Convert audio data to numpy array
            audio_data = self._chunk_to_numpy(chunk)
            
            if audio_data is None:
                result.errors.append("Failed to convert audio chunk to numpy array")
                return result
            
            # Stage 1: Input validation
            result.stages_completed.append(ProcessingStage.INPUT)
            
            # Stage 2: Filtering
            filtered_data = self._apply_filters(audio_data, self.config.audio_receiver.sample_rate)
            result.stages_completed.append(ProcessingStage.FILTERING)
            
            # Stage 3: Enhancement (could include AGC, echo cancellation, etc.)
            enhanced_data = self._enhance_audio(filtered_data)
            result.stages_completed.append(ProcessingStage.ENHANCEMENT)
            
            # Stage 4: Analysis
            result.metrics = self.analyzer.analyze(enhanced_data, self.config.audio_receiver.sample_rate)
            result.stages_completed.append(ProcessingStage.ANALYSIS)
            
            # Stage 5: Output conversion
            result.processed_data = self._numpy_to_chunk(enhanced_data, chunk)
            result.stages_completed.append(ProcessingStage.OUTPUT)
            
            # Update statistics
            processing_time = time.time() - start_time
            result.processing_time = processing_time
            
            with self._lock:
                self.stats['chunks_processed'] += 1
                self.stats['total_processing_time'] += processing_time
                self.stats['average_processing_time'] = (
                    self.stats['total_processing_time'] / self.stats['chunks_processed']
                )
            
            # Publish processing event
            self.event_bus.publish("audio.chunk_processed", result.to_dict(), source="audio-processor")
            
            self.logger.debug(f"Audio chunk processed", extra={
                'device_id': chunk.device_id,
                'processing_time_ms': processing_time * 1000,
                'stages': len(result.stages_completed)
            })
            
        except Exception as e:
            result.errors.append(f"Processing failed: {str(e)}")
            with self._lock:
                self.stats['errors'] += 1
            
            self.logger.error(f"Audio chunk processing failed", extra={
                'device_id': chunk.device_id,
                'error': str(e)
            })
        
        return result
    
    def _chunk_to_numpy(self, chunk: AudioChunk) -> Optional[np.ndarray]:
        """Convert audio chunk to numpy array."""
        try:
            if chunk.format == AudioFormat.PCM_16BIT:
                # Convert 16-bit PCM to numpy array
                data = np.frombuffer(chunk.data, dtype=np.int16)
                return data.astype(np.float32) / 32768.0  # Normalize to [-1, 1]
            
            elif chunk.format == AudioFormat.PCM_24BIT:
                # Convert 24-bit PCM to numpy array
                data = np.frombuffer(chunk.data, dtype=np.uint8)
                # Reshape and combine bytes
                data = data.reshape(-1, 3)
                audio_data = (data[:, 0] | (data[:, 1] << 8) | (data[:, 2] << 16)).astype(np.int32)
                # Convert to 24-bit signed
                audio_data = np.where(audio_data >= 8388608, audio_data - 16777216, audio_data)
                return audio_data.astype(np.float32) / 8388608.0  # Normalize to [-1, 1]
            
            else:
                self.logger.warning(f"Unsupported audio format: {chunk.format}")
                return None
                
        except Exception as e:
            self.logger.error(f"Failed to convert chunk to numpy: {e}")
            return None
    
    def _numpy_to_chunk(self, audio_data: np.ndarray, original_chunk: AudioChunk) -> bytes:
        """Convert numpy array back to audio chunk bytes."""
        try:
            if original_chunk.format == AudioFormat.PCM_16BIT:
                # Convert back to 16-bit PCM
                normalized_data = np.clip(audio_data, -1.0, 1.0)
                int_data = (normalized_data * 32767).astype(np.int16)
                return int_data.tobytes()
            
            elif original_chunk.format == AudioFormat.PCM_24BIT:
                # Convert back to 24-bit PCM
                normalized_data = np.clip(audio_data, -1.0, 1.0)
                int_data = (normalized_data * 8388607).astype(np.int32)
                
                # Convert to 24-bit bytes
                byte_data = bytearray()
                for sample in int_data:
                    if sample < 0:
                        sample = sample + (1 << 24)
                    byte_data.extend(sample.to_bytes(3, 'little'))
                
                return bytes(byte_data)
            
            else:
                return original_chunk.data
                
        except Exception as e:
            self.logger.error(f"Failed to convert numpy to chunk: {e}")
            return original_chunk.data
    
    def _apply_filters(self, audio_data: np.ndarray, sample_rate: int) -> np.ndarray:
        """Apply all enabled filters to audio data."""
        filtered_data = audio_data.copy()
        
        with self._lock:
            for audio_filter in self.filters:
                if audio_filter.enabled:
                    try:
                        filtered_data = audio_filter.process(filtered_data, sample_rate)
                    except Exception as e:
                        self.logger.error(f"Filter '{audio_filter.name}' failed: {e}")
        
        return filtered_data
    
    def _enhance_audio(self, audio_data: np.ndarray) -> np.ndarray:
        """Apply audio enhancement algorithms."""
        # This could include AGC, dynamic range compression, etc.
        # For now, just return the filtered data
        return audio_data
    
    def get_stats(self) -> Dict[str, Any]:
        """Get processing statistics."""
        with self._lock:
            stats = self.stats.copy()
            stats['filters'] = [
                {
                    'name': f.name,
                    'enabled': f.enabled,
                    'type': f.__class__.__name__
                }
                for f in self.filters
            ]
            return stats
    
    def reset_stats(self) -> None:
        """Reset processing statistics."""
        with self._lock:
            self.stats = {
                'chunks_processed': 0,
                'total_processing_time': 0.0,
                'average_processing_time': 0.0,
                'errors': 0
            }
        
        self.logger.info("Audio processor statistics reset")
    
    def health_check(self) -> Dict[str, Any]:
        """Perform health check."""
        health = {
            'filters_loaded': len(self.filters),
            'enabled_filters': sum(1 for f in self.filters if f.enabled),
            'chunks_processed': self.stats['chunks_processed'],
            'average_processing_time_ms': self.stats['average_processing_time'] * 1000,
            'error_rate': self.stats['errors'] / max(1, self.stats['chunks_processed']),
            'healthy': True
        }
        
        # Check if processing time is reasonable
        if health['average_processing_time_ms'] > 100:  # More than 100ms is too slow
            health['healthy'] = False
        
        # Check error rate
        if health['error_rate'] > 0.05:  # More than 5% error rate
            health['healthy'] = False
        
        return health