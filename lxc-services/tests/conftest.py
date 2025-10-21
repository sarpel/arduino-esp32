"""
Pytest configuration and fixtures for the audio streaming platform tests.
"""

import pytest
import asyncio
import tempfile
import shutil
from pathlib import Path
from typing import AsyncGenerator, Generator
import numpy as np

# Mock fixtures for testing without actual dependencies
@pytest.fixture
def mock_config():
    """Mock configuration for testing."""
    class MockConfig:
        def __init__(self):
            self.database_host = "localhost"
            self.database_port = 5432
            self.database_name = "test_audio"
            self.redis_host = "localhost"
            self.redis_port = 6379
            self.audio_receiver_host = "0.0.0.0"
            self.audio_receiver_port = 9000
            self.data_dir = tempfile.mkdtemp()
            self.log_level = "DEBUG"
    
    return MockConfig()


@pytest.fixture
def mock_logger():
    """Mock logger for testing."""
    import logging
    return logging.getLogger("test_logger")


@pytest.fixture
def mock_event_bus():
    """Mock event bus for testing."""
    class MockEventBus:
        def __init__(self):
            self.events = []
        
        def emit(self, event):
            self.events.append(event)
        
        def subscribe(self, event_type, callback):
            pass
        
        def unsubscribe(self, event_type, callback):
            pass
    
    return MockEventBus()


@pytest.fixture
def sample_audio_data():
    """Generate sample audio data for testing."""
    # Generate 1 second of sine wave at 440Hz with 16kHz sample rate
    sample_rate = 16000
    duration = 1.0
    frequency = 440.0
    
    t = np.linspace(0, duration, int(sample_rate * duration), False)
    audio_data = np.sin(2 * np.pi * frequency * t).astype(np.float32)
    
    return audio_data, sample_rate


@pytest.fixture
def temp_audio_dir():
    """Create temporary directory for audio files."""
    temp_dir = tempfile.mkdtemp()
    yield Path(temp_dir)
    shutil.rmtree(temp_dir)


@pytest.fixture
def mock_database():
    """Mock database for testing."""
    class MockDatabase:
        def __init__(self):
            self.connected = False
            self.data = {}
        
        def connect(self):
            self.connected = True
        
        def disconnect(self):
            self.connected = False
        
        def execute(self, query, params=None):
            return []
        
        def get_session(self):
            return MockSession()
    
    class MockSession:
        def __init__(self):
            self.committed = False
            self.rolled_back = False
        
        def commit(self):
            self.committed = True
        
        def rollback(self):
            self.rolled_back = True
        
        def close(self):
            pass
        
        def query(self, model):
            return MockQuery()
        
        def add(self, obj):
            pass
        
        def delete(self, obj):
            pass
    
    class MockQuery:
        def filter(self, *args):
            return self
        
        def filter_by(self, **kwargs):
            return self
        
        def order_by(self, *args):
            return self
        
        def limit(self, limit):
            return self
        
        def all(self):
            return []
        
        def first(self):
            return None
        
        def count(self):
            return 0
    
    return MockDatabase()


@pytest.fixture(scope="session")
def event_loop():
    """Create an instance of the default event loop for the test session."""
    loop = asyncio.get_event_loop_policy().new_event_loop()
    yield loop
    loop.close()


@pytest.fixture
async def async_client():
    """Create async HTTP client for testing."""
    import httpx
    async with httpx.AsyncClient() as client:
        yield client


# Audio processing fixtures
@pytest.fixture
def audio_processor():
    """Mock audio processor for testing."""
    class MockAudioProcessor:
        def __init__(self):
            self.processed_count = 0
        
        def process_audio(self, audio_data):
            self.processed_count += 1
            return audio_data
        
        def apply_filter(self, audio_data, filter_type):
            return audio_data
        
        def analyze_quality(self, audio_data):
            return 0.95  # Mock quality score
    
    return MockAudioProcessor()


@pytest.fixture
def compression_manager():
    """Mock compression manager for testing."""
    class MockCompressionManager:
        def __init__(self):
            self.compression_count = 0
        
        def compress(self, audio_data, compression_type="zlib"):
            self.compression_count += 1
            return audio_data.tobytes(), {"ratio": 2.0}
        
        def decompress(self, compressed_data, original_shape):
            return np.frombuffer(compressed_data, dtype=np.float32).reshape(original_shape)
    
    return MockCompressionManager()


@pytest.fixture
def monitoring_system():
    """Mock monitoring system for testing."""
    class MockMonitoringSystem:
        def __init__(self):
            self.metrics = {}
            self.alerts = []
        
        def increment_counter(self, name, value=1):
            self.metrics[name] = self.metrics.get(name, 0) + value
        
        def set_gauge(self, name, value):
            self.metrics[name] = value
        
        def record_timer(self, name, duration):
            if name not in self.metrics:
                self.metrics[name] = []
            self.metrics[name].append(duration)
        
        def get_metrics(self):
            return self.metrics.copy()
        
        def create_alert(self, level, message):
            self.alerts.append({"level": level, "message": message})
    
    return MockMonitoringSystem()


# Network testing fixtures
@pytest.fixture
def mock_tcp_server():
    """Mock TCP server for testing network functionality."""
    import socket
    import threading
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(('localhost', 0))  # Let OS choose port
    port = server_socket.getsockname()[1]
    
    def handle_client(client_socket):
        try:
            while True:
                data = client_socket.recv(1024)
                if not data:
                    break
                client_socket.send(data)  # Echo back
        finally:
            client_socket.close()
    
    server_socket.listen(5)
    server_thread = threading.Thread(target=lambda: None, daemon=True)
    server_thread.start()
    
    yield server_socket, port
    
    server_socket.close()


# Performance testing fixtures
@pytest.fixture
def performance_timer():
    """Context manager for timing performance tests."""
    import time
    
    class Timer:
        def __init__(self):
            self.start_time = None
            self.end_time = None
        
        def __enter__(self):
            self.start_time = time.time()
            return self
        
        def __exit__(self, exc_type, exc_val, exc_tb):
            self.end_time = time.time()
        
        @property
        def duration(self):
            if self.start_time and self.end_time:
                return self.end_time - self.start_time
            return None
    
    return Timer()


# Test markers
def pytest_configure(config):
    """Configure custom pytest markers."""
    config.addinivalue_line(
        "markers", "unit: mark test as a unit test"
    )
    config.addinivalue_line(
        "markers", "integration: mark test as an integration test"
    )
    config.addinivalue_line(
        "markers", "performance: mark test as a performance test"
    )
    config.addinivalue_line(
        "markers", "slow: mark test as slow running"
    )
    config.addinivalue_line(
        "markers", "audio: mark test as audio processing related"
    )
    config.addinivalue_line(
        "markers", "network: mark test as network related"
    )
    config.addinivalue_line(
        "markers", "database: mark test as database related"
    )