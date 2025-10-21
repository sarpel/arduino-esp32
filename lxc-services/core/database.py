"""
Database abstraction layer for the audio streaming platform.
Provides ORM integration, connection pooling, and database management.
"""

import asyncio
import threading
import time
from contextlib import contextmanager, asynccontextmanager
from typing import Any, Dict, List, Optional, Union, Type, TypeVar, Generic
from datetime import datetime
from dataclasses import dataclass, field
from enum import Enum
import json

from sqlalchemy import create_engine, Column, Integer, String, DateTime, Float, Boolean, Text, LargeBinary
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, Session, scoped_session
from sqlalchemy.pool import StaticPool
from sqlalchemy.exc import SQLAlchemyError, IntegrityError
from sqlalchemy.dialects.postgresql import UUID, JSONB
from sqlalchemy import event as sqlalchemy_event
import redis

from .logger import get_logger, LogContext
from .config import get_config

# Type variables
T = TypeVar('T')
ModelType = TypeVar('ModelType', bound='BaseModel')


class DatabaseType(Enum):
    """Supported database types."""
    SQLITE = "sqlite"
    POSTGRESQL = "postgresql"
    MYSQL = "mysql"


@dataclass
class DatabaseConfig:
    """Database configuration."""
    database_type: DatabaseType = DatabaseType.SQLITE
    host: str = "localhost"
    port: int = 5432
    name: str = "audio_streamer.db"
    user: str = ""
    password: str = ""
    pool_size: int = 10
    max_overflow: int = 20
    echo: bool = False
    ssl_mode: str = "prefer"


class DatabaseError(Exception):
    """Database-related errors."""
    pass


class ConnectionPoolError(DatabaseError):
    """Connection pool errors."""
    pass


# SQLAlchemy Base
Base = declarative_base()


