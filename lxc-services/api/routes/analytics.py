"""
Analytics API routes.
"""

from fastapi import APIRouter, Depends, HTTPException
from typing import List, Dict, Any, Optional
from pydantic import BaseModel
from datetime import datetime, timedelta

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from core.logger import get_logger
from core.database import get_database

logger = get_logger(__name__)
router = APIRouter()


# Pydantic models
class AnalyticsData(BaseModel):
    """Analytics data model."""
    timestamp: float
    metric_name: str
    value: float
    device_id: Optional[str] = None
    tags: Dict[str, str] = {}


class AnalyticsQuery(BaseModel):
    """Analytics query model."""
    metric_names: List[str]
    start_time: float
    end_time: float
    device_ids: Optional[List[str]] = None
    aggregation: Optional[str] = "avg"  # avg, sum, min, max, count
    interval: Optional[str] = "1h"  # 1m, 5m, 15m, 1h, 1d


class AnalyticsResponse(BaseModel):
    """Analytics response model."""
    query: AnalyticsQuery
    data: List[Dict[str, Any]]
    total_points: int
    has_more: bool


# Routes
@router.post("/query", response_model=AnalyticsResponse)
async def query_analytics(
    query: AnalyticsQuery,
    database = Depends(get_database)
):
    """
    Query analytics data.
    
    Args:
        query: Analytics query parameters
        database: Database instance
        
    Returns:
        Analytics data
    """
    try:
        # Mock implementation
        mock_data = []
        current_time = query.start_time
        
        while current_time <= query.end_time:
            for metric_name in query.metric_names:
                mock_data.append({
                    "timestamp": current_time,
                    "metric_name": metric_name,
                    "value": 50.0 + (hash(f"{metric_name}{current_time}") % 100),
                    "device_id": query.device_ids[0] if query.device_ids else None
                })
            
            current_time += 3600  # 1 hour intervals
        
        return AnalyticsResponse(
            query=query,
            data=mock_data,
            total_points=len(mock_data),
            has_more=False
        )
        
    except Exception as e:
        logger.error(f"Failed to query analytics: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/metrics/available")
async def get_available_metrics():
    """
    Get list of available metrics.
    
    Returns:
        Available metrics
    """
    try:
        return {
            "metrics": [
                {
                    "name": "audio_quality_score",
                    "description": "Audio quality score (0-1)",
                    "unit": "score",
                    "type": "gauge"
                },
                {
                    "name": "processing_latency_ms",
                    "description": "Audio processing latency",
                    "unit": "milliseconds",
                    "type": "gauge"
                },
                {
                    "name": "bytes_received",
                    "description": "Bytes received from devices",
                    "unit": "bytes",
                    "type": "counter"
                },
                {
                    "name": "cpu_percent",
                    "description": "CPU usage percentage",
                    "unit": "percent",
                    "type": "gauge"
                },
                {
                    "name": "memory_percent",
                    "description": "Memory usage percentage",
                    "unit": "percent",
                    "type": "gauge"
                }
            ]
        }
        
    except Exception as e:
        logger.error(f"Failed to get available metrics: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/dashboard/summary")
async def get_dashboard_summary():
    """
    Get dashboard summary data.
    
    Returns:
        Dashboard summary
    """
    try:
        return {
            "overview": {
                "total_devices": 2,
                "active_devices": 1,
                "total_audio_files": 1250,
                "storage_used_gb": 15.7,
                "avg_quality_score": 0.92
            },
            "recent_activity": [
                {
                    "timestamp": datetime.now().timestamp() - 300,
                    "type": "audio_uploaded",
                    "device_id": "esp32_001",
                    "details": "New audio segment uploaded"
                },
                {
                    "timestamp": datetime.now().timestamp() - 600,
                    "type": "device_connected",
                    "device_id": "esp32_002",
                    "details": "Device reconnected"
                }
            ],
            "alerts": [
                {
                    "level": "warning",
                    "message": "High memory usage detected",
                    "timestamp": datetime.now().timestamp() - 1800
                }
            ]
        }
        
    except Exception as e:
        logger.error(f"Failed to get dashboard summary: {e}")
        raise HTTPException(status_code=500, detail=str(e))