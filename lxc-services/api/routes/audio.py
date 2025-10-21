"""
Audio management API routes.
"""

from fastapi import APIRouter, Depends, HTTPException, UploadFile, File, BackgroundTasks
from fastapi.responses import StreamingResponse
from typing import List, Optional, Dict, Any
from pydantic import BaseModel
import asyncio
import io
import uuid

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from core.logger import get_logger
from core.events import get_event_bus
from audio_receiver.compression import get_compressor, CompressionType
from audio_receiver.storage import AudioStorageManager
from audio_receiver.processor import AudioProcessor
from audio_receiver.monitoring import get_monitor

logger = get_logger(__name__)
event_bus = get_event_bus()
router = APIRouter()


# Pydantic models
class AudioMetadata(BaseModel):
    """Audio metadata model."""
    device_id: str
    sample_rate: int
    channels: int
    bits_per_sample: int
    duration: float
    format: str
    size: int
    timestamp: float
    quality_score: Optional[float] = None
    compression_ratio: Optional[float] = None


class AudioProcessingRequest(BaseModel):
    """Audio processing request model."""
    filter_type: Optional[str] = None
    noise_reduction: bool = False
    normalize: bool = False
    equalizer_settings: Optional[Dict[str, float]] = None
    compression_type: Optional[str] = "zlib"


class AudioSegment(BaseModel):
    """Audio segment model."""
    id: str
    device_id: str
    start_time: float
    end_time: float
    duration: float
    size: int
    format: str
    quality_score: float
    file_path: str
    metadata: Dict[str, Any]


class AudioStreamResponse(BaseModel):
    """Audio stream response model."""
    stream_id: str
    device_id: str
    status: str
    sample_rate: int
    channels: int
    format: str
    bitrate: Optional[int] = None


# Dependencies
async def get_audio_processor():
    """Get audio processor instance."""
    return AudioProcessor()


async def get_storage_manager():
    """Get storage manager instance."""
    return AudioStorageManager()


async def get_compressor_instance():
    """Get compressor instance."""
    return get_compressor()


# Routes
@router.post("/upload", response_model=AudioMetadata)
async def upload_audio(
    file: UploadFile = File(...),
    device_id: str = "unknown",
    background_tasks: BackgroundTasks = BackgroundTasks(),
    processor: AudioProcessor = Depends(get_audio_processor),
    storage: AudioStorageManager = Depends(get_storage_manager),
    compressor = Depends(get_compressor_instance)
):
    """
    Upload and process audio file.
    
    Args:
        file: Audio file to upload
        device_id: Device identifier
        background_tasks: Background tasks
        processor: Audio processor
        storage: Storage manager
        compressor: Audio compressor
        
    Returns:
        Audio metadata
    """
    try:
        # Validate file type
        if not file.content_type or not file.content_type.startswith('audio/'):
            raise HTTPException(status_code=400, detail="Invalid file type")
        
        # Read file content
        content = await file.read()
        
        # Process audio
        audio_data, metadata = await processor.process_audio_data(content, file.filename)
        
        # Compress audio
        compressed_data, compression_metrics = await compressor.compress_audio(
            audio_data, CompressionType.ZLIB
        )
        
        # Store audio
        file_id = str(uuid.uuid4())
        file_path = await storage.store_audio_segment(
            device_id, compressed_data, metadata, file_id
        )
        
        # Update monitoring
        monitor = get_monitor()
        monitor.increment_counter('audio_files_uploaded')
        monitor.increment_counter('bytes_received', len(content))
        monitor.set_gauge('processing_latency_ms', compression_metrics.compression_time * 1000)
        
        # Emit event
        event_bus.emit({
            'event_type': 'audio.uploaded',
            'source': 'API',
            'data': {
                'file_id': file_id,
                'device_id': device_id,
                'size': len(content),
                'compression_ratio': compression_metrics.compression_ratio
            }
        })
        
        # Schedule background processing
        background_tasks.add_task(
            process_audio_background,
            file_id,
            device_id,
            audio_data
        )
        
        return AudioMetadata(
            device_id=device_id,
            sample_rate=metadata['sample_rate'],
            channels=metadata['channels'],
            bits_per_sample=metadata['bits_per_sample'],
            duration=metadata['duration'],
            format=metadata['format'],
            size=len(content),
            timestamp=metadata['timestamp'],
            quality_score=compression_metrics.quality_score,
            compression_ratio=compression_metrics.compression_ratio
        )
        
    except Exception as e:
        logger.error(f"Audio upload failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/segments", response_model=List[AudioSegment])
async def get_audio_segments(
    device_id: Optional[str] = None,
    limit: int = 100,
    offset: int = 0,
    storage: AudioStorageManager = Depends(get_storage_manager)
):
    """
    Get audio segments with optional filtering.
    
    Args:
        device_id: Filter by device ID
        limit: Maximum number of segments
        offset: Offset for pagination
        storage: Storage manager
        
    Returns:
        List of audio segments
    """
    try:
        segments = await storage.get_audio_segments(
            device_id=device_id,
            limit=limit,
            offset=offset
        )
        
        return [
            AudioSegment(
                id=segment['id'],
                device_id=segment['device_id'],
                start_time=segment['start_time'],
                end_time=segment['end_time'],
                duration=segment['duration'],
                size=segment['size'],
                format=segment['format'],
                quality_score=segment['quality_score'],
                file_path=segment['file_path'],
                metadata=segment['metadata']
            )
            for segment in segments
        ]
        
    except Exception as e:
        logger.error(f"Failed to get audio segments: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/segments/{segment_id}")
