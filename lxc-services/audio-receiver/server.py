"""
Enhanced audio receiver server with multi-device support and monitoring.
"""

import socket
import threading
import time
import uuid
from datetime import datetime
from typing import Dict, List, Optional, Callable, Any
from dataclasses import dataclass, field
from enum import Enum
import json
import struct

from ..core.logger import get_logger, LogContext
from ..core.events import get_event_bus, Event, EventPriority
from ..core.config import get_config


class DeviceStatus(Enum):
    """Device connection status."""
    DISCONNECTED = "disconnected"
    CONNECTING = "connecting"
    CONNECTED = "connected"
    STREAMING = "streaming"
    ERROR = "error"


class AudioFormat(Enum):
    """Supported audio formats."""
    PCM_16BIT = "pcm_16bit"
    PCM_24BIT = "pcm_24bit"
    OPUS = "opus"
    FLAC = "flac"


@dataclass
class DeviceInfo:
    """Device information."""
    device_id: str
    ip_address: str
    port: int
    user_agent: str = None
    connected_at: datetime = field(default_factory=datetime.now)
    last_activity: datetime = field(default_factory=datetime.now)
    status: DeviceStatus = DeviceStatus.DISCONNECTED
    audio_format: AudioFormat = AudioFormat.PCM_16BIT
    sample_rate: int = 16000
    channels: int = 1
    bits_per_sample: int = 16
    bytes_received: int = 0
    chunks_received: int = 0
    errors: int = 0
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            'device_id': self.device_id,
            'ip_address': self.ip_address,
            'port': self.port,
            'user_agent': self.user_agent,
            'connected_at': self.connected_at.isoformat(),
            'last_activity': self.last_activity.isoformat(),
            'status': self.status.value,
            'audio_format': self.audio_format.value,
            'sample_rate': self.sample_rate,
            'channels': self.channels,
            'bits_per_sample': self.bits_per_sample,
            'bytes_received': self.bytes_received,
            'chunks_received': self.chunks_received,
            'errors': self.errors,
            'metadata': self.metadata
        }


@dataclass
class AudioChunk:
    """Audio chunk data."""
    device_id: str
    data: bytes
    timestamp: float = field(default_factory=time.time)
    sequence_number: int = 0
    chunk_size: int = 0
    format: AudioFormat = AudioFormat.PCM_16BIT


