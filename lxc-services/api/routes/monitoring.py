"""
Monitoring API routes.
"""

from fastapi import APIRouter, Depends, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.responses import StreamingResponse
from typing import List, Dict, Any, Optional
from pydantic import BaseModel
import asyncio
import json
from datetime import datetime, timedelta

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from core.logger import get_logger
from core.events import get_event_bus
from audio_receiver.monitoring import get_monitor, AlertLevel

logger = get_logger(__name__)
event_bus = get_event_bus()
router = APIRouter()


# Pydantic models
class SystemMetrics(BaseModel):
    """System metrics model."""
    timestamp: float
    cpu_percent: float
    memory_percent: float
    memory_used_mb: float
    disk_usage_percent: float
    network_io: Dict[str, int]
    active_connections: int
    thread_count: int


class AudioMetrics(BaseModel):
    """Audio metrics model."""
    timestamp: float
    devices_active: int
    total_bytes_received: int
    total_bytes_processed: int
    processing_latency_ms: float
    buffer_utilization: float
    dropped_packets: int
    quality_score: float
    compression_ratio: float


class PerformanceAlert(BaseModel):
    """Performance alert model."""
    level: str
    metric: str
    threshold: float
    message: str
    timestamp: float
    resolved: bool = False
    resolved_timestamp: Optional[float] = None


class MetricsSummary(BaseModel):
    """Metrics summary model."""
    duration_minutes: int
    system_samples: int
    audio_samples: int
    system: Optional[Dict[str, float]] = None
    audio: Optional[Dict[str, float]] = None
    timers: Optional[Dict[str, Dict[str, float]]] = None


class WebSocketMessage(BaseModel):
    """WebSocket message model."""
    type: str
    data: Dict[str, Any]
    timestamp: float


# WebSocket connection manager
class ConnectionManager:
    """Manages WebSocket connections for real-time monitoring."""
    
    def __init__(self):
        self.active_connections: List[WebSocket] = []
    
    async def connect(self, websocket: WebSocket):
        """Accept WebSocket connection."""
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"WebSocket connected. Total connections: {len(self.active_connections)}")
    
    def disconnect(self, websocket: WebSocket):
        """Remove WebSocket connection."""
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            logger.info(f"WebSocket disconnected. Total connections: {len(self.active_connections)}")
    
    async def send_personal_message(self, message: str, websocket: WebSocket):
        """Send message to specific WebSocket."""
        await websocket.send_text(message)
    
    async def broadcast(self, message: str):
        """Broadcast message to all connected WebSockets."""
        disconnected = []
        for connection in self.active_connections:
            try:
                await connection.send_text(message)
            except:
                disconnected.append(connection)
        
        # Remove disconnected connections
        for connection in disconnected:
            self.disconnect(connection)


manager = ConnectionManager()


# Routes
@router.get("/metrics/current")
async def get_current_metrics():
    """
    Get current system and audio metrics.
    
    Returns:
        Current metrics
    """
    try:
        monitor = get_monitor()
        metrics = monitor.get_current_metrics()
        
        return {
            "timestamp": datetime.now().isoformat(),
            "system": metrics.get("system"),
            "audio": metrics.get("audio"),
            "counters": metrics.get("counters"),
            "gauges": metrics.get("gauges"),
            "active_alerts": metrics.get("active_alerts", 0),
            "total_alerts": metrics.get("total_alerts", 0)
        }
        
    except Exception as e:
        logger.error(f"Failed to get current metrics: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/metrics/summary", response_model=MetricsSummary)
async def get_metrics_summary(duration_minutes: int = 60):
    """
    Get metrics summary for specified duration.
    
    Args:
        duration_minutes: Duration in minutes
        
    Returns:
        Metrics summary
    """
    try:
        monitor = get_monitor()
        summary = monitor.get_metrics_summary(duration_minutes)
        
        return MetricsSummary(**summary)
        
    except Exception as e:
        logger.error(f"Failed to get metrics summary: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/alerts", response_model=List[PerformanceAlert])
async def get_alerts(active_only: bool = True):
    """
    Get performance alerts.
    
    Args:
        active_only: Return only active alerts
        
    Returns:
        List of alerts
    """
    try:
        monitor = get_monitor()
        
        if active_only:
            alerts = monitor.get_active_alerts()
        else:
            alerts = monitor.alerts
        
        return [
            PerformanceAlert(
                level=alert.level.value,
                metric=alert.metric,
                threshold=alert.threshold,
                message=alert.message,
                timestamp=alert.timestamp,
                resolved=alert.resolved,
                resolved_timestamp=alert.resolved_timestamp
            )
            for alert in alerts
        ]
        
    except Exception as e:
        logger.error(f"Failed to get alerts: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/alerts/{alert_id}/resolve")
