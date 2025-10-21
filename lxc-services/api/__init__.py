"""
FastAPI REST API for the audio streaming platform.
Provides comprehensive endpoints for audio management, monitoring, and analytics.
"""

from .main import app
from .routes import (
    audio,
    devices,
    monitoring,
    analytics,
    auth,
    system
)

__all__ = [
    'app',
    'audio',
    'devices', 
    'monitoring',
    'analytics',
    'auth',
    'system'
]