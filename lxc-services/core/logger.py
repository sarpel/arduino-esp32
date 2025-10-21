"""
Enhanced logging system for the audio streaming platform.
Provides structured logging with multiple outputs, correlation IDs, and performance tracking.
"""

import logging
import logging.handlers
import json
import time
import uuid
import threading
from pathlib import Path
from typing import Any, Dict, Optional, Union
from datetime import datetime
from enum import Enum
from dataclasses import dataclass, asdict


class LogLevel(Enum):
    """Log levels with numeric values."""
    DEBUG = logging.DEBUG
    INFO = logging.INFO
    WARNING = logging.WARNING
    ERROR = logging.ERROR
    CRITICAL = logging.CRITICAL


@dataclass
class LogContext:
    """Context information for log entries."""
    correlation_id: Optional[str] = None
    user_id: Optional[str] = None
    session_id: Optional[str] = None
    request_id: Optional[str] = None
    device_id: Optional[str] = None
    component: Optional[str] = None
    function: Optional[str] = None
    line_number: Optional[int] = None
    extra: Dict[str, Any] = None
    
    def __post_init__(self):
        if self.extra is None:
            self.extra = {}


class StructuredFormatter(logging.Formatter):
    """Structured JSON formatter for log entries."""
    
    def format(self, record: logging.LogRecord) -> str:
        """Format log record as structured JSON."""
        # Create base log entry
        log_entry = {
            'timestamp': datetime.fromtimestamp(record.created).isoformat(),
            'level': record.levelname,
            'logger': record.name,
            'message': record.getMessage(),
            'module': record.module,
            'function': record.funcName,
            'line': record.lineno,
            'thread': threading.current_thread().name,
            'process': record.process,
        }
        
        # Add context information if available
        if hasattr(record, 'context') and record.context:
            context = record.context
            if isinstance(context, LogContext):
                log_entry['context'] = {
                    'correlation_id': context.correlation_id,
                    'user_id': context.user_id,
                    'session_id': context.session_id,
                    'request_id': context.request_id,
                    'device_id': context.device_id,
                    'component': context.component,
                    'function': context.function,
                    'line_number': context.line_number,
                }
                
                # Add extra context data
                if context.extra:
                    log_entry['context'].update(context.extra)
        
        # Add exception information if present
        if record.exc_info:
            log_entry['exception'] = {
                'type': record.exc_info[0].__name__,
                'message': str(record.exc_info[1]),
                'traceback': self.formatException(record.exc_info)
            }
        
        # Add performance metrics if available
        if hasattr(record, 'duration'):
            log_entry['duration_ms'] = record.duration
        
        if hasattr(record, 'memory_usage'):
            log_entry['memory_usage_mb'] = record.memory_usage
        
        return json.dumps(log_entry, default=str)


class ColoredFormatter(logging.Formatter):
    """Colored formatter for console output."""
    
    # ANSI color codes
    COLORS = {
        'DEBUG': '\033[36m',      # Cyan
        'INFO': '\033[32m',       # Green
        'WARNING': '\033[33m',    # Yellow
        'ERROR': '\033[31m',      # Red
        'CRITICAL': '\033[35m',   # Magenta
        'RESET': '\033[0m'        # Reset
    }
    
    def format(self, record: logging.LogRecord) -> str:
        """Format log record with colors."""
        # Add color to level name
        level_color = self.COLORS.get(record.levelname, self.COLORS['RESET'])
        reset_color = self.COLORS['RESET']
        
        # Format the message
        formatted = super().format(record)
        
        # Add colors
        formatted = formatted.replace(
            f'[{record.levelname}]',
            f'[{level_color}{record.levelname}{reset_color}]'
        )
        
        return formatted


class PerformanceTracker:
    """Performance tracking for operations."""
    
    def __init__(self, logger: 'Logger', operation: str, context: Optional[LogContext] = None):
        self.logger = logger
        self.operation = operation
        self.context = context
        self.start_time = None
        self.start_memory = None
    
    def __enter__(self):
        """Start performance tracking."""
        self.start_time = time.time()
        try:
            import psutil
            process = psutil.Process()
            self.start_memory = process.memory_info().rss / 1024 / 1024  # MB
        except ImportError:
            self.start_memory = None
        
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """End performance tracking and log metrics."""
        if self.start_time:
            duration = (time.time() - self.start_time) * 1000  # ms
            
            extra = {'duration': duration}
            if self.start_memory:
                try:
                    import psutil
                    process = psutil.Process()
                    current_memory = process.memory_info().rss / 1024 / 1024  # MB
                    extra['memory_usage'] = current_memory
                    extra['memory_delta'] = current_memory - self.start_memory
                except ImportError:
                    pass
            
            if exc_type:
                self.logger.error(
                    f"Operation '{self.operation}' failed after {duration:.2f}ms",
                    context=self.context,
                    extra=extra,
                    exc_info=(exc_type, exc_val, exc_tb)
                )
            else:
                self.logger.info(
                    f"Operation '{self.operation}' completed in {duration:.2f}ms",
                    context=self.context,
                    extra=extra
                )


