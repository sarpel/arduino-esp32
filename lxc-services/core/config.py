"""
Centralized configuration management for the audio streaming platform.
Supports environment variables, config files, and runtime configuration.
"""

import os
import json
import yaml
from pathlib import Path
from typing import Any, Dict, Optional, Union
from dataclasses import dataclass, field


@dataclass
class DatabaseConfig:
    """Database configuration settings."""
    host: str = "localhost"
    port: int = 5432
    name: str = "audio_streamer"
    user: str = "audio_user"
    password: str = "audio_password"
    pool_size: int = 10
    max_overflow: int = 20
    echo: bool = False


@dataclass
class RedisConfig:
    """Redis configuration settings."""
    host: str = "localhost"
    port: int = 6379
    db: int = 0
    password: Optional[str] = None
    max_connections: int = 10


@dataclass
class AudioReceiverConfig:
    """Audio receiver configuration settings."""
    host: str = "0.0.0.0"
    port: int = 9000
    data_dir: str = "/data/audio"
    sample_rate: int = 16000
    bits_per_sample: int = 16
    channels: int = 1
    segment_duration: int = 600
    tcp_chunk_size: int = 19200
    max_connections: int = 100
    timeout: int = 30
    enable_compression: bool = True
    compression_format: str = "flac"
    compression_delay: int = 10
    delete_original_wav: bool = True


@dataclass
class WebUIConfig:
    """Web UI configuration settings."""
    host: str = "0.0.0.0"
    port: int = 8080
    debug: bool = False
    secret_key: str = "your-secret-key-change-in-production"
    upload_folder: str = "/tmp/uploads"
    max_content_length: int = 100 * 1024 * 1024  # 100MB
    enable_cors: bool = True
    cors_origins: list = field(default_factory=lambda: ["*"])


@dataclass
class SecurityConfig:
    """Security configuration settings."""
    secret_key: str = "your-secret-key-change-in-production"
    jwt_expiration_hours: int = 24
    password_min_length: int = 8
    enable_2fa: bool = False
    session_timeout: int = 3600
    max_login_attempts: int = 5
    lockout_duration: int = 900
    encryption_key: Optional[str] = None


@dataclass
class MonitoringConfig:
    """Monitoring configuration settings."""
    enable_metrics: bool = True
    metrics_port: int = 9090
    health_check_interval: int = 30
    alert_webhook_url: Optional[str] = None
    log_level: str = "INFO"
    enable_audit_log: bool = True
    retention_days: int = 30


@dataclass
class SystemConfig:
    """Main system configuration."""
    environment: str = "development"
    debug: bool = False
    log_level: str = "INFO"
    data_dir: str = "/data/audio"
    temp_dir: str = "/tmp/audio-streamer"
    
    # Sub-configurations
    database: DatabaseConfig = field(default_factory=DatabaseConfig)
    redis: RedisConfig = field(default_factory=RedisConfig)
    audio_receiver: AudioReceiverConfig = field(default_factory=AudioReceiverConfig)
    web_ui: WebUIConfig = field(default_factory=WebUIConfig)
    security: SecurityConfig = field(default_factory=SecurityConfig)
    monitoring: MonitoringConfig = field(default_factory=MonitoringConfig)