async def resolve_alert(alert_id: int):
    """
    Resolve a specific alert.
    
    Args:
        alert_id: Alert ID
        
    Returns:
        Resolution status
    """
    try:
        monitor = get_monitor()
        success = monitor.resolve_alert(alert_id)
        
        if not success:
            raise HTTPException(status_code=404, detail="Alert not found")
        
        return {"status": "resolved", "alert_id": alert_id}
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to resolve alert: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/alerts/clear")
async def clear_resolved_alerts():
    """
    Clear all resolved alerts.
    
    Returns:
        Number of cleared alerts
    """
    try:
        monitor = get_monitor()
        cleared_count = monitor.clear_resolved_alerts()
        
        return {"cleared_count": cleared_count}
        
    except Exception as e:
        logger.error(f"Failed to clear resolved alerts: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/metrics/export")
async def export_metrics(
    format: str = "json",
    duration_hours: int = 24
):
    """
    Export metrics data.
    
    Args:
        format: Export format (json, csv)
        duration_hours: Duration in hours
        
    Returns:
        Exported data file
    """
    try:
        monitor = get_monitor()
        
        # Generate filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"metrics_{timestamp}.{format}"
        
        # Export metrics
        success = monitor.export_metrics(filename, format)
        
        if not success:
            raise HTTPException(status_code=500, detail="Export failed")
        
        # Read file and return as response
        def iterfile():
            with open(filename, 'rb') as f:
                yield from f
        
        return StreamingResponse(
            iterfile(),
            media_type="application/octet-stream",
            headers={"Content-Disposition": f"attachment; filename={filename}"}
        )
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to export metrics: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint for real-time metrics updates.
    """
    await manager.connect(websocket)
    
    try:
        # Send initial metrics
        monitor = get_monitor()
        current_metrics = monitor.get_current_metrics()
        
        initial_message = WebSocketMessage(
            type="metrics_update",
            data=current_metrics,
            timestamp=datetime.now().timestamp()
        )
        
        await manager.send_personal_message(
            initial_message.json(),
            websocket
        )
        
        # Subscribe to monitoring events
        def metrics_callback(data):
            """Callback for metrics updates."""
            message = WebSocketMessage(
                type="metrics_update",
                data=data,
                timestamp=datetime.now().timestamp()
            )
            
            # Schedule message to be sent
            asyncio.create_task(
                manager.send_personal_message(message.json(), websocket)
            )
        
        # Add callback
        monitor.add_performance_callback(metrics_callback)
        
        # Keep connection alive
        while True:
            try:
                # Wait for client message or ping
                data = await websocket.receive_text()
                
                # Handle client messages
                try:
                    message = json.loads(data)
                    if message.get("type") == "ping":
                        await websocket.send_text(json.dumps({"type": "pong"}))
                    elif message.get("type") == "get_metrics":
                        current_metrics = monitor.get_current_metrics()
                        response = WebSocketMessage(
                            type="metrics_update",
                            data=current_metrics,
                            timestamp=datetime.now().timestamp()
                        )
                        await websocket.send_text(response.json())
                        
                except json.JSONDecodeError:
                    logger.warning(f"Invalid JSON received: {data}")
                
            except WebSocketDisconnect:
                break
                
    except WebSocketDisconnect:
        pass
    except Exception as e:
        logger.error(f"WebSocket error: {e}")
    finally:
        manager.disconnect(websocket)


@router.get("/health")
async def monitoring_health():
    """
    Get monitoring system health.
    
    Returns:
        Health status
    """
    try:
        monitor = get_monitor()
        
        return {
            "status": "healthy" if monitor.monitoring_active else "unhealthy",
            "monitoring_active": monitor.monitoring_active,
            "active_connections": len(manager.active_connections),
            "total_alerts": len(monitor.alerts),
            "active_alerts": len(monitor.get_active_alerts()),
            "metrics_history_size": len(monitor.system_metrics),
            "timestamp": datetime.now().isoformat()
        }
        
    except Exception as e:
        logger.error(f"Failed to get monitoring health: {e}")
        return {
            "status": "unhealthy",
            "error": str(e),
            "timestamp": datetime.now().isoformat()
        }


@router.post("/metrics/reset")
async def reset_metrics():
    """
    Reset all metrics.
    
    Returns:
        Reset status
    """
    try:
        monitor = get_monitor()
        monitor.reset_metrics()
        
        return {"status": "reset", "timestamp": datetime.now().isoformat()}
        
    except Exception as e:
        logger.error(f"Failed to reset metrics: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/performance/timers")
async def get_performance_timers():
    """
    Get performance timer statistics.
    
    Returns:
        Timer statistics
    """
    try:
        monitor = get_monitor()
        summary = monitor.get_metrics_summary(60)  # Last hour
        
        return {
            "timers": summary.get("timers", {}),
            "timestamp": datetime.now().isoformat()
        }
        
    except Exception as e:
        logger.error(f"Failed to get performance timers: {e}")
        raise HTTPException(status_code=500, detail=str(e))