class Logger:
    """
    Enhanced logger with structured logging, context tracking, and performance monitoring.
    """
    
    def __init__(self, name: str, level: Union[str, LogLevel] = LogLevel.INFO):
        """
        Initialize logger.
        
        Args:
            name: Logger name
            level: Log level
        """
        self.name = name
        self.logger = logging.getLogger(name)
        
        # Set log level
        if isinstance(level, str):
            level = LogLevel[level.upper()]
        self.logger.setLevel(level.value)
        
        # Clear existing handlers
        self.logger.handlers.clear()
        
        # Setup default handlers
        self._setup_handlers()
        
        # Thread-local context storage
        self._context = threading.local()
    
    def _setup_handlers(self) -> None:
        """Setup default log handlers."""
        # Console handler with colored output
        console_handler = logging.StreamHandler()
        console_formatter = ColoredFormatter(
            fmt='[%(levelname)s] %(asctime)s - %(name)s - %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
        console_handler.setFormatter(console_formatter)
        self.logger.addHandler(console_handler)
    
    def add_file_handler(self, 
                        file_path: Union[str, Path], 
                        level: Union[str, LogLevel] = LogLevel.INFO,
                        structured: bool = True,
                        max_bytes: int = 10 * 1024 * 1024,  # 10MB
                        backup_count: int = 5) -> None:
        """
        Add file handler to logger.
        
        Args:
            file_path: Path to log file
            level: Log level for this handler
            structured: Use structured JSON format
            max_bytes: Maximum file size before rotation
            backup_count: Number of backup files to keep
        """
        file_path = Path(file_path)
        file_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Create rotating file handler
        file_handler = logging.handlers.RotatingFileHandler(
            file_path,
            maxBytes=max_bytes,
            backupCount=backup_count
        )
        
        # Set formatter
        if structured:
            formatter = StructuredFormatter()
        else:
            formatter = logging.Formatter(
                fmt='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                datefmt='%Y-%m-%d %H:%M:%S'
            )
        
        file_handler.setFormatter(formatter)
        
        # Set level
        if isinstance(level, str):
            level = LogLevel[level.upper()]
        file_handler.setLevel(level.value)
        
        self.logger.addHandler(file_handler)
    
    def set_context(self, context: LogContext) -> None:
        """Set context for subsequent log entries."""
        self._context.value = context
    
    def get_context(self) -> Optional[LogContext]:
        """Get current context."""
        return getattr(self._context, 'value', None)
    
    def clear_context(self) -> None:
        """Clear current context."""
        self._context.value = None
    
    def _log(self, level: LogLevel, message: str, context: Optional[LogContext] = None, 
             extra: Optional[Dict[str, Any]] = None, exc_info=None) -> None:
        """
        Internal logging method with context support.
        
        Args:
            level: Log level
            message: Log message
            context: Log context (overrides current context)
            extra: Extra data to include
            exc_info: Exception information
        """
        # Use provided context or current context
        log_context = context or self.get_context()
        
        # Prepare extra data for log record
        record_extra = {}
        if log_context:
            record_extra['context'] = log_context
        if extra:
            record_extra.update(extra)
        
        # Log the message
        self.logger.log(level.value, message, extra=record_extra, exc_info=exc_info)
    
    def debug(self, message: str, context: Optional[LogContext] = None, 
              extra: Optional[Dict[str, Any]] = None) -> None:
        """Log debug message."""
        self._log(LogLevel.DEBUG, message, context, extra)
    
    def info(self, message: str, context: Optional[LogContext] = None, 
             extra: Optional[Dict[str, Any]] = None) -> None:
        """Log info message."""
        self._log(LogLevel.INFO, message, context, extra)
    
    def warning(self, message: str, context: Optional[LogContext] = None, 
                extra: Optional[Dict[str, Any]] = None) -> None:
        """Log warning message."""
        self._log(LogLevel.WARNING, message, context, extra)
    
    def error(self, message: str, context: Optional[LogContext] = None, 
              extra: Optional[Dict[str, Any]] = None, exc_info=None) -> None:
        """Log error message."""
        self._log(LogLevel.ERROR, message, context, extra, exc_info)
    
    def critical(self, message: str, context: Optional[LogContext] = None, 
                 extra: Optional[Dict[str, Any]] = None, exc_info=None) -> None:
        """Log critical message."""
        self._log(LogLevel.CRITICAL, message, context, extra, exc_info)
    
    def exception(self, message: str, context: Optional[LogContext] = None, 
                  extra: Optional[Dict[str, Any]] = None) -> None:
        """Log exception with traceback."""
        self.error(message, context, extra, exc_info=True)
    
    def track_performance(self, operation: str, context: Optional[LogContext] = None) -> PerformanceTracker:
        """
        Track performance of an operation.
        
        Args:
            operation: Operation name
            context: Log context
            
        Returns:
            Performance tracker context manager
        """
        return PerformanceTracker(self, operation, context)
    
    def log_function_call(self, func_name: str, args: tuple = (), kwargs: dict = None,
                         context: Optional[LogContext] = None) -> None:
        """
        Log function call details.
        
        Args:
            func_name: Function name
            args: Function arguments
            kwargs: Function keyword arguments
            context: Log context
        """
        kwargs = kwargs or {}
        
        # Sanitize arguments for logging (remove sensitive data)
        safe_args = []
        for arg in args:
            if isinstance(arg, (str, int, float, bool, type(None))):
                safe_args.append(arg)
            else:
                safe_args.append(type(arg).__name__)
        
        safe_kwargs = {}
        for k, v in kwargs.items():
            if any(sensitive in k.lower() for sensitive in ['password', 'secret', 'key', 'token']):
                safe_kwargs[k] = '[REDACTED]'
            elif isinstance(v, (str, int, float, bool, type(None))):
                safe_kwargs[k] = v
            else:
                safe_kwargs[k] = type(v).__name__
        
        extra = {
            'function_args': safe_args,
            'function_kwargs': safe_kwargs
        }
        
        self.debug(f"Calling function: {func_name}", context, extra)
    
    def log_api_request(self, method: str, endpoint: str, status_code: int = None,
                       response_time: float = None, user_id: str = None,
                       context: Optional[LogContext] = None) -> None:
        """
        Log API request details.
        
        Args:
            method: HTTP method
            endpoint: API endpoint
            status_code: Response status code
            response_time: Response time in milliseconds
            user_id: User ID
            context: Log context
        """
        extra = {
            'api_method': method,
            'api_endpoint': endpoint,
            'api_status_code': status_code,
            'api_response_time_ms': response_time
        }
        
        if user_id:
            if not context:
                context = LogContext()
            context.user_id = user_id
        
        message = f"API {method} {endpoint}"
        if status_code:
            message += f" -> {status_code}"
        if response_time:
            message += f" ({response_time:.2f}ms)"
        
        if status_code and status_code >= 400:
            self.warning(message, context, extra)
        else:
            self.info(message, context, extra)
    
    def log_device_event(self, device_id: str, event_type: str, details: Dict[str, Any] = None,
                        context: Optional[LogContext] = None) -> None:
        """
        Log device-related event.
        
        Args:
            device_id: Device identifier
            event_type: Type of event
            details: Event details
            context: Log context
        """
        if not context:
            context = LogContext()
        context.device_id = device_id
        
        extra = {
            'device_event_type': event_type,
            'device_event_details': details or {}
        }
        
        self.info(f"Device {device_id}: {event_type}", context, extra)


# Global logger registry
_loggers: Dict[str, Logger] = {}


def get_logger(name: str, level: Union[str, LogLevel] = LogLevel.INFO) -> Logger:
    """
    Get or create logger instance.
    
    Args:
        name: Logger name
        level: Log level
        
    Returns:
        Logger instance
    """
    if name not in _loggers:
        _loggers[name] = Logger(name, level)
    return _loggers[name]


def setup_logging(config: Optional[Dict[str, Any]] = None) -> None:
    """
    Setup logging configuration.
    
    Args:
        config: Logging configuration
    """
    if config is None:
        config = {
            'level': 'INFO',
            'file_handler': {
                'enabled': True,
                'file_path': '/var/log/audio-streamer.log',
                'structured': True,
                'max_bytes': 10 * 1024 * 1024,
                'backup_count': 5
            }
        }
    
    # Configure root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(getattr(logging, config.get('level', 'INFO')))
    
    # Add file handler if configured
    file_config = config.get('file_handler', {})
    if file_config.get('enabled', False):
        logger = get_logger('audio-streamer')
        logger.add_file_handler(
            file_path=file_config.get('file_path', '/var/log/audio-streamer.log'),
            level=file_config.get('level', 'INFO'),
            structured=file_config.get('structured', True),
            max_bytes=file_config.get('max_bytes', 10 * 1024 * 1024),
            backup_count=file_config.get('backup_count', 5)
        )


# Decorator for automatic function logging
def log_function_calls(logger_name: str = None):
    """
    Decorator to automatically log function calls.
    
    Args:
        logger_name: Logger name to use
    """
    def decorator(func):
        def wrapper(*args, **kwargs):
            logger = get_logger(logger_name or func.__module__)
            
            # Get function context
            context = logger.get_context()
            if not context:
                context = LogContext(
                    component=func.__module__,
                    function=func.__name__
                )
            else:
                context.function = func.__name__
            
            # Log function call
            logger.log_function_call(func.__name__, args, kwargs, context)
            
            # Track performance
            with logger.track_performance(f"function:{func.__name__}", context):
                try:
                    result = func(*args, **kwargs)
                    logger.debug(f"Function {func.__name__} completed successfully", context)
                    return result
                except Exception as e:
                    logger.exception(f"Function {func.__name__} failed", context)
                    raise
        
        return wrapper
    return decorator