class DeviceConnection:
    """Manages a single device connection."""
    
    def __init__(self, connection: socket.socket, address: tuple, server: 'AudioReceiverServer'):
        self.connection = connection
        self.address = address
        self.server = server
        self.logger = get_logger(f"device.{address[0]}")
        
        # Device information
        self.device_info = DeviceInfo(
            device_id=str(uuid.uuid4()),
            ip_address=address[0],
            port=address[1]
        )
        
        # Connection state
        self.is_running = False
        self.receive_thread = None
        
        # Statistics
        self.start_time = time.time()
        self.last_chunk_time = 0
        
        # Configure socket
        self._configure_socket()
    
    def _configure_socket(self) -> None:
        """Configure socket options."""
        try:
            # TCP_NODELAY for low latency
            self.connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            
            # Increase receive buffer
            self.connection.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            
            # Set timeout
            config = get_config()
            self.connection.settimeout(config.audio_receiver.timeout)
            
        except Exception as e:
            self.logger.error(f"Failed to configure socket: {e}")
    
    def start(self) -> None:
        """Start handling device connection."""
        self.is_running = True
        self.device_info.status = DeviceStatus.CONNECTED
        
        # Start receive thread
        self.receive_thread = threading.Thread(
            target=self._receive_loop,
            name=f"Device-{self.device_info.device_id[:8]}"
        )
        self.receive_thread.start()
        
        # Publish connection event
        self.server._publish_device_event(self.device_info.device_id, "connected", {
            'ip_address': self.device_info.ip_address,
            'port': self.device_info.port
        })
        
        self.logger.info(f"Device connected", extra={
            'device_id': self.device_info.device_id,
            'ip_address': self.device_info.ip_address
        })
    
    def stop(self) -> None:
        """Stop handling device connection."""
        self.is_running = False
        self.device_info.status = DeviceStatus.DISCONNECTED
        
        # Close connection
        try:
            self.connection.close()
        except Exception:
            pass
        
        # Wait for thread to finish
        if self.receive_thread and self.receive_thread.is_alive():
            self.receive_thread.join(timeout=5)
        
        # Publish disconnection event
        self.server._publish_device_event(self.device_info.device_id, "disconnected", {
            'duration_seconds': time.time() - self.start_time,
            'bytes_received': self.device_info.bytes_received,
            'chunks_received': self.device_info.chunks_received
        })
        
        self.logger.info(f"Device disconnected", extra={
            'device_id': self.device_info.device_id,
            'duration_seconds': time.time() - self.start_time
        })
    
    def _receive_loop(self) -> None:
        """Main receive loop for audio data."""
        config = get_config()
        chunk_size = config.audio_receiver.tcp_chunk_size
        
        while self.is_running:
            try:
                # Receive audio data
                data = self.connection.recv(chunk_size)
                
                if not data:
                    self.logger.warning("Connection closed by client")
                    break
                
                # Update statistics
                self.device_info.last_activity = datetime.now()
                self.device_info.bytes_received += len(data)
                self.device_info.chunks_received += 1
                self.last_chunk_time = time.time()
                
                # Create audio chunk
                audio_chunk = AudioChunk(
                    device_id=self.device_info.device_id,
                    data=data,
                    chunk_size=len(data)
                )
                
                # Process audio chunk
                self.server._process_audio_chunk(audio_chunk)
                
                # Update device status to streaming
                if self.device_info.status != DeviceStatus.STREAMING:
                    self.device_info.status = DeviceStatus.STREAMING
                    self.server._publish_device_event(self.device_info.device_id, "streaming_started")
                
            except socket.timeout:
                self.logger.warning("Socket timeout - no data received")
                break
            except Exception as e:
                self.device_info.errors += 1
                self.logger.error(f"Error receiving data: {e}")
                break
        
        self.is_running = False
    
    def send_command(self, command: str, data: Dict[str, Any] = None) -> bool:
        """
        Send command to device.
        
        Args:
            command: Command type
            data: Command data
            
        Returns:
            True if successful
        """
        try:
            command_data = {
                'command': command,
                'timestamp': time.time(),
                'data': data or {}
            }
            
            message = json.dumps(command_data).encode() + b'\n'
            self.connection.send(message)
            
            self.logger.debug(f"Command sent to device", extra={
                'device_id': self.device_info.device_id,
                'command': command
            })
            
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to send command: {e}")
            return False


