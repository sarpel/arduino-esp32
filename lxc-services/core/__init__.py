"""
Core system components for the audio streaming platform.
Provides centralized configuration, logging, events, and database abstraction.
"""

from .config import get_config
from .logger import get_logger
from .events import get_event_bus
from .database import get_database

__all__ = ['get_config', 'get_logger', 'get_event_bus', 'get_database']