async def get_audio_segment(
    segment_id: str,
    storage: AudioStorageManager = Depends(get_storage_manager),
    compressor = Depends(get_compressor_instance)
):
    """
    Get specific audio segment.
    
    Args:
        segment_id: Segment ID
        storage: Storage manager
        compressor: Audio compressor
        
    Returns:
        Audio file stream
    """
    try:
        # Get segment metadata
        segment = await storage.get_audio_segment(segment_id)
        if not segment:
            raise HTTPException(status_code=404, detail="Segment not found")
        
        # Get compressed data
        compressed_data = await storage.get_audio_data(segment['file_path'])
        
        # Decompress audio
        audio_data = await compressor.decompress_audio(
            compressed_data,
            CompressionType.ZLIB,
            segment['metadata']['shape']
        )
        
        # Create streaming response
        def iterfile():
            yield audio_data.tobytes()
        
        return StreamingResponse(
            iterfile(),
            media_type=f"audio/{segment['format']}",
            headers={
                "Content-Disposition": f"attachment; filename={segment_id}.{segment['format']}"
            }
        )
        
    except Exception as e:
        logger.error(f"Failed to get audio segment: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/segments/{segment_id}/process")
async def process_audio_segment(
    segment_id: str,
    request: AudioProcessingRequest,
    background_tasks: BackgroundTasks,
    processor: AudioProcessor = Depends(get_audio_processor),
    storage: AudioStorageManager = Depends(get_storage_manager)
):
    """
    Process audio segment with filters and effects.
    
    Args:
        segment_id: Segment ID
        request: Processing request
        background_tasks: Background tasks
        processor: Audio processor
        storage: Storage manager
        
    Returns:
        Processing job ID
    """
    try:
        # Get segment
        segment = await storage.get_audio_segment(segment_id)
        if not segment:
            raise HTTPException(status_code=404, detail="Segment not found")
        
        # Create processing job
        job_id = str(uuid.uuid4())
        
        # Schedule background processing
        background_tasks.add_task(
            apply_audio_processing,
            job_id,
            segment_id,
            request.dict()
        )
        
        # Update monitoring
        monitor = get_monitor()
        monitor.increment_counter('audio_processing_jobs')
        
        return {"job_id": job_id, "status": "queued"}
        
    except Exception as e:
        logger.error(f"Failed to process audio segment: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.delete("/segments/{segment_id}")
async def delete_audio_segment(
    segment_id: str,
    storage: AudioStorageManager = Depends(get_storage_manager)
):
    """
    Delete audio segment.
    
    Args:
        segment_id: Segment ID
        storage: Storage manager
        
    Returns:
        Deletion status
    """
    try:
        # Get segment
        segment = await storage.get_audio_segment(segment_id)
        if not segment:
            raise HTTPException(status_code=404, detail="Segment not found")
        
        # Delete segment
        await storage.delete_audio_segment(segment_id)
        
        # Update monitoring
        monitor = get_monitor()
        monitor.increment_counter('audio_files_deleted')
        
        # Emit event
        event_bus.emit({
            'event_type': 'audio.deleted',
            'source': 'API',
            'data': {
                'segment_id': segment_id,
                'device_id': segment['device_id']
            }
        })
        
        return {"status": "deleted", "segment_id": segment_id}
        
    except Exception as e:
        logger.error(f"Failed to delete audio segment: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/streams", response_model=List[AudioStreamResponse])
async def get_active_streams():
    """
    Get active audio streams.
    
    Returns:
        List of active streams
    """
    try:
        # This would integrate with the actual streaming server
        # For now, return mock data
        return [
            AudioStreamResponse(
                stream_id="stream_1",
                device_id="esp32_001",
                status="active",
                sample_rate=16000,
                channels=1,
                format="wav",
                bitrate=128
            )
        ]
        
    except Exception as e:
        logger.error(f"Failed to get active streams: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/stats")
async def get_audio_statistics():
    """
    Get audio processing statistics.
    
    Returns:
        Audio statistics
    """
    try:
        monitor = get_monitor()
        current_metrics = monitor.get_current_metrics()
        
        return {
            "total_files_uploaded": monitor.counters.get('audio_files_uploaded', 0),
            "total_bytes_received": monitor.counters.get('bytes_received', 0),
            "total_processing_jobs": monitor.counters.get('audio_processing_jobs', 0),
            "current_processing_latency": current_metrics.get('gauges', {}).get('processing_latency_ms', 0),
            "average_quality_score": current_metrics.get('gauges', {}).get('quality_score', 0),
            "compression_ratio": current_metrics.get('gauges', {}).get('compression_ratio', 1.0)
        }
        
    except Exception as e:
        logger.error(f"Failed to get audio statistics: {e}")
        raise HTTPException(status_code=500, detail=str(e))


# Background tasks
async def process_audio_background(file_id: str, device_id: str, audio_data):
    """Background task for audio processing."""
    try:
        # Additional processing can be done here
        logger.info(f"Background processing for file {file_id} from device {device_id}")
        
    except Exception as e:
        logger.error(f"Background processing failed: {e}")


async def apply_audio_processing(job_id: str, segment_id: str, processing_params: Dict[str, Any]):
    """Background task for audio processing."""
    try:
        logger.info(f"Processing job {job_id} for segment {segment_id}")
        
        # Apply processing based on parameters
        # This would integrate with the actual audio processor
        
    except Exception as e:
        logger.error(f"Audio processing job failed: {e}")