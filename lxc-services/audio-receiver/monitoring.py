"""
Audio Receiver Monitoring Module

Provides comprehensive monitoring, metrics collection, and performance
analysis for the audio receiver system.
"""

import time
import psutil
import threading
from typing import Dict, List, Optional, Any, Callable
from dataclasses import dataclass, field
from enum import Enum
from collections import deque, defaultdict
import statistics
import json

from ..core.logger import get_logger
from ..core.events import get_event_bus, Event, EventPriority
from ..core.config import get_config

logger = get_logger(__name__)
config = get_config()


class AlertLevel(Enum):
    """Alert severity levels."""
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


@dataclass
class SystemMetrics:
    """System performance metrics."""
    timestamp: float
    cpu_percent: float
    memory_percent: float
    memory_used_mb: float
    disk_usage_percent: float
    network_io: Dict[str, int]
    active_connections: int
    thread_count: int


@dataclass
class AudioMetrics:
    """Audio processing metrics."""
    timestamp: float
    devices_active: int
    total_bytes_received: int
    total_bytes_processed: int
    processing_latency_ms: float
    buffer_utilization: float
    dropped_packets: int
    quality_score: float
    compression_ratio: float


@dataclass
class PerformanceAlert:
    """Performance alert definition."""
    level: AlertLevel
    metric: str
    threshold: float
    message: str
    timestamp: float
    resolved: bool = False
    resolved_timestamp: Optional[float] = None


@dataclass
class MonitoringConfig:
    """Monitoring configuration."""
    metrics_interval: float = 1.0
    history_size: int = 1000
    alert_cooldown: float = 60.0
    cpu_threshold: float = 80.0
    memory_threshold: float = 85.0
    disk_threshold: float = 90.0
    latency_threshold: float = 100.0
    buffer_threshold: float = 80.0
    quality_threshold: float = 0.7


class AudioReceiverMonitor:
    """Comprehensive monitoring system for audio receiver."""
    
    def __init__(self, config: Optional[MonitoringConfig] = None):
        self.config = config or MonitoringConfig()
        self.event_bus = get_event_bus()
        
        # Metrics storage
        self.system_metrics: deque = deque(maxlen=self.config.history_size)
        self.audio_metrics: deque = deque(maxlen=self.config.history_size)
        self.alerts: List[PerformanceAlert] = []
        self.alert_history: deque = deque(maxlen=self.config.history_size)
        
        # Real-time counters
        self.counters: Dict[str, float] = defaultdict(float)
        self.gauges: Dict[str, float] = defaultdict(float)
        self.timers: Dict[str, List[float]] = defaultdict(list)
        
        # Monitoring state
        self.monitoring_active = False
        self.monitor_thread: Optional[threading.Thread] = None
        self.last_alert_times: Dict[str, float] = {}
        
        # Performance tracking
        self.performance_callbacks: List[Callable] = []
        
        logger.info("AudioReceiverMonitor initialized")
    
    def start_monitoring(self) -> None:
        """Start the monitoring thread."""
        if self.monitoring_active:
            logger.warning("Monitoring is already active")
            return
        
        self.monitoring_active = True
        self.monitor_thread = threading.Thread(target=self._monitoring_loop, daemon=True)
        self.monitor_thread.start()
        
        logger.info("Audio receiver monitoring started")
        
        # Emit monitoring started event
        self.event_bus.emit(Event(
            event_type="monitoring.started",
            source="AudioReceiverMonitor",
            data={"interval": self.config.metrics_interval}
        ))
    
    def stop_monitoring(self) -> None:
        """Stop the monitoring thread."""
        if not self.monitoring_active:
            return
        
        self.monitoring_active = False
        
        if self.monitor_thread and self.monitor_thread.is_alive():
            self.monitor_thread.join(timeout=5.0)
        
        logger.info("Audio receiver monitoring stopped")
        
        # Emit monitoring stopped event
        self.event_bus.emit(Event(
            event_type="monitoring.stopped",
            source="AudioReceiverMonitor"
        ))
    
    def _monitoring_loop(self) -> None:
        """Main monitoring loop."""
        while self.monitoring_active:
            try:
                # Collect system metrics
                system_metrics = self._collect_system_metrics()
                self.system_metrics.append(system_metrics)
                
                # Collect audio metrics
                audio_metrics = self._collect_audio_metrics()
                self.audio_metrics.append(audio_metrics)
                
                # Check for alerts
                self._check_alerts(system_metrics, audio_metrics)
                
                # Update performance callbacks
                self._update_performance_callbacks(system_metrics, audio_metrics)
                
                # Sleep until next collection
                time.sleep(self.config.metrics_interval)
                
            except Exception as e:
                logger.error(f"Monitoring loop error: {e}")
                time.sleep(self.config.metrics_interval)
    
    def _collect_system_metrics(self) -> SystemMetrics:
        """Collect system performance metrics."""
        # CPU and memory
        cpu_percent = psutil.cpu_percent(interval=None)
        memory = psutil.virtual_memory()
        disk = psutil.disk_usage('/')
        
        # Network I/O
        network = psutil.net_io_counters()
        network_io = {
            'bytes_sent': network.bytes_sent,
            'bytes_recv': network.bytes_recv,
            'packets_sent': network.packets_sent,
            'packets_recv': network.packets_recv
        }
        
        # Process information
        process = psutil.Process()
        
        return SystemMetrics(
            timestamp=time.time(),
            cpu_percent=cpu_percent,
            memory_percent=memory.percent,
            memory_used_mb=memory.used / 1024 / 1024,
            disk_usage_percent=disk.percent,
            network_io=network_io,
            active_connections=len(process.connections()),
            thread_count=process.num_threads()
        )
    
    def _collect_audio_metrics(self) -> AudioMetrics:
        """Collect audio processing metrics."""
        # Get metrics from counters and gauges
        devices_active = int(self.gauges.get('devices_active', 0))
        total_bytes_received = self.counters.get('bytes_received', 0)
        total_bytes_processed = self.counters.get('bytes_processed', 0)
        processing_latency_ms = self.gauges.get('processing_latency_ms', 0)
        buffer_utilization = self.gauges.get('buffer_utilization', 0)
        dropped_packets = int(self.counters.get('dropped_packets', 0))
        quality_score = self.gauges.get('quality_score', 1.0)
        compression_ratio = self.gauges.get('compression_ratio', 1.0)
        
        return AudioMetrics(
            timestamp=time.time(),
            devices_active=devices_active,
            total_bytes_received=int(total_bytes_received),
            total_bytes_processed=int(total_bytes_processed),
            processing_latency_ms=processing_latency_ms,
            buffer_utilization=buffer_utilization,
            dropped_packets=dropped_packets,
            quality_score=quality_score,
            compression_ratio=compression_ratio
        )
    
    def _check_alerts(self, system_metrics: SystemMetrics, audio_metrics: AudioMetrics) -> None:
        """Check for performance alerts."""
        current_time = time.time()
        
        # System alerts
        self._check_metric_alert(
            'cpu', system_metrics.cpu_percent, self.config.cpu_threshold,
            f"High CPU usage: {system_metrics.cpu_percent:.1f}%", current_time
        )
        
        self._check_metric_alert(
            'memory', system_metrics.memory_percent, self.config.memory_threshold,
            f"High memory usage: {system_metrics.memory_percent:.1f}%", current_time
        )
        
        self._check_metric_alert(
            'disk', system_metrics.disk_usage_percent, self.config.disk_threshold,
            f"High disk usage: {system_metrics.disk_usage_percent:.1f}%", current_time
        )
        
        # Audio alerts
        self._check_metric_alert(
            'latency', audio_metrics.processing_latency_ms, self.config.latency_threshold,
            f"High processing latency: {audio_metrics.processing_latency_ms:.1f}ms", current_time
        )
        
        self._check_metric_alert(
            'buffer', audio_metrics.buffer_utilization, self.config.buffer_threshold,
            f"High buffer utilization: {audio_metrics.buffer_utilization:.1f}%", current_time
        )
        
        self._check_metric_alert(
            'quality', audio_metrics.quality_score, self.config.quality_threshold,
            f"Low audio quality: {audio_metrics.quality_score:.2f}", current_time,
            lower_better=True
        )
    
    def _check_metric_alert(self, metric_name: str, value: float, threshold: float,
                           message: str, current_time: float, lower_better: bool = False) -> None:
        """Check if metric exceeds threshold and create alert."""
        # Check if alert should be triggered
        if lower_better:
            should_alert = value < threshold
        else:
            should_alert = value > threshold
        
        if not should_alert:
            # Check if we need to resolve an existing alert
            self._resolve_alert(metric_name, current_time)
            return
        
        # Check cooldown
        last_alert_time = self.last_alert_times.get(metric_name, 0)
        if current_time - last_alert_time < self.config.alert_cooldown:
            return
        
        # Determine alert level
        if metric_name in ['cpu', 'memory', 'disk']:
            level = AlertLevel.CRITICAL if value > threshold * 1.2 else AlertLevel.ERROR
        elif metric_name in ['latency', 'buffer']:
            level = AlertLevel.ERROR if value > threshold * 1.5 else AlertLevel.WARNING
        else:
            level = AlertLevel.WARNING
        
        # Create alert
        alert = PerformanceAlert(
            level=level,
            metric=metric_name,
            threshold=threshold,
            message=message,
            timestamp=current_time
        )
        
        self.alerts.append(alert)
        self.alert_history.append(alert)
        self.last_alert_times[metric_name] = current_time
        
        logger.warning(f"Performance alert: {message}")
        
        # Emit alert event
        self.event_bus.emit(Event(
            event_type="monitoring.alert",
            source="AudioReceiverMonitor",
            data={
                'level': level.value,
                'metric': metric_name,
                'value': value,
                'threshold': threshold,
                'message': message
            },
            priority=EventPriority.HIGH
        ))
    
    def _resolve_alert(self, metric_name: str, current_time: float) -> None:
        """Resolve existing alerts for a metric."""
        for alert in self.alerts:
            if alert.metric == metric_name and not alert.resolved:
                alert.resolved = True
                alert.resolved_timestamp = current_time
                
                logger.info(f"Alert resolved: {alert.metric}")
                
                # Emit alert resolved event
                self.event_bus.emit(Event(
                    event_type="monitoring.alert_resolved",
                    source="AudioReceiverMonitor",
                    data={
                        'metric': metric_name,
                        'duration': current_time - alert.timestamp
                    }
                ))
    
    def _update_performance_callbacks(self, system_metrics: SystemMetrics, 
                                    audio_metrics: AudioMetrics) -> None:
        """Update registered performance callbacks."""
        data = {
            'system': system_metrics,
            'audio': audio_metrics,
            'counters': dict(self.counters),
            'gauges': dict(self.gauges)
        }
        
        for callback in self.performance_callbacks:
            try:
                callback(data)
            except Exception as e:
                logger.error(f"Performance callback error: {e}")
    
    def increment_counter(self, name: str, value: float = 1.0) -> None:
        """Increment a counter metric."""
        self.counters[name] += value
    
    def set_gauge(self, name: str, value: float) -> None:
        """Set a gauge metric."""
        self.gauges[name] = value
    
    def record_timer(self, name: str, duration: float) -> None:
        """Record a timer metric."""
        self.timers[name].append(duration)
        
        # Keep only recent values
        if len(self.timers[name]) > 1000:
            self.timers[name] = self.timers[name][-500:]
    
    def add_performance_callback(self, callback: Callable[[Dict[str, Any]], None]) -> None:
        """Add a performance monitoring callback."""
        self.performance_callbacks.append(callback)
    
    def remove_performance_callback(self, callback: Callable[[Dict[str, Any]], None]) -> None:
        """Remove a performance monitoring callback."""
        if callback in self.performance_callbacks:
            self.performance_callbacks.remove(callback)
    
    def get_current_metrics(self) -> Dict[str, Any]:
        """Get current system and audio metrics."""
        system = self.system_metrics[-1] if self.system_metrics else None
        audio = self.audio_metrics[-1] if self.audio_metrics else None
        
        return {
            'system': system.__dict__ if system else None,
            'audio': audio.__dict__ if audio else None,
            'counters': dict(self.counters),
            'gauges': dict(self.gauges),
            'active_alerts': len([a for a in self.alerts if not a.resolved]),
            'total_alerts': len(self.alerts)
        }
    
    def get_metrics_summary(self, duration_minutes: int = 60) -> Dict[str, Any]:
        """Get metrics summary for the specified duration."""
        cutoff_time = time.time() - (duration_minutes * 60)
        
        # Filter metrics by time
        recent_system = [m for m in self.system_metrics if m.timestamp >= cutoff_time]
        recent_audio = [m for m in self.audio_metrics if m.timestamp >= cutoff_time]
        
        summary = {
            'duration_minutes': duration_minutes,
            'system_samples': len(recent_system),
            'audio_samples': len(recent_audio)
        }
        
        if recent_system:
            summary['system'] = {
                'cpu_avg': statistics.mean(m.cpu_percent for m in recent_system),
                'cpu_max': max(m.cpu_percent for m in recent_system),
                'memory_avg': statistics.mean(m.memory_percent for m in recent_system),
                'memory_max': max(m.memory_percent for m in recent_system),
                'disk_avg': statistics.mean(m.disk_usage_percent for m in recent_system),
                'disk_max': max(m.disk_usage_percent for m in recent_system)
            }
        
        if recent_audio:
            summary['audio'] = {
                'devices_avg': statistics.mean(m.devices_active for m in recent_audio),
                'devices_max': max(m.devices_active for m in recent_audio),
                'latency_avg': statistics.mean(m.processing_latency_ms for m in recent_audio),
                'latency_max': max(m.processing_latency_ms for m in recent_audio),
                'quality_avg': statistics.mean(m.quality_score for m in recent_audio),
                'quality_min': min(m.quality_score for m in recent_audio),
                'total_bytes_received': sum(m.total_bytes_received for m in recent_audio),
                'total_dropped_packets': sum(m.dropped_packets for m in recent_audio)
            }
        
        # Timer statistics
        summary['timers'] = {}
        for name, values in self.timers.items():
            if values:
                summary['timers'][name] = {
                    'count': len(values),
                    'avg': statistics.mean(values),
                    'min': min(values),
                    'max': max(values),
                    'p95': statistics.quantiles(values, n=20)[18] if len(values) > 20 else max(values)
                }
        
        return summary
    
    def get_active_alerts(self) -> List[PerformanceAlert]:
        """Get all active (unresolved) alerts."""
        return [alert for alert in self.alerts if not alert.resolved]
    
    def resolve_alert(self, alert_id: int) -> bool:
        """Manually resolve an alert by ID."""
        for i, alert in enumerate(self.alerts):
            if i == alert_id and not alert.resolved:
                alert.resolved = True
                alert.resolved_timestamp = time.time()
                return True
        return False
    
    def clear_resolved_alerts(self) -> int:
        """Clear all resolved alerts and return count cleared."""
        resolved_count = len([a for a in self.alerts if a.resolved])
        self.alerts = [a for a in self.alerts if not a.resolved]
        return resolved_count
    
    def export_metrics(self, filename: str, format: str = 'json') -> bool:
        """Export metrics to file."""
        try:
            data = {
                'export_timestamp': time.time(),
                'current_metrics': self.get_current_metrics(),
                'metrics_summary': self.get_metrics_summary(60),  # Last hour
                'active_alerts': [alert.__dict__ for alert in self.get_active_alerts()],
                'alert_history': [alert.__dict__ for alert in list(self.alert_history)[-100:]]
            }
            
            if format.lower() == 'json':
                with open(filename, 'w') as f:
                    json.dump(data, f, indent=2, default=str)
            else:
                raise ValueError(f"Unsupported format: {format}")
            
            logger.info(f"Metrics exported to {filename}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to export metrics: {e}")
            return False
    
    def reset_metrics(self) -> None:
        """Reset all metrics and counters."""
        self.counters.clear()
        self.gauges.clear()
        self.timers.clear()
        self.system_metrics.clear()
        self.audio_metrics.clear()
        
        logger.info("All metrics reset")