class BaseModel(Base):
    """Base model for all database models."""
    __abstract__ = True
    
    id = Column(Integer, primary_key=True, autoincrement=True)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False)
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert model to dictionary."""
        result = {}
        for column in self.__table__.columns:
            value = getattr(self, column.name)
            if isinstance(value, datetime):
                result[column.name] = value.isoformat()
            elif isinstance(value, (dict, list)):
                result[column.name] = value
            else:
                result[column.name] = value
        return result
    
    @classmethod
    def from_dict(cls: Type[ModelType], data: Dict[str, Any]) -> ModelType:
        """Create model from dictionary."""
        return cls(**data)
    
    def update_from_dict(self, data: Dict[str, Any]) -> None:
        """Update model from dictionary."""
        for key, value in data.items():
            if hasattr(self, key):
                setattr(self, key, value)


class DatabaseManager:
    """
    Database manager with connection pooling and session management.
    """
    
    def __init__(self, config: Optional[DatabaseConfig] = None):
        """
        Initialize database manager.
        
        Args:
            config: Database configuration
        """
        self.config = config or DatabaseConfig()
        self.logger = get_logger("database")
        
        # Database engine and session
        self._engine = None
        self._session_factory = None
        self._scoped_session = None
        
        # Redis connection (for caching)
        self._redis_client = None
        
        # Connection statistics
        self._stats = {
            'connections_created': 0,
            'connections_closed': 0,
            'queries_executed': 0,
            'transactions_committed': 0,
            'transactions_rolled_back': 0,
            'cache_hits': 0,
            'cache_misses': 0
        }
        
        # Thread-local session storage
        self._local = threading.local()
        
        # Initialize database
        self._initialize()
    
    def _initialize(self) -> None:
        """Initialize database connection and create tables."""
        try:
            # Build database URL
            database_url = self._build_database_url()
            
            # Create engine
            self._engine = create_engine(
                database_url,
                pool_size=self.config.pool_size,
                max_overflow=self.config.max_overflow,
                echo=self.config.echo,
                pool_pre_ping=True,
                pool_recycle=3600  # Recycle connections every hour
            )
            
            # Register connection event listeners
            sqlalchemy_event.listen(self._engine, "connect", self._on_connect)
            sqlalchemy_event.listen(self._engine, "checkout", self._on_checkout)
            sqlalchemy_event.listen(self._engine, "checkin", self._on_checkin)
            
            # Create session factory
            self._session_factory = sessionmaker(bind=self._engine)
            self._scoped_session = scoped_session(self._session_factory)
            
            # Create tables
            Base.metadata.create_all(self._engine)
            
            # Initialize Redis if available
            self._initialize_redis()
            
            self.logger.info(f"Database initialized successfully", extra={
                'database_type': self.config.database_type.value,
                'host': self.config.host,
                'database': self.config.name,
                'pool_size': self.config.pool_size
            })
            
        except Exception as e:
            self.logger.error(f"Failed to initialize database: {e}")
            raise DatabaseError(f"Database initialization failed: {e}")
    
    def _build_database_url(self) -> str:
        """Build database URL from configuration."""
        if self.config.database_type == DatabaseType.SQLITE:
            return f"sqlite:///{self.config.name}"
        
        elif self.config.database_type == DatabaseType.POSTGRESQL:
            auth = ""
            if self.config.user and self.config.password:
                auth = f"{self.config.user}:{self.config.password}@"
            
            ssl_params = ""
            if self.config.ssl_mode:
                ssl_params = f"?sslmode={self.config.ssl_mode}"
            
            return f"postgresql://{auth}{self.config.host}:{self.config.port}/{self.config.name}{ssl_params}"
        
        elif self.config.database_type == DatabaseType.MYSQL:
            auth = ""
            if self.config.user and self.config.password:
                auth = f"{self.config.user}:{self.config.password}@"
            
            return f"mysql+pymysql://{auth}{self.config.host}:{self.config.port}/{self.config.name}"
        
        else:
            raise DatabaseError(f"Unsupported database type: {self.config.database_type}")
    
    def _initialize_redis(self) -> None:
        """Initialize Redis connection for caching."""
        try:
            config = get_config()
            redis_config = config.redis
            
            self._redis_client = redis.Redis(
                host=redis_config.host,
                port=redis_config.port,
                db=redis_config.db,
                password=redis_config.password,
                decode_responses=True,
                socket_connect_timeout=5,
                socket_timeout=5,
                retry_on_timeout=True
            )
            
            # Test connection
            self._redis_client.ping()
            
            self.logger.info("Redis connection established", extra={
                'host': redis_config.host,
                'port': redis_config.port,
                'db': redis_config.db
            })
            
        except Exception as e:
            self.logger.warning(f"Redis connection failed: {e}")
            self._redis_client = None
    
    def _on_connect(self, connection, branch):
        """Handle new database connection."""
        self._stats['connections_created'] += 1
    
    def _on_checkout(self, connection, branch, connection_recorder):
        """Handle connection checkout."""
        pass
    
    def _on_checkin(self, connection, branch):
        """Handle connection checkin."""
        self._stats['connections_closed'] += 1
    
    @contextmanager
    def get_session(self) -> Session:
        """
        Get database session with automatic cleanup.
        
        Yields:
            Database session
        """
        session = self._scoped_session()
        try:
            yield session
            session.commit()
            self._stats['transactions_committed'] += 1
        except Exception as e:
            session.rollback()
            self._stats['transactions_rolled_back'] += 1
            self.logger.error(f"Database transaction failed: {e}")
            raise DatabaseError(f"Transaction failed: {e}")
        finally:
            self._scoped_session.remove()
    
    @asynccontextmanager
    async def get_async_session(self):
        """
        Get async database session.
        
        Yields:
            Database session
        """
        # For async support, we'd use asyncpg or aiomysql
        # For now, provide a sync interface in async context
        with self.get_session() as session:
            yield session
    
    def execute_query(self, query: str, params: Dict[str, Any] = None) -> List[Dict[str, Any]]:
        """
        Execute raw SQL query.
        
        Args:
            query: SQL query
            params: Query parameters
            
        Returns:
            Query results
        """
        with self.get_session() as session:
            try:
                result = session.execute(query, params or {})
                self._stats['queries_executed'] += 1
                
                # Convert to list of dictionaries
                if result.returns_rows:
                    columns = result.keys()
                    return [dict(zip(columns, row)) for row in result.fetchall()]
                else:
                    return []
                    
            except SQLAlchemyError as e:
                self.logger.error(f"Query execution failed: {e}")
                raise DatabaseError(f"Query failed: {e}")
    
    def get_by_id(self, model_class: Type[ModelType], id: int) -> Optional[ModelType]:
        """
        Get record by ID.
        
        Args:
            model_class: Model class
            id: Record ID
            
        Returns:
            Model instance or None
        """
        with self.get_session() as session:
            try:
                return session.query(model_class).filter(model_class.id == id).first()
            except SQLAlchemyError as e:
                self.logger.error(f"Failed to get {model_class.__name__} by ID: {e}")
                raise DatabaseError(f"Get by ID failed: {e}")
    
    def get_all(self, model_class: Type[ModelType], 
                filters: Dict[str, Any] = None,
                order_by: str = None,
                limit: int = None,
                offset: int = None) -> List[ModelType]:
        """
        Get all records with optional filtering.
        
        Args:
            model_class: Model class
            filters: Filter conditions
            order_by: Order by clause
            limit: Limit results
            offset: Offset results
            
        Returns:
            List of model instances
        """
        with self.get_session() as session:
            try:
                query = session.query(model_class)
                
                # Apply filters
                if filters:
                    for key, value in filters.items():
                        if hasattr(model_class, key):
                            query = query.filter(getattr(model_class, key) == value)
                
                # Apply ordering
                if order_by:
                    if hasattr(model_class, order_by):
                        query = query.order_by(getattr(model_class, order_by))
                
                # Apply pagination
                if offset:
                    query = query.offset(offset)
                if limit:
                    query = query.limit(limit)
                
                return query.all()
                
            except SQLAlchemyError as e:
                self.logger.error(f"Failed to get {model_class.__name__} records: {e}")
                raise DatabaseError(f"Get all failed: {e}")
    
    def create(self, model_instance: BaseModel) -> BaseModel:
        """
        Create new record.
        
        Args:
            model_instance: Model instance to create
            
        Returns:
            Created model instance
        """
        with self.get_session() as session:
            try:
                session.add(model_instance)
                session.flush()  # Get the ID without committing
                session.refresh(model_instance)
                
                self.logger.debug(f"Created {model_instance.__class__.__name__}", extra={
                    'id': model_instance.id
                })
                
                return model_instance
                
            except IntegrityError as e:
                self.logger.error(f"Integrity error creating {model_instance.__class__.__name__}: {e}")
                raise DatabaseError(f"Create failed - integrity constraint: {e}")
            except SQLAlchemyError as e:
                self.logger.error(f"Failed to create {model_instance.__class__.__name__}: {e}")
                raise DatabaseError(f"Create failed: {e}")
    
    def update(self, model_instance: BaseModel) -> BaseModel:
        """
        Update existing record.
        
        Args:
            model_instance: Model instance to update
            
        Returns:
            Updated model instance
        """
        with self.get_session() as session:
            try:
                session.merge(model_instance)
                session.flush()
                session.refresh(model_instance)
                
                self.logger.debug(f"Updated {model_instance.__class__.__name__}", extra={
                    'id': model_instance.id
                })
                
                return model_instance
                
            except SQLAlchemyError as e:
                self.logger.error(f"Failed to update {model_instance.__class__.__name__}: {e}")
                raise DatabaseError(f"Update failed: {e}")
    
    def delete(self, model_class: Type[ModelType], id: int) -> bool:
        """
        Delete record by ID.
        
        Args:
            model_class: Model class
            id: Record ID
            
        Returns:
            True if deleted, False if not found
        """
        with self.get_session() as session:
            try:
                instance = session.query(model_class).filter(model_class.id == id).first()
                if instance:
                    session.delete(instance)
                    
                    self.logger.debug(f"Deleted {model_class.__name__}", extra={
                        'id': id
                    })
                    
                    return True
                return False
                
            except SQLAlchemyError as e:
                self.logger.error(f"Failed to delete {model_class.__name__}: {e}")
                raise DatabaseError(f"Delete failed: {e}")
    
    def count(self, model_class: Type[ModelType], 
              filters: Dict[str, Any] = None) -> int:
        """
        Count records with optional filtering.
        
        Args:
            model_class: Model class
            filters: Filter conditions
            
        Returns:
            Number of records
        """
        with self.get_session() as session:
            try:
                query = session.query(model_class)
                
                # Apply filters
                if filters:
                    for key, value in filters.items():
                        if hasattr(model_class, key):
                            query = query.filter(getattr(model_class, key) == value)
                
                return query.count()
                
            except SQLAlchemyError as e:
                self.logger.error(f"Failed to count {model_class.__name__} records: {e}")
                raise DatabaseError(f"Count failed: {e}")
    
    def cache_get(self, key: str) -> Optional[Any]:
        """
        Get value from cache.
        
        Args:
            key: Cache key
            
        Returns:
            Cached value or None
        """
        if not self._redis_client:
            return None
        
        try:
            value = self._redis_client.get(key)
            if value:
                self._stats['cache_hits'] += 1
                return json.loads(value)
            else:
                self._stats['cache_misses'] += 1
                return None
        except Exception as e:
            self.logger.warning(f"Cache get failed: {e}")
            return None
    
    def cache_set(self, key: str, value: Any, ttl: int = 3600) -> bool:
        """
        Set value in cache.
        
        Args:
            key: Cache key
            value: Value to cache
            ttl: Time to live in seconds
            
        Returns:
            True if successful
        """
        if not self._redis_client:
            return False
        
        try:
            serialized_value = json.dumps(value, default=str)
            return self._redis_client.setex(key, ttl, serialized_value)
        except Exception as e:
            self.logger.warning(f"Cache set failed: {e}")
            return False
    
    def cache_delete(self, key: str) -> bool:
        """
        Delete value from cache.
        
        Args:
            key: Cache key
            
        Returns:
            True if successful
        """
        if not self._redis_client:
            return False
        
        try:
            return bool(self._redis_client.delete(key))
        except Exception as e:
            self.logger.warning(f"Cache delete failed: {e}")
            return False
    
    def get_stats(self) -> Dict[str, Any]:
        """Get database statistics."""
        stats = self._stats.copy()
        
        # Add connection pool info
        if self._engine and hasattr(self._engine.pool, 'size'):
            stats['pool_size'] = self._engine.pool.size()
            stats['pool_checked_in'] = self._engine.pool.checkedin()
            stats['pool_checked_out'] = self._engine.pool.checkedout()
        
        # Add Redis info
        if self._redis_client:
            try:
                info = self._redis_client.info()
                stats['redis_connected_clients'] = info.get('connected_clients', 0)
                stats['redis_used_memory'] = info.get('used_memory_human', 'N/A')
            except Exception:
                stats['redis_connected_clients'] = 'N/A'
                stats['redis_used_memory'] = 'N/A'
        
        return stats
    
    def health_check(self) -> Dict[str, Any]:
        """Perform health check on database connections."""
        health = {
            'database': False,
            'redis': False,
            'errors': []
        }
        
        # Check database
        try:
            with self.get_session() as session:
                session.execute("SELECT 1")
            health['database'] = True
        except Exception as e:
            health['errors'].append(f"Database health check failed: {e}")
        
        # Check Redis
        if self._redis_client:
            try:
                self._redis_client.ping()
                health['redis'] = True
            except Exception as e:
                health['errors'].append(f"Redis health check failed: {e}")
        
        return health
    
    def close(self) -> None:
        """Close database connections."""
        if self._engine:
            self._engine.dispose()
        
        if self._redis_client:
            self._redis_client.close()
        
        self.logger.info("Database connections closed")


# Global database manager instance
_db_manager: Optional[DatabaseManager] = None


def get_database_manager() -> DatabaseManager:
    """Get global database manager instance."""
    global _db_manager
    if _db_manager is None:
        config = get_config()
        db_config = DatabaseConfig(
            database_type=DatabaseType.POSTGRESQL,
            host=config.database.host,
            port=config.database.port,
            name=config.database.name,
            user=config.database.user,
            password=config.database.password,
            pool_size=config.database.pool_size,
            max_overflow=config.database.max_overflow,
            echo=config.database.echo
        )
        _db_manager = DatabaseManager(db_config)
    return _db_manager


def init_database(config: Optional[DatabaseConfig] = None) -> DatabaseManager:
    """
    Initialize global database manager.
    
    Args:
        config: Database configuration
        
    Returns:
        Database manager instance
    """
    global _db_manager
    _db_manager = DatabaseManager(config)
    return _db_manager


# Decorator for database operations
def with_database(func):
    """Decorator to provide database session to function."""
    def wrapper(*args, **kwargs):
        db = get_database_manager()
        with db.get_session() as session:
            return func(session, *args, **kwargs)
    return wrapper


# Database transaction decorator
def transactional(func):
    """Decorator to run function within database transaction."""
    def wrapper(*args, **kwargs):
        db = get_database_manager()
        with db.get_session() as session:
            try:
                result = func(session, *args, **kwargs)
                return result
            except Exception:
                # Session will be automatically rolled back
                raise
    return wrapper