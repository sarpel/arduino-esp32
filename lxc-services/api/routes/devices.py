"""
Device management API routes.
"""

from fastapi import APIRouter, Depends, HTTPException
from typing import List, Dict, Any, Optional
from pydantic import BaseModel
from datetime import datetime

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from core.logger import get_logger
from core.events import get_event_bus

logger = get_logger(__name__)
event_bus = get_event_bus()
router = APIRouter()


# Pydantic models
class DeviceInfo(BaseModel):
    """Device information model."""
    device_id: str
    name: str
    type: str
    status: str
    last_seen: float
    ip_address: Optional[str] = None
    mac_address: Optional[str] = None
    firmware_version: Optional[str] = None
    hardware_version: Optional[str] = None
    battery_level: Optional[float] = None
    signal_strength: Optional[float] = None
    sample_rate: int
    channels: int
    bits_per_sample: int
    audio_format: str
    compression_enabled: bool
    location: Optional[str] = None
    metadata: Dict[str, Any] = {}


class DeviceConfig(BaseModel):
    """Device configuration model."""
    sample_rate: Optional[int] = None
    channels: Optional[int] = None
    bits_per_sample: Optional[int] = None
    compression_enabled: Optional[bool] = None
    compression_type: Optional[str] = None
    audio_format: Optional[str] = None
    streaming_enabled: Optional[bool] = None
    auto_reconnect: Optional[bool] = None
    buffer_size: Optional[int] = None


class DeviceStats(BaseModel):
    """Device statistics model."""
    device_id: str
    uptime: float
    bytes_sent: int
    packets_sent: int
    packets_dropped: int
    average_latency: float
    quality_score: float
    last_reset: float


# Routes
@router.get("/", response_model=List[DeviceInfo])
async def get_devices(
    status: Optional[str] = None,
    device_type: Optional[str] = None,
    limit: int = 100
):
    """
    Get list of devices with optional filtering.
    
    Args:
        status: Filter by device status
        device_type: Filter by device type
        limit: Maximum number of devices
        
    Returns:
        List of devices
    """
    try:
        # Mock data for now - would integrate with actual device manager
        devices = [
            DeviceInfo(
                device_id="esp32_001",
                name="Living Room Sensor",
                type="ESP32",
                status="online",
                last_seen=datetime.now().timestamp(),
                ip_address="192.168.1.100",
                mac_address="24:6F:28:12:34:56",
                firmware_version="2.1.0",
                hardware_version="1.0",
                battery_level=85.0,
                signal_strength=-45.0,
                sample_rate=16000,
                channels=1,
                bits_per_sample=16,
                audio_format="wav",
                compression_enabled=True,
                location="living_room"
            ),
            DeviceInfo(
                device_id="esp32_002",
                name="Bedroom Sensor",
                type="ESP32",
                status="offline",
                last_seen=datetime.now().timestamp() - 3600,
                ip_address="192.168.1.101",
                mac_address="24:6F:28:12:34:57",
                firmware_version="2.0.1",
                hardware_version="1.0",
                battery_level=45.0,
                signal_strength=-65.0,
                sample_rate=16000,
                channels=1,
                bits_per_sample=16,
                audio_format="wav",
                compression_enabled=True,
                location="bedroom"
            )
        ]
        
        # Apply filters
        if status:
            devices = [d for d in devices if d.status == status]
        if device_type:
            devices = [d for d in devices if d.type == device_type]
        
        return devices[:limit]
        
    except Exception as e:
        logger.error(f"Failed to get devices: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/{device_id}", response_model=DeviceInfo)
async def get_device(device_id: str):
    """
    Get specific device information.
    
    Args:
        device_id: Device identifier
        
    Returns:
        Device information
    """
    try:
        # Mock implementation
        if device_id == "esp32_001":
            return DeviceInfo(
                device_id="esp32_001",
                name="Living Room Sensor",
                type="ESP32",
                status="online",
                last_seen=datetime.now().timestamp(),
                ip_address="192.168.1.100",
                mac_address="24:6F:28:12:34:56",
                firmware_version="2.1.0",
                hardware_version="1.0",
                battery_level=85.0,
                signal_strength=-45.0,
                sample_rate=16000,
                channels=1,
                bits_per_sample=16,
                audio_format="wav",
                compression_enabled=True,
                location="living_room"
            )
        
        raise HTTPException(status_code=404, detail="Device not found")
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to get device: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.put("/{device_id}/config", response_model=DeviceInfo)
async def update_device_config(device_id: str, config: DeviceConfig):
    """
    Update device configuration.
    
    Args:
        device_id: Device identifier
        config: New configuration
        
    Returns:
        Updated device information
    """
    try:
        # Mock implementation
        device = await get_device(device_id)
        
        # Update configuration (mock)
        updated_device = device.copy()
        
        # Emit configuration update event
        event_bus.emit({
            'event_type': 'device.config_updated',
            'source': 'API',
            'data': {
                'device_id': device_id,
                'config': config.dict()
            }
        })
        
        return updated_device
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to update device config: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/{device_id}/stats", response_model=DeviceStats)
async def get_device_stats(device_id: str):
    """
    Get device statistics.
    
    Args:
        device_id: Device identifier
        
    Returns:
        Device statistics
    """
    try:
        # Mock implementation
        return DeviceStats(
            device_id=device_id,
            uptime=86400.0,  # 24 hours
            bytes_sent=1048576,  # 1MB
            packets_sent=1024,
            packets_dropped=5,
            average_latency=25.5,
            quality_score=0.95,
            last_reset=datetime.now().timestamp() - 86400
        )
        
    except Exception as e:
        logger.error(f"Failed to get device stats: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/{device_id}/restart")
async def restart_device(device_id: str):
    """
    Restart device.
    
    Args:
        device_id: Device identifier
        
    Returns:
        Restart status
    """
    try:
        # Check if device exists
        await get_device(device_id)
        
        # Emit restart command
        event_bus.emit({
            'event_type': 'device.restart_requested',
            'source': 'API',
            'data': {
                'device_id': device_id,
                'timestamp': datetime.now().timestamp()
            }
        })
        
        return {"status": "restart_requested", "device_id": device_id}
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to restart device: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.delete("/{device_id}")
async def delete_device(device_id: str):
    """
    Delete/remove device from system.
    
    Args:
        device_id: Device identifier
        
    Returns:
        Deletion status
    """
    try:
        # Check if device exists
        await get_device(device_id)
        
        # Emit device removal event
        event_bus.emit({
            'event_type': 'device.removed',
            'source': 'API',
            'data': {
                'device_id': device_id,
                'timestamp': datetime.now().timestamp()
            }
        })
        
        return {"status": "deleted", "device_id": device_id}
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to delete device: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/types/available")
async def get_available_device_types():
    """
    Get list of available device types.
    
    Returns:
        Available device types
    """
    try:
        return {
            "device_types": [
                {
                    "type": "ESP32",
                    "description": "ESP32-based audio sensor",
                    "supported_formats": ["wav", "mp3"],
                    "max_sample_rate": 48000,
                    "max_channels": 2,
                    "compression_supported": True
                },
                {
                    "type": "RaspberryPi",
                    "description": "Raspberry Pi audio gateway",
                    "supported_formats": ["wav", "flac", "mp3"],
                    "max_sample_rate": 96000,
                    "max_channels": 8,
                    "compression_supported": True
                }
            ]
        }
        
    except Exception as e:
        logger.error(f"Failed to get device types: {e}")
        raise HTTPException(status_code=500, detail=str(e))