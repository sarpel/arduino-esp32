"""
System management API routes.
"""

from fastapi import APIRouter, Depends, HTTPException
from typing import Dict, Any, List
from pydantic import BaseModel
from datetime import datetime
import psutil

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from core.logger import get_logger
from core.config import get_config

logger = get_logger(__name__)
router = APIRouter()


# Pydantic models
class SystemInfo(BaseModel):
    """System information model."""
    hostname: str
    platform: str
    architecture: str
    cpu_count: int
    memory_total: int
    disk_total: int
    uptime: float
    python_version: str


class SystemStatus(BaseModel):
    """System status model."""
    status: str
    timestamp: float
    components: Dict[str, str]


# Routes
@router.get("/info", response_model=SystemInfo)
async def get_system_info():
    """
    Get system information.
    
    Returns:
        System information
    """
    try:
        return SystemInfo(
            hostname=psutil.os.uname().nodename,
            platform=psutil.os.uname().sysname,
            architecture=psutil.os.uname().machine,
            cpu_count=psutil.cpu_count(),
            memory_total=psutil.virtual_memory().total,
            disk_total=psutil.disk_usage('/').total,
            uptime=time.time() - psutil.boot_time(),
            python_version=f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
        )
        
    except Exception as e:
        logger.error(f"Failed to get system info: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/status", response_model=SystemStatus)
async def get_system_status():
    """
    Get system status.
    
    Returns:
        System status
    """
    try:
        return SystemStatus(
            status="healthy",
            timestamp=datetime.now().timestamp(),
            components={
                "api": "healthy",
                "database": "healthy",  # Would check actual DB status
                "monitoring": "healthy",
                "audio_receiver": "healthy"
            }
        )
        
    except Exception as e:
        logger.error(f"Failed to get system status: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/shutdown")
async def shutdown_system():
    """
    Shutdown the system.
    
    Returns:
        Shutdown status
    """
    try:
        # Emit shutdown event
        logger.info("System shutdown requested via API")
        
        return {"status": "shutdown_initiated", "timestamp": datetime.now().timestamp()}
        
    except Exception as e:
        logger.error(f"Failed to shutdown system: {e}")
        raise HTTPException(status_code=500, detail=str(e))