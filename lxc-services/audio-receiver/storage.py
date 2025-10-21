"""
Audio storage manager with database integration and file management.
"""

import os
import threading
import time
import struct
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass, field
from enum import Enum
import json
import uuid

from .server import AudioChunk, AudioFormat, DeviceInfo
from .processor import ProcessingResult
from ..core.logger import get_logger, LogContext
from ..core.events import get_event_bus
from ..core.config import get_config


class StorageStatus(Enum):
    """Storage operation status."""
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


@dataclass
class AudioSegment:
    """Audio segment metadata."""
    segment_id: str
    device_id: str
    file_path: str
    start_time: datetime
    end_time: Optional[datetime] = None
    duration_seconds: float = 0.0
    file_size_bytes: int = 0
    sample_rate: int = 16000
    channels: int = 1
    bits_per_sample: int = 16
    format: str = "wav"
    chunks_count: int = 0
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: datetime = field(default_factory=datetime.now)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            'segment_id': self.segment_id,
            'device_id': self.device_id,
            'file_path': self.file_path,
            'start_time': self.start_time.isoformat(),
            'end_time': self.end_time.isoformat() if self.end_time else None,
            'duration_seconds': self.duration_seconds,
            'file_size_bytes': self.file_size_bytes,
            'sample_rate': self.sample_rate,
            'channels': self.channels,
            'bits_per_sample': self.bits_per_sample,
            'format': self.format,
            'chunks_count': self.chunks_count,
            'metadata': self.metadata,
            'created_at': self.created_at.isoformat()
        }


@dataclass
class StorageOperation:
    """Storage operation tracking."""
    operation_id: str
    operation_type: str  # 'write', 'read', 'delete', 'compress'
    status: StorageStatus
    file_path: str
    device_id: str = None
    start_time: datetime = field(default_factory=datetime.now)
    end_time: Optional[datetime] = None
    bytes_processed: int = 0
    error_message: str = None
    metadata: Dict[str, Any] = field(default_factory=dict)