class Config:
    """
    Centralized configuration manager.
    Supports environment variables, config files, and runtime overrides.
    """
    
    def __init__(self, config_file: Optional[Union[str, Path]] = None):
        """
        Initialize configuration manager.
        
        Args:
            config_file: Path to configuration file (JSON or YAML)
        """
        self._config = SystemConfig()
        self._config_file = config_file
        
        # Load configuration from file if provided
        if config_file:
            self._load_from_file(config_file)
        
        # Override with environment variables
        self._load_from_env()
        
        # Validate configuration
        self._validate()
    
    def _load_from_file(self, config_file: Union[str, Path]) -> None:
        """Load configuration from file."""
        config_path = Path(config_file)
        
        if not config_path.exists():
            raise FileNotFoundError(f"Configuration file not found: {config_file}")
        
        try:
            with open(config_path, 'r') as f:
                if config_path.suffix.lower() in ['.yaml', '.yml']:
                    data = yaml.safe_load(f)
                elif config_path.suffix.lower() == '.json':
                    data = json.load(f)
                else:
                    raise ValueError(f"Unsupported config file format: {config_path.suffix}")
            
            self._update_config(data)
            
        except Exception as e:
            raise ValueError(f"Error loading configuration file: {e}")
    
    def _load_from_env(self) -> None:
        """Load configuration from environment variables."""
        # System settings
        self._config.environment = os.getenv('ENVIRONMENT', self._config.environment)
        self._config.debug = os.getenv('DEBUG', 'false').lower() == 'true'
        self._config.log_level = os.getenv('LOG_LEVEL', self._config.log_level)
        self._config.data_dir = os.getenv('DATA_DIR', self._config.data_dir)
        self._config.temp_dir = os.getenv('TEMP_DIR', self._config.temp_dir)
        
        # Database settings
        self._config.database.host = os.getenv('DB_HOST', self._config.database.host)
        self._config.database.port = int(os.getenv('DB_PORT', str(self._config.database.port)))
        self._config.database.name = os.getenv('DB_NAME', self._config.database.name)
        self._config.database.user = os.getenv('DB_USER', self._config.database.user)
        self._config.database.password = os.getenv('DB_PASSWORD', self._config.database.password)
        
        # Redis settings
        self._config.redis.host = os.getenv('REDIS_HOST', self._config.redis.host)
        self._config.redis.port = int(os.getenv('REDIS_PORT', str(self._config.redis.port)))
        self._config.redis.password = os.getenv('REDIS_PASSWORD', self._config.redis.password)
        
        # Audio receiver settings
        self._config.audio_receiver.host = os.getenv('AUDIO_RECEIVER_HOST', self._config.audio_receiver.host)
        self._config.audio_receiver.port = int(os.getenv('AUDIO_RECEIVER_PORT', str(self._config.audio_receiver.port)))
        self._config.audio_receiver.data_dir = os.getenv('AUDIO_DATA_DIR', self._config.audio_receiver.data_dir)
        
        # Web UI settings
        self._config.web_ui.host = os.getenv('WEB_UI_HOST', self._config.web_ui.host)
        self._config.web_ui.port = int(os.getenv('WEB_UI_PORT', str(self._config.web_ui.port)))
        self._config.web_ui.secret_key = os.getenv('WEB_UI_SECRET_KEY', self._config.web_ui.secret_key)
        
        # Security settings
        self._config.security.secret_key = os.getenv('SECRET_KEY', self._config.security.secret_key)
        self._config.security.encryption_key = os.getenv('ENCRYPTION_KEY', self._config.security.encryption_key)
        
        # Monitoring settings
        self._config.monitoring.log_level = os.getenv('MONITORING_LOG_LEVEL', self._config.monitoring.log_level)
        self._config.monitoring.alert_webhook_url = os.getenv('ALERT_WEBHOOK_URL', self._config.monitoring.alert_webhook_url)
    
    def _update_config(self, data: Dict[str, Any]) -> None:
        """Update configuration with data from file."""
        def update_dataclass(obj, data):
            if isinstance(data, dict):
                for key, value in data.items():
                    if hasattr(obj, key):
                        attr = getattr(obj, key)
                        if hasattr(attr, '__dataclass_fields__'):
                            update_dataclass(attr, value)
                        else:
                            setattr(obj, key, value)
        
        update_dataclass(self._config, data)
    
    def _validate(self) -> None:
        """Validate configuration values."""
        # Validate required directories
        for dir_path in [self._config.data_dir, self._config.temp_dir]:
            Path(dir_path).mkdir(parents=True, exist_ok=True)
        
        # Validate ports
        if not (1 <= self._config.audio_receiver.port <= 65535):
            raise ValueError(f"Invalid audio receiver port: {self._config.audio_receiver.port}")
        
        if not (1 <= self._config.web_ui.port <= 65535):
            raise ValueError(f"Invalid web UI port: {self._config.web_ui.port}")
        
        # Validate audio settings
        if self._config.audio_receiver.sample_rate not in [8000, 16000, 22050, 44100, 48000]:
            raise ValueError(f"Unsupported sample rate: {self._config.audio_receiver.sample_rate}")
        
        if self._config.audio_receiver.bits_per_sample not in [16, 24, 32]:
            raise ValueError(f"Unsupported bits per sample: {self._config.audio_receiver.bits_per_sample}")
        
        # Validate compression format
        if self._config.audio_receiver.compression_format not in ['flac', 'opus', 'mp3']:
            raise ValueError(f"Unsupported compression format: {self._config.audio_receiver.compression_format}")
        
        # Validate log level
        valid_log_levels = ['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']
        if self._config.log_level not in valid_log_levels:
            raise ValueError(f"Invalid log level: {self._config.log_level}")
    
    def get(self, key: str, default: Any = None) -> Any:
        """
        Get configuration value by key.
        
        Args:
            key: Configuration key (e.g., 'database.host')
            default: Default value if key not found
            
        Returns:
            Configuration value
        """
        keys = key.split('.')
        value = self._config
        
        try:
            for k in keys:
                value = getattr(value, k)
            return value
        except AttributeError:
            return default
    
    def set(self, key: str, value: Any) -> None:
        """
        Set configuration value by key.
        
        Args:
            key: Configuration key (e.g., 'database.host')
            value: Value to set
        """
        keys = key.split('.')
        obj = self._config
        
        for k in keys[:-1]:
            obj = getattr(obj, k)
        
        setattr(obj, keys[-1], value)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert configuration to dictionary."""
        def dataclass_to_dict(obj):
            if hasattr(obj, '__dataclass_fields__'):
                return {k: dataclass_to_dict(getattr(obj, k)) for k in obj.__dataclass_fields__}
            elif isinstance(obj, dict):
                return {k: dataclass_to_dict(v) for k, v in obj.items()}
            elif isinstance(obj, list):
                return [dataclass_to_dict(item) for item in obj]
            else:
                return obj
        
        result = dataclass_to_dict(self._config)
        return result as Dict[str, Any]
    
    def save(self, file_path: Union[str, Path]) -> None:
        """
        Save configuration to file.
        
        Args:
            file_path: Path to save configuration
        """
        config_path = Path(file_path)
        data = self.to_dict()
        
        with open(config_path, 'w') as f:
            if config_path.suffix.lower() in ['.yaml', '.yml']:
                yaml.dump(data, f, default_flow_style=False, indent=2)
            elif config_path.suffix.lower() == '.json':
                json.dump(data, f, indent=2)
            else:
                raise ValueError(f"Unsupported config file format: {config_path.suffix}")
    
    @property
    def system(self) -> SystemConfig:
        """Get the complete system configuration."""
        return self._config
    
    @property
    def database(self) -> DatabaseConfig:
        """Get database configuration."""
        return self._config.database
    
    @property
    def redis(self) -> RedisConfig:
        """Get Redis configuration."""
        return self._config.redis
    
    @property
    def audio_receiver(self) -> AudioReceiverConfig:
        """Get audio receiver configuration."""
        return self._config.audio_receiver
    
    @property
    def web_ui(self) -> WebUIConfig:
        """Get web UI configuration."""
        return self._config.web_ui
    
    @property
    def security(self) -> SecurityConfig:
        """Get security configuration."""
        return self._config.security
    
    @property
    def monitoring(self) -> MonitoringConfig:
        """Get monitoring configuration."""
        return self._config.monitoring


# Global configuration instance
_config = None


def get_config() -> Config:
    """Get global configuration instance."""
    global _config
    if _config is None:
        _config = Config()
    return _config


def init_config(config_file: Optional[Union[str, Path]] = None) -> Config:
    """
    Initialize global configuration.
    
    Args:
        config_file: Path to configuration file
        
    Returns:
        Configuration instance
    """
    global _config
    _config = Config(config_file)
    return _config