class AudioReceiverServer:
    """
    Enhanced audio receiver server with multi-device support.
    """
    
    def __init__(self, host: str = None, port: int = None):
        """
        Initialize audio receiver server.
        
        Args:
            host: Server host
            port: Server port
        """
        self.config = get_config()
        self.host = host or self.config.audio_receiver.host
        self.port = port or self.config.audio_receiver.port
        
        # Core components
        self.logger = get_logger("audio-receiver")
        self.event_bus = get_event_bus()
        
        # Server state
        self.is_running = False
        self.server_socket = None
        self.accept_thread = None
        
        # Device connections
        self.connections: Dict[str, DeviceConnection] = {}
        self.connections_lock = threading.Lock()
        
        # Statistics
        self.stats = {
            'start_time': time.time(),
            'total_connections': 0,
            'active_connections': 0,
            'total_bytes_received': 0,
            'total_chunks_received': 0,
            'total_errors': 0
        }
        
        # Audio processors
        self.audio_processors: List[Callable] = []
        
        self.logger.info(f"AudioReceiverServer initialized", extra={
            'host': self.host,
            'port': self.port
        })
    
    def start(self) -> None:
        """Start the audio receiver server."""
        if self.is_running:
            self.logger.warning("Server is already running")
            return
        
        try:
            # Create server socket
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind((self.host, self.port))
            self.server_socket.listen(self.config.audio_receiver.max_connections)
            
            self.is_running = True
            
            # Start accept thread
            self.accept_thread = threading.Thread(
                target=self._accept_loop,
                name="AudioReceiver-Accept"
            )
            self.accept_thread.start()
            
            self.logger.info(f"Audio receiver server started", extra={
                'host': self.host,
                'port': self.port,
                'max_connections': self.config.audio_receiver.max_connections
            })
            
            # Publish server start event
            self.event_bus.publish("server.started", {
                'host': self.host,
                'port': self.port
            }, source="audio-receiver")
            
        except Exception as e:
            self.logger.error(f"Failed to start server: {e}")
            self.is_running = False
            raise
    
    def stop(self) -> None:
        """Stop the audio receiver server."""
        if not self.is_running:
            return
        
        self.logger.info("Stopping audio receiver server")
        
        self.is_running = False
        
        # Close all connections
        with self.connections_lock:
            for connection in list(self.connections.values()):
                connection.stop()
            self.connections.clear()
        
        # Close server socket
        if self.server_socket:
            try:
                self.server_socket.close()
            except Exception:
                pass
        
        # Wait for accept thread to finish
        if self.accept_thread and self.accept_thread.is_alive():
            self.accept_thread.join(timeout=5)
        
        self.logger.info("Audio receiver server stopped")
        
        # Publish server stop event
        self.event_bus.publish("server.stopped", {
            'host': self.host,
            'port': self.port,
            'uptime_seconds': time.time() - self.stats['start_time']
        }, source="audio-receiver")
    
    def _accept_loop(self) -> None:
        """Main accept loop for new connections."""
        while self.is_running:
            try:
                # Accept new connection
                connection, address = self.server_socket.accept()
                
                # Check connection limit
                if len(self.connections) >= self.config.audio_receiver.max_connections:
                    self.logger.warning(f"Connection limit reached, rejecting {address}")
                    connection.close()
                    continue
                
                # Create device connection
                device_connection = DeviceConnection(connection, address, self)
                
                # Add to connections
                with self.connections_lock:
                    self.connections[device_connection.device_info.device_id] = device_connection
                    self.stats['total_connections'] += 1
                    self.stats['active_connections'] = len(self.connections)
                
                # Start handling connection
                device_connection.start()
                
                self.logger.info(f"New device connection", extra={
                    'device_id': device_connection.device_info.device_id,
                    'ip_address': address[0],
                    'port': address[1],
                    'active_connections': self.stats['active_connections']
                })
                
            except Exception as e:
                if self.is_running:
                    self.logger.error(f"Error accepting connection: {e}")
                break
    
    def _process_audio_chunk(self, chunk: AudioChunk) -> None:
        """
        Process received audio chunk.
        
        Args:
            chunk: Audio chunk to process
        """
        try:
            # Update statistics
            self.stats['total_bytes_received'] += chunk.chunk_size
            self.stats['total_chunks_received'] += 1
            
            # Process with registered processors
            for processor in self.audio_processors:
                try:
                    processor(chunk)
                except Exception as e:
                    self.logger.error(f"Audio processor failed: {e}")
            
            # Publish audio chunk event
            self.event_bus.publish("audio.chunk_received", {
                'device_id': chunk.device_id,
                'chunk_size': chunk.chunk_size,
                'timestamp': chunk.timestamp
            }, source="audio-receiver")
            
        except Exception as e:
            self.stats['total_errors'] += 1
            self.logger.error(f"Error processing audio chunk: {e}")
    
    def _publish_device_event(self, device_id: str, event_type: str, data: Dict[str, Any] = None) -> None:
        """Publish device-specific event."""
        self.event_bus.publish(f"device.{event_type}", {
            'device_id': device_id,
            **(data or {})
        }, source="audio-receiver")
    
    def add_audio_processor(self, processor: Callable[[AudioChunk], None]) -> None:
        """
        Add audio processor function.
        
        Args:
            processor: Audio processor function
        """
        self.audio_processors.append(processor)
        self.logger.info(f"Audio processor added")
    
    def remove_audio_processor(self, processor: Callable[[AudioChunk], None]) -> bool:
        """
        Remove audio processor function.
        
        Args:
            processor: Audio processor function
            
        Returns:
            True if removed
        """
        if processor in self.audio_processors:
            self.audio_processors.remove(processor)
            self.logger.info(f"Audio processor removed")
            return True
        return False
    
    def get_device_list(self) -> List[DeviceInfo]:
        """Get list of connected devices."""
        with self.connections_lock:
            return [conn.device_info for conn in self.connections.values()]
    
    def get_device(self, device_id: str) -> Optional[DeviceInfo]:
        """Get device information by ID."""
        with self.connections_lock:
            connection = self.connections.get(device_id)
            return connection.device_info if connection else None
    
    def disconnect_device(self, device_id: str) -> bool:
        """
        Disconnect specific device.
        
        Args:
            device_id: Device ID to disconnect
            
        Returns:
            True if device was disconnected
        """
        with self.connections_lock:
            connection = self.connections.get(device_id)
            if connection:
                connection.stop()
                del self.connections[device_id]
                self.stats['active_connections'] = len(self.connections)
                return True
        return False
    
    def send_command_to_device(self, device_id: str, command: str, data: Dict[str, Any] = None) -> bool:
        """
        Send command to specific device.
        
        Args:
            device_id: Device ID
            command: Command to send
            data: Command data
            
        Returns:
            True if command was sent
        """
        with self.connections_lock:
            connection = self.connections.get(device_id)
            if connection:
                return connection.send_command(command, data)
        return False
    
    def broadcast_command(self, command: str, data: Dict[str, Any] = None) -> int:
        """
        Broadcast command to all connected devices.
        
        Args:
            command: Command to broadcast
            data: Command data
            
        Returns:
            Number of devices command was sent to
        """
        count = 0
        with self.connections_lock:
            for connection in self.connections.values():
                if connection.send_command(command, data):
                    count += 1
        return count
    
    def get_stats(self) -> Dict[str, Any]:
        """Get server statistics."""
        uptime = time.time() - self.stats['start_time']
        
        return {
            'host': self.host,
            'port': self.port,
            'is_running': self.is_running,
            'uptime_seconds': uptime,
            'uptime_formatted': f"{uptime//3600:.0f}h {(uptime%3600)//60:.0f}m {uptime%60:.0f}s",
            'total_connections': self.stats['total_connections'],
            'active_connections': self.stats['active_connections'],
            'total_bytes_received': self.stats['total_bytes_received'],
            'total_chunks_received': self.stats['total_chunks_received'],
            'total_errors': self.stats['total_errors'],
            'bytes_per_second': self.stats['total_bytes_received'] / uptime if uptime > 0 else 0,
            'chunks_per_second': self.stats['total_chunks_received'] / uptime if uptime > 0 else 0,
            'audio_processors': len(self.audio_processors)
        }
    
    def health_check(self) -> Dict[str, Any]:
        """Perform health check."""
        health = {
            'server': self.is_running,
            'socket': self.server_socket is not None if self.is_running else False,
            'active_connections': self.stats['active_connections'],
            'max_connections': self.config.audio_receiver.max_connections,
            'memory_usage': 'N/A',  # Would implement memory monitoring
            'cpu_usage': 'N/A',     # Would implement CPU monitoring
            'errors': self.stats['total_errors']
        }
        
        # Check if server is healthy
        health['healthy'] = (
            health['server'] and
            health['socket'] and
            health['active_connections'] >= 0 and
            health['errors'] < 100  # Arbitrary error threshold
        )
        
        return health