class AudioFileWriter:
    """Handles writing audio data to files in various formats."""
    
    def __init__(self, file_path: str, sample_rate: int = 16000, channels: int = 1, 
                 bits_per_sample: int = 16, format: str = "wav"):
        self.file_path = file_path
        self.sample_rate = sample_rate
        self.channels = channels
        self.bits_per_sample = bits_per_sample
        self.format = format.lower()
        self.bytes_per_sample = bits_per_sample // 8
        
        self.logger = get_logger(f"audio.writer.{Path(file_path).name}")
        self.file_handle = None
        self.bytes_written = 0
        self.is_open = False
        
        # Ensure directory exists
        Path(file_path).parent.mkdir(parents=True, exist_ok=True)
    
    def open(self) -> bool:
        """Open file for writing."""
        try:
            if self.format == "wav":
                return self._open_wav()
            else:
                self.logger.error(f"Unsupported format: {self.format}")
                return False
        except Exception as e:
            self.logger.error(f"Failed to open file: {e}")
            return False
    
    def write_chunk(self, chunk: AudioChunk) -> bool:
        """Write audio chunk to file."""
        if not self.is_open:
            return False
        
        try:
            if self.format == "wav":
                return self._write_wav_chunk(chunk)
            else:
                self.logger.error(f"Unsupported format for writing: {self.format}")
                return False
        except Exception as e:
            self.logger.error(f"Failed to write chunk: {e}")
            return False
    
    def close(self) -> bool:
        """Close file and finalize."""
        try:
            if self.format == "wav":
                return self._close_wav()
            else:
                return True  # Nothing to do for other formats
        except Exception as e:
            self.logger.error(f"Failed to close file: {e}")
            return False
    
    def _open_wav(self) -> bool:
        """Open WAV file for writing."""
        self.file_handle = open(self.file_path, 'wb')
        
        # Write WAV header (will be updated when closing)
        self._write_wav_header(0)  # Placeholder size
        
        self.is_open = True
        self.logger.debug(f"WAV file opened for writing: {self.file_path}")
        return True
    
    def _write_wav_chunk(self, chunk: AudioChunk) -> bool:
        """Write chunk to WAV file."""
        if not self.file_handle:
            return False
        
        # Write audio data
        self.file_handle.write(chunk.data)
        self.bytes_written += len(chunk.data)
        
        return True
    
    def _close_wav(self) -> bool:
        """Close WAV file and update header."""
        if not self.file_handle:
            return False
        
        try:
            # Update WAV header with actual file size
            current_pos = self.file_handle.tell()
            self.file_handle.seek(0)
            self._write_wav_header(self.bytes_written)
            self.file_handle.seek(current_pos)
            
            self.file_handle.close()
            self.is_open = False
            
            self.logger.debug(f"WAV file closed: {self.file_path}, bytes written: {self.bytes_written}")
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to close WAV file properly: {e}")
            if self.file_handle:
                self.file_handle.close()
            self.is_open = False
            return False
    
    def _write_wav_header(self, data_size: int) -> None:
        """Write WAV file header."""
        if not self.file_handle:
            return
        
        # RIFF header
        self.file_handle.write(b'RIFF')
        self.file_handle.write(struct.pack('<I', 36 + data_size))  # File size - 8
        self.file_handle.write(b'WAVE')
        
        # fmt chunk
        self.file_handle.write(b'fmt ')
        self.file_handle.write(struct.pack('<I', 16))  # fmt chunk size
        self.file_handle.write(struct.pack('<H', 1))   # PCM format
        self.file_handle.write(struct.pack('<H', self.channels))  # Channels
        self.file_handle.write(struct.pack('<I', self.sample_rate))  # Sample rate
        byte_rate = self.sample_rate * self.channels * self.bytes_per_sample
        self.file_handle.write(struct.pack('<I', byte_rate))  # Byte rate
        block_align = self.channels * self.bytes_per_sample
        self.file_handle.write(struct.pack('<H', block_align))  # Block align
        self.file_handle.write(struct.pack('<H', self.bits_per_sample))  # Bits per sample
        
        # data chunk
        self.file_handle.write(b'data')
        self.file_handle.write(struct.pack('<I', data_size))  # Data size


class AudioStorageManager:
    """
    Manages audio storage with database integration and file organization.
    """
    
    def __init__(self):
        self.logger = get_logger("audio.storage")
        self.event_bus = get_event_bus()
        self.config = get_config()
        
        # Storage configuration
        self.data_dir = Path(self.config.audio_receiver.data_dir)
        self.segment_duration = self.config.audio_receiver.segment_duration
        
        # Active segments by device
        self.active_segments: Dict[str, AudioSegment] = {}
        self.segment_writers: Dict[str, AudioFileWriter] = {}
        
        # Storage statistics
        self.stats = {
            'segments_created': 0,
            'segments_completed': 0,
            'total_bytes_written': 0,
            'total_files_stored': 0,
            'storage_errors': 0,
            'active_segments': 0
        }
        
        # Thread safety
        self._lock = threading.Lock()
        
        # Ensure data directory exists
        self.data_dir.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"AudioStorageManager initialized", extra={
            'data_dir': str(self.data_dir),
            'segment_duration': self.segment_duration
        })
    
    def start_device_segment(self, device_id: str, device_info: DeviceInfo) -> str:
        """
        Start a new audio segment for a device.
        
        Args:
            device_id: Device identifier
            device_info: Device information
            
        Returns:
            Segment ID
        """
        with self._lock:
            # Close existing segment if any
            if device_id in self.active_segments:
                self._complete_segment(device_id)
            
            # Generate segment ID and file path
            segment_id = str(uuid.uuid4())
            now = datetime.now()
            
            # Create date-based directory structure
            date_dir = self.data_dir / now.strftime('%Y-%m-%d')
            date_dir.mkdir(exist_ok=True)
            
            # Generate filename
            timestamp = now.strftime('%Y%m%d_%H%M%S')
            filename = f"{timestamp}_{device_id[:8]}_{segment_id[:8]}.wav"
            file_path = date_dir / filename
            
            # Create segment metadata
            segment = AudioSegment(
                segment_id=segment_id,
                device_id=device_id,
                file_path=str(file_path),
                start_time=now,
                sample_rate=device_info.sample_rate,
                channels=device_info.channels,
                bits_per_sample=device_info.bits_per_sample,
                format="wav"
            )
            
            # Create file writer
            writer = AudioFileWriter(
                file_path=str(file_path),
                sample_rate=device_info.sample_rate,
                channels=device_info.channels,
                bits_per_sample=device_info.bits_per_sample
            )
            
            if not writer.open():
                self.logger.error(f"Failed to create segment writer for device {device_id}")
                return None
            
            # Store active segment and writer
            self.active_segments[device_id] = segment
            self.segment_writers[device_id] = writer
            
            self.stats['segments_created'] += 1
            self.stats['active_segments'] = len(self.active_segments)
            
            self.logger.info(f"Started audio segment", extra={
                'segment_id': segment_id,
                'device_id': device_id,
                'file_path': str(file_path)
            })
            
            # Publish segment start event
            self.event_bus.publish("storage.segment_started", segment.to_dict(), source="audio-storage")
            
            return segment_id
    
    def write_audio_chunk(self, device_id: str, chunk: AudioChunk, 
                         processing_result: Optional[ProcessingResult] = None) -> bool:
        """
        Write audio chunk to active segment.
        
        Args:
            device_id: Device identifier
            chunk: Audio chunk to write
            processing_result: Processing result metadata
            
        Returns:
            True if successful
        """
        with self._lock:
            if device_id not in self.active_segments:
                # Auto-start segment if none exists
                self.start_device_segment(device_id, DeviceInfo(
                    device_id=device_id,
                    ip_address="unknown",
                    port=0
                ))
            
            segment = self.active_segments[device_id]
            writer = self.segment_writers[device_id]
            
            try:
                # Write chunk to file
                success = writer.write_chunk(chunk)
                
                if success:
                    # Update segment metadata
                    segment.chunks_count += 1
                    segment.duration_seconds = time.time() - segment.start_time.timestamp()
                    segment.file_size_bytes += len(chunk.data)
                    
                    # Add processing metadata if available
                    if processing_result:
                        if 'audio_metrics' not in segment.metadata:
                            segment.metadata['audio_metrics'] = []
                        segment.metadata['audio_metrics'].append(processing_result.metrics.to_dict())
                    
                    self.stats['total_bytes_written'] += len(chunk.data)
                    
                    # Check if segment should be completed
                    if segment.duration_seconds >= self.segment_duration:
                        self._complete_segment(device_id)
                    
                    self.logger.debug(f"Audio chunk written", extra={
                        'segment_id': segment.segment_id,
                        'device_id': device_id,
                        'chunk_size': len(chunk.data),
                        'duration_seconds': segment.duration_seconds
                    })
                
                return success
                
            except Exception as e:
                self.logger.error(f"Failed to write audio chunk", extra={
                    'segment_id': segment.segment_id,
                    'device_id': device_id,
                    'error': str(e)
                })
                self.stats['storage_errors'] += 1
                return False
    
    def _complete_segment(self, device_id: str) -> Optional[AudioSegment]:
        """Complete active segment for device."""
        if device_id not in self.active_segments:
            return None
        
        segment = self.active_segments[device_id]
        writer = self.segment_writers[device_id]
        
        try:
            # Close file writer
            writer.close()
            
            # Update segment metadata
            segment.end_time = datetime.now()
            segment.duration_seconds = (segment.end_time - segment.start_time).total_seconds()
            
            # Get actual file size
            try:
                segment.file_size_bytes = os.path.getsize(segment.file_path)
            except OSError:
                pass
            
            # Update statistics
            self.stats['segments_completed'] += 1
            self.stats['total_files_stored'] += 1
            
            self.logger.info(f"Audio segment completed", extra={
                'segment_id': segment.segment_id,
                'device_id': device_id,
                'duration_seconds': segment.duration_seconds,
                'file_size_bytes': segment.file_size_bytes,
                'chunks_count': segment.chunks_count
            })
            
            # Publish segment completion event
            self.event_bus.publish("storage.segment_completed", segment.to_dict(), source="audio-storage")
            
        except Exception as e:
            self.logger.error(f"Failed to complete segment", extra={
                'segment_id': segment.segment_id,
                'device_id': device_id,
                'error': str(e)
            })
            self.stats['storage_errors'] += 1
        
        finally:
            # Clean up
            del self.active_segments[device_id]
            del self.segment_writers[device_id]
            self.stats['active_segments'] = len(self.active_segments)
        
        return segment
    
    def complete_device_segment(self, device_id: str) -> Optional[AudioSegment]:
        """
        Manually complete active segment for device.
        
        Args:
            device_id: Device identifier
            
        Returns:
            Completed segment or None
        """
        with self._lock:
            return self._complete_segment(device_id)
    
    def get_active_segments(self) -> List[AudioSegment]:
        """Get list of active segments."""
        with self._lock:
            return list(self.active_segments.values())
    
    def get_device_segment(self, device_id: str) -> Optional[AudioSegment]:
        """Get active segment for device."""
        with self._lock:
            return self.active_segments.get(device_id)
    
    def list_segments(self, 
                     start_date: Optional[datetime] = None,
                     end_date: Optional[datetime] = None,
                     device_id: Optional[str] = None,
                     limit: int = 100) -> List[AudioSegment]:
        """
        List stored audio segments.
        
        Args:
            start_date: Filter by start date
            end_date: Filter by end date
            device_id: Filter by device ID
            limit: Maximum number of results
            
        Returns:
            List of audio segments
        """
        segments = []
        
        try:
            # Walk through data directory
            for date_dir in self.data_dir.iterdir():
                if not date_dir.is_dir():
                    continue
                
                # Check date filter
                try:
                    dir_date = datetime.strptime(date_dir.name, '%Y-%m-%d')
                    if start_date and dir_date < start_date:
                        continue
                    if end_date and dir_date > end_date:
                        continue
                except ValueError:
                    continue
                
                # List files in date directory
                for file_path in date_dir.glob("*.wav"):
                    try:
                        # Extract segment info from filename
                        segment = self._file_path_to_segment(file_path, device_id)
                        if segment:
                            segments.append(segment)
                    except Exception as e:
                        self.logger.warning(f"Failed to process file {file_path}: {e}")
            
            # Sort by start time (newest first)
            segments.sort(key=lambda s: s.start_time, reverse=True)
            
            # Apply limit
            if limit:
                segments = segments[:limit]
            
        except Exception as e:
            self.logger.error(f"Failed to list segments: {e}")
        
        return segments
    
    def _file_path_to_segment(self, file_path: Path, device_filter: Optional[str] = None) -> Optional[AudioSegment]:
        """Convert file path to segment metadata."""
        try:
            # Parse filename: YYYYMMDD_HHMMSS_deviceID_segmentID.wav
            filename = file_path.stem
            parts = filename.split('_')
            
            if len(parts) < 3:
                return None
            
            # Extract timestamp
            timestamp_str = parts[0] + parts[1]
            start_time = datetime.strptime(timestamp_str, '%Y%m%d%H%M%S')
            
            # Extract device ID
            device_id = parts[2]
            if device_filter and device_id != device_filter:
                return None
            
            # Get file stats
            stat = file_path.stat()
            
            # Create segment
            segment = AudioSegment(
                segment_id=parts[3] if len(parts) > 3 else str(uuid.uuid4()),
                device_id=device_id,
                file_path=str(file_path),
                start_time=start_time,
                file_size_bytes=stat.st_size,
                created_at=datetime.fromtimestamp(stat.st_ctime)
            )
            
            return segment
            
        except Exception as e:
            self.logger.debug(f"Failed to parse file path {file_path}: {e}")
            return None
    
    def delete_segment(self, segment_id: str) -> bool:
        """
        Delete audio segment.
        
        Args:
            segment_id: Segment ID to delete
            
        Returns:
            True if deleted
        """
        try:
            # Find segment file
            segments = self.list_segments()
            target_segment = None
            
            for segment in segments:
                if segment.segment_id == segment_id:
                    target_segment = segment
                    break
            
            if not target_segment:
                self.logger.warning(f"Segment not found: {segment_id}")
                return False
            
            # Delete file
            os.remove(target_segment.file_path)
            
            self.logger.info(f"Segment deleted", extra={
                'segment_id': segment_id,
                'file_path': target_segment.file_path
            })
            
            # Publish deletion event
            self.event_bus.publish("storage.segment_deleted", target_segment.to_dict(), source="audio-storage")
            
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to delete segment {segment_id}: {e}")
            return False
    
    def get_storage_stats(self) -> Dict[str, Any]:
        """Get storage statistics."""
        with self._lock:
            stats = self.stats.copy()
            
            # Calculate storage usage
            try:
                total_size = 0
                file_count = 0
                
                for root, dirs, files in os.walk(self.data_dir):
                    for file in files:
                        if file.endswith('.wav'):
                            file_path = os.path.join(root, file)
                            total_size += os.path.getsize(file_path)
                            file_count += 1
                
                stats['total_storage_bytes'] = total_size
                stats['total_storage_mb'] = total_size / (1024 * 1024)
                stats['total_storage_gb'] = total_size / (1024 * 1024 * 1024)
                stats['total_files'] = file_count
                
            except Exception as e:
                self.logger.error(f"Failed to calculate storage usage: {e}")
                stats['total_storage_bytes'] = 0
                stats['total_files'] = 0
            
            return stats
    
    def cleanup_old_segments(self, days_to_keep: int = 30) -> int:
        """
        Clean up old audio segments.
        
        Args:
            days_to_keep: Number of days to keep segments
            
        Returns:
            Number of segments deleted
        """
        cutoff_date = datetime.now() - timedelta(days=days_to_keep)
        deleted_count = 0
        
        try:
            old_segments = self.list_segments(end_date=cutoff_date)
            
            for segment in old_segments:
                if self.delete_segment(segment.segment_id):
                    deleted_count += 1
            
            self.logger.info(f"Cleanup completed", extra={
                'days_to_keep': days_to_keep,
                'segments_deleted': deleted_count
            })
            
        except Exception as e:
            self.logger.error(f"Cleanup failed: {e}")
        
        return deleted_count
    
    def health_check(self) -> Dict[str, Any]:
        """Perform storage health check."""
        health = {
            'data_directory_exists': self.data_dir.exists(),
            'data_directory_writable': False,
            'active_segments': self.stats['active_segments'],
            'total_files': 0,
            'storage_errors': self.stats['storage_errors'],
            'healthy': True
        }
        
        # Check directory permissions
        try:
            test_file = self.data_dir / '.health_check'
            test_file.write_text('test')
            test_file.unlink()
            health['data_directory_writable'] = True
        except Exception:
            health['data_directory_writable'] = False
            health['healthy'] = False
        
        # Count total files
        try:
            for root, dirs, files in os.walk(self.data_dir):
                health['total_files'] += len([f for f in files if f.endswith('.wav')])
        except Exception:
            pass
        
        # Check error rate
        total_operations = self.stats['segments_created'] + self.stats['segments_completed']
        if total_operations > 0:
            error_rate = self.stats['storage_errors'] / total_operations
            if error_rate > 0.05:  # More than 5% error rate
                health['healthy'] = False
        
        return health