# Global monitor instance
_monitor_instance: Optional[AudioReceiverMonitor] = None


def get_monitor() -> AudioReceiverMonitor:
    """Get global audio receiver monitor instance."""
    global _monitor_instance
    if _monitor_instance is None:
        _monitor_instance = AudioReceiverMonitor()
    return _monitor_instance


def start_monitoring() -> None:
    """Start global monitoring."""
    monitor = get_monitor()
    monitor.start_monitoring()


def stop_monitoring() -> None:
    """Stop global monitoring."""
    monitor = get_monitor()
    monitor.stop_monitoring()


def increment_counter(name: str, value: float = 1.0) -> None:
    """Increment a global counter metric."""
    monitor = get_monitor()
    monitor.increment_counter(name, value)


def set_gauge(name: str, value: float) -> None:
    """Set a global gauge metric."""
    monitor = get_monitor()
    monitor.set_gauge(name, value)


def record_timer(name: str, duration: float) -> None:
    """Record a global timer metric."""
    monitor = get_monitor()
    monitor.record_timer(name, duration)


# Context manager for performance timing
class PerformanceTimer:
    """Context manager for timing operations."""
    
    def __init__(self, name: str):
        self.name = name
        self.start_time = None
    
    def __enter__(self):
        self.start_time = time.time()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.start_time is not None:
            duration = time.time() - self.start_time
            record_timer(self.name, duration)


def timer(name: str) -> PerformanceTimer:
    """Create a performance timer context manager."""
    return PerformanceTimer(name)