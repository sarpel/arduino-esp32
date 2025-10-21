"""
Event bus system for the audio streaming platform.
Provides publish-subscribe messaging with priority handling, async processing, and event persistence.
"""

import asyncio
import threading
import time
import uuid
import json
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Union
from dataclasses import dataclass, field
from concurrent.futures import ThreadPoolExecutor, Future
from collections import defaultdict
import weakref

from .logger import get_logger, LogContext


class EventPriority(Enum):
    """Event priority levels."""
    LOW = 1
    NORMAL = 2
    HIGH = 3
    CRITICAL = 4


class EventStatus(Enum):
    """Event processing status."""
    PENDING = "pending"
    PROCESSING = "processing"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


@dataclass
class Event:
    """Event data structure."""
    event_type: str
    data: Any = None
    source: str = None
    timestamp: float = field(default_factory=time.time)
    correlation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    priority: EventPriority = EventPriority.NORMAL
    metadata: Dict[str, Any] = field(default_factory=dict)
    retry_count: int = 0
    max_retries: int = 3
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert event to dictionary."""
        return {
            'event_type': self.event_type,
            'data': self.data,
            'source': self.source,
            'timestamp': self.timestamp,
            'correlation_id': self.correlation_id,
            'priority': self.priority.name,
            'metadata': self.metadata,
            'retry_count': self.retry_count,
            'max_retries': self.max_retries
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Event':
        """Create event from dictionary."""
        return cls(
            event_type=data['event_type'],
            data=data.get('data'),
            source=data.get('source'),
            timestamp=data.get('timestamp', time.time()),
            correlation_id=data.get('correlation_id', str(uuid.uuid4())),
            priority=EventPriority[data.get('priority', 'NORMAL')],
            metadata=data.get('metadata', {}),
            retry_count=data.get('retry_count', 0),
            max_retries=data.get('max_retries', 3)
        )


@dataclass
class EventHandler:
    """Event handler configuration."""
    callback: Callable
    event_type: str
    priority: EventPriority = EventPriority.NORMAL
    async_handler: bool = False
    filter_func: Optional[Callable[[Event], bool]] = None
    max_retries: int = 3
    timeout: Optional[float] = None
    weak_ref: bool = False
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    
    def __post_init__(self):
        """Initialize handler."""
        if self.weak_ref and hasattr(self.callback, '__self__'):
            # Create weak reference to bound method
            self.callback = weakref.WeakMethod(self.callback)
        elif self.weak_ref:
            # Create weak reference to function
            self.callback = weakref.ref(self.callback)


@dataclass
class EventProcessingResult:
    """Result of event processing."""
    event: Event
    handler_id: str
    status: EventStatus
    result: Any = None
    error: Optional[Exception] = None
    processing_time: float = 0.0
    timestamp: float = field(default_factory=time.time)


class EventBus:
    """
    Event bus for publish-subscribe messaging.
    Supports priority handling, async processing, and event persistence.
    """
    
    def __init__(self, name: str = "default", max_workers: int = 10, enable_persistence: bool = False):
        """
        Initialize event bus.
        
        Args:
            name: Event bus name
            max_workers: Maximum number of worker threads
            enable_persistence: Enable event persistence
        """
        self.name = name
        self.logger = get_logger(f"eventbus.{name}")
        
        # Event handlers by event type
        self._handlers: Dict[str, List[EventHandler]] = defaultdict(list)
        
        # Thread pool for async processing
        self._executor = ThreadPoolExecutor(max_workers=max_workers, thread_name_prefix=f"eventbus-{name}")
        
        # Event processing statistics
        self._stats = {
            'events_published': 0,
            'events_processed': 0,
            'events_failed': 0,
            'handlers_registered': 0,
            'handlers_executed': 0
        }
        
        # Event history (for debugging)
        self._event_history: List[EventProcessingResult] = []
        self._max_history_size = 1000
        
        # Persistence
        self._enable_persistence = enable_persistence
        self._persistence_lock = threading.Lock()
        
        # Shutdown flag
        self._shutdown = False
        
        self.logger.info(f"EventBus '{name}' initialized", extra={
            'max_workers': max_workers,
            'enable_persistence': enable_persistence
        })
    
    def subscribe(self, 
                  event_type: str, 
                  callback: Callable,
                  priority: EventPriority = EventPriority.NORMAL,
                  async_handler: bool = False,
                  filter_func: Optional[Callable[[Event], bool]] = None,
                  max_retries: int = 3,
                  timeout: Optional[float] = None,
                  weak_ref: bool = False) -> str:
        """
        Subscribe to events.
        
        Args:
            event_type: Event type to subscribe to
            callback: Handler callback function
            priority: Handler priority
            async_handler: Run handler asynchronously
            filter_func: Filter function for events
            max_retries: Maximum retry attempts
            timeout: Handler timeout in seconds
            weak_ref: Use weak reference for callback
            
        Returns:
            Handler ID
        """
        handler = EventHandler(
            callback=callback,
            event_type=event_type,
            priority=priority,
            async_handler=async_handler,
            filter_func=filter_func,
            max_retries=max_retries,
            timeout=timeout,
            weak_ref=weak_ref
        )
        
        # Add handler to list
        self._handlers[event_type].append(handler)
        
        # Sort handlers by priority (higher priority first)
        self._handlers[event_type].sort(key=lambda h: h.priority.value, reverse=True)
        
        self._stats['handlers_registered'] += 1
        
        self.logger.debug(f"Handler registered for event type '{event_type}'", extra={
            'handler_id': handler.id,
            'priority': priority.name,
            'async': async_handler
        })
        
        return handler.id
    
    def unsubscribe(self, handler_id: str) -> bool:
        """
        Unsubscribe handler.
        
        Args:
            handler_id: Handler ID to unsubscribe
            
        Returns:
            True if handler was found and removed
        """
        for event_type, handlers in self._handlers.items():
            for i, handler in enumerate(handlers):
                if handler.id == handler_id:
                    del handlers[i]
                    self._stats['handlers_registered'] -= 1
                    
                    self.logger.debug(f"Handler unsubscribed from event type '{event_type}'", extra={
                        'handler_id': handler_id
                    })
                    
                    return True
        
        return False
    
    def publish(self, 
                event_type: str, 
                data: Any = None,
                source: str = None,
                priority: EventPriority = EventPriority.NORMAL,
                metadata: Dict[str, Any] = None,
                correlation_id: str = None) -> Event:
        """
        Publish event.
        
        Args:
            event_type: Event type
            data: Event data
            source: Event source
            priority: Event priority
            metadata: Event metadata
            correlation_id: Correlation ID for event tracing
            
        Returns:
            Published event
        """
        if self._shutdown:
            raise RuntimeError("EventBus is shutdown")
        
        event = Event(
            event_type=event_type,
            data=data,
            source=source,
            priority=priority,
            metadata=metadata or {},
            correlation_id=correlation_id or str(uuid.uuid4())
        )
        
        self._stats['events_published'] += 1
        
        # Persist event if enabled
        if self._enable_persistence:
            self._persist_event(event)
        
        # Get handlers for this event type
        handlers = self._handlers.get(event_type, [])
        
        if not handlers:
            self.logger.debug(f"No handlers registered for event type '{event_type}'", extra={
                'event_id': event.correlation_id
            })
            return event
        
        # Process event
        self._process_event(event, handlers)
        
        return event
    
    def _process_event(self, event: Event, handlers: List[EventHandler]) -> None:
        """Process event with handlers."""
        for handler in handlers:
            # Check if handler is still valid (for weak references)
            if handler.weak_ref:
                callback = handler.callback()
                if callback is None:
                    # Handler has been garbage collected, remove it
                    self._handlers[event.event_type].remove(handler)
                    continue
            else:
                callback = handler.callback
            
            # Apply filter if present
            if handler.filter_func and not handler.filter_func(event):
                continue
            
            # Process handler
            if handler.async_handler:
                future = self._executor.submit(self._execute_handler, event, handler, callback)
            else:
                self._execute_handler(event, handler, callback)
    
    def _execute_handler(self, event: Event, handler: EventHandler, callback: Callable) -> EventProcessingResult:
        """Execute event handler."""
        start_time = time.time()
        
        try:
            # Create context for logging
            context = LogContext(
                correlation_id=event.correlation_id,
                component=self.name,
                function=callback.__name__ if hasattr(callback, '__name__') else str(callback)
            )
            
            self.logger.debug(f"Executing handler for event '{event.event_type}'", context, extra={
                'handler_id': handler.id,
                'event_priority': event.priority.name
            })
            
            # Execute handler with timeout if specified
            if handler.timeout:
                future = self._executor.submit(callback, event)
                result = future.result(timeout=handler.timeout)
            else:
                result = callback(event)
            
            processing_time = time.time() - start_time
            
            # Create success result
            processing_result = EventProcessingResult(
                event=event,
                handler_id=handler.id,
                status=EventStatus.COMPLETED,
                result=result,
                processing_time=processing_time
            )
            
            self._stats['events_processed'] += 1
            self._stats['handlers_executed'] += 1
            
            self.logger.debug(f"Handler completed successfully", context, extra={
                'handler_id': handler.id,
                'processing_time_ms': processing_time * 1000
            })
            
            return processing_result
            
        except Exception as e:
            processing_time = time.time() - start_time
            
            # Create failure result
            processing_result = EventProcessingResult(
                event=event,
                handler_id=handler.id,
                status=EventStatus.FAILED,
                error=e,
                processing_time=processing_time
            )
            
            self._stats['events_failed'] += 1
            
            self.logger.error(f"Handler execution failed", extra={
                'handler_id': handler.id,
                'error': str(e),
                'processing_time_ms': processing_time * 1000
            })
            
            # Retry logic
            if event.retry_count < handler.max_retries:
                event.retry_count += 1
                self.logger.info(f"Retrying event handler", extra={
                    'handler_id': handler.id,
                    'retry_count': event.retry_count,
                    'max_retries': handler.max_retries
                })
                
                # Schedule retry with exponential backoff
                delay = 2 ** event.retry_count
                timer = threading.Timer(delay, self._execute_handler, args=[event, handler, callback])
                timer.start()
            else:
                self.logger.error(f"Handler failed after maximum retries", extra={
                    'handler_id': handler.id,
                    'retry_count': event.retry_count,
                    'max_retries': handler.max_retries
                })
            
            return processing_result
        
        finally:
            # Add to history
            self._add_to_history(processing_result)
    
    def _add_to_history(self, result: EventProcessingResult) -> None:
        """Add processing result to history."""
        self._event_history.append(result)
        
        # Trim history if too large
        if len(self._event_history) > self._max_history_size:
            self._event_history = self._event_history[-self._max_history_size:]
    
    def _persist_event(self, event: Event) -> None:
        """Persist event to storage."""
        # This would integrate with a database or file system
        # For now, just log the event
        self.logger.debug(f"Persisting event '{event.event_type}'", extra={
            'event_id': event.correlation_id,
            'event_data': event.to_dict()
        })
    
    def get_stats(self) -> Dict[str, Any]:
        """Get event bus statistics."""
        return {
            'name': self.name,
            'stats': self._stats.copy(),
            'handlers_by_event_type': {
                event_type: len(handlers) 
                for event_type, handlers in self._handlers.items()
            },
            'event_history_size': len(self._event_history),
            'shutdown': self._shutdown
        }
    
    def get_event_history(self, 
                         event_type: str = None,
                         status: EventStatus = None,
                         limit: int = 100) -> List[EventProcessingResult]:
        """
        Get event processing history.
        
        Args:
            event_type: Filter by event type
            status: Filter by status
            limit: Maximum number of results
            
        Returns:
            List of processing results
        """
        history = self._event_history
        
        # Apply filters
        if event_type:
            history = [r for r in history if r.event.event_type == event_type]
        
        if status:
            history = [r for r in history if r.status == status]
        
        # Sort by timestamp (newest first) and limit
        history.sort(key=lambda r: r.timestamp, reverse=True)
        
        return history[:limit]
    
    def clear_history(self) -> None:
        """Clear event history."""
        self._event_history.clear()
        self.logger.info("Event history cleared")
    
    def shutdown(self, wait: bool = True, timeout: float = 30.0) -> None:
        """
        Shutdown event bus.
        
        Args:
            wait: Wait for pending events to complete
            timeout: Maximum time to wait
        """
        self.logger.info("Shutting down EventBus")
        
        self._shutdown = True
        
        if wait:
            self._executor.shutdown(wait=True)
        
        self.logger.info("EventBus shutdown complete")
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.shutdown()


# Global event bus registry
_event_buses: Dict[str, EventBus] = {}


def get_event_bus(name: str = "default", **kwargs) -> EventBus:
    """
    Get or create event bus instance.
    
    Args:
        name: Event bus name
        **kwargs: Event bus initialization arguments
        
    Returns:
        EventBus instance
    """
    if name not in _event_buses:
        _event_buses[name] = EventBus(name, **kwargs)
    return _event_buses[name]


def shutdown_all_event_buses(wait: bool = True, timeout: float = 30.0) -> None:
    """
    Shutdown all event buses.
    
    Args:
        wait: Wait for pending events to complete
        timeout: Maximum time to wait
    """
    for event_bus in _event_buses.values():
        event_bus.shutdown(wait, timeout)
    
    _event_buses.clear()


# Decorator for event handlers
def event_handler(event_type: str, 
                 priority: EventPriority = EventPriority.NORMAL,
                 async_handler: bool = False,
                 event_bus: str = "default",
                 **kwargs):
    """
    Decorator to register function as event handler.
    
    Args:
        event_type: Event type to handle
        priority: Handler priority
        async_handler: Run handler asynchronously
        event_bus: Event bus name
        **kwargs: Additional handler arguments
    """
    def decorator(func):
        bus = get_event_bus(event_bus)
        bus.subscribe(event_type, func, priority, async_handler, **kwargs)
        return func
    return decorator


# Convenience functions for common event types
def publish_system_event(event_type: str, data: Any = None, **kwargs) -> Event:
    """Publish system event."""
    return get_event_bus().publish(f"system.{event_type}", data, source="system", **kwargs)


def publish_audio_event(event_type: str, data: Any = None, **kwargs) -> Event:
    """Publish audio-related event."""
    return get_event_bus().publish(f"audio.{event_type}", data, source="audio", **kwargs)


def publish_device_event(device_id: str, event_type: str, data: Any = None, **kwargs) -> Event:
    """Publish device-specific event."""
    metadata = kwargs.get('metadata', {})
    metadata['device_id'] = device_id
    kwargs['metadata'] = metadata
    
    return get_event_bus().publish(f"device.{event_type}", data, source=f"device:{device_id}", **kwargs)


def publish_web_event(event_type: str, data: Any = None, **kwargs) -> Event:
    """Publish web UI event."""
    return get_event_bus().publish(f"web.{event_type}", data, source="web", **kwargs)