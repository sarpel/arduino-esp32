"""
FastAPI main application for the audio streaming platform.
"""

from fastapi import FastAPI, Request, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.middleware.gzip import GZipMiddleware
from fastapi.responses import JSONResponse
from contextlib import asynccontextmanager
import time
import logging

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.logger import get_logger
from core.config import get_config
from core.events import get_event_bus
from core.database import get_database
from api.routes import audio, devices, monitoring, analytics, auth, system

logger = get_logger(__name__)
config = get_config()
event_bus = get_event_bus()
database = get_database()


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan manager."""
    # Startup
    logger.info("Starting Audio Streaming Platform API...")
    
    try:
        # Initialize database connection
        await database.connect()
        logger.info("Database connected successfully")
        
        # Start monitoring
        from audio_receiver.monitoring import start_monitoring
        start_monitoring()
        logger.info("Monitoring system started")
        
        # Emit startup event
        event_bus.emit({
            'event_type': 'api.started',
            'source': 'FastAPI',
            'data': {'timestamp': time.time()}
        })
        
        yield
        
    except Exception as e:
        logger.error(f"Failed to start application: {e}")
        raise
    
    finally:
        # Shutdown
        logger.info("Shutting down Audio Streaming Platform API...")
        
        try:
            # Stop monitoring
            from audio_receiver.monitoring import stop_monitoring
            stop_monitoring()
            
            # Close database connection
            await database.disconnect()
            
            # Emit shutdown event
            event_bus.emit({
                'event_type': 'api.stopped',
                'source': 'FastAPI',
                'data': {'timestamp': time.time()}
            })
            
            logger.info("Application shutdown complete")
            
        except Exception as e:
            logger.error(f"Error during shutdown: {e}")


# Create FastAPI application
app = FastAPI(
    title="Audio Streaming Platform API",
    description="Enterprise-grade audio streaming and monitoring platform",
    version="2.0.0",
    docs_url="/docs",
    redoc_url="/redoc",
    openapi_url="/openapi.json",
    lifespan=lifespan
)

# Add middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Configure appropriately for production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.add_middleware(GZipMiddleware, minimum_size=1000)


# Request timing middleware
@app.middleware("http")
async def add_process_time_header(request: Request, call_next):
    """Add request processing time header."""
    start_time = time.time()
    response = await call_next(request)
    process_time = time.time() - start_time
    response.headers["X-Process-Time"] = str(process_time)
    
    # Log slow requests
    if process_time > 1.0:
        logger.warning(f"Slow request: {request.method} {request.url.path} took {process_time:.2f}s")
    
    return response


# Global exception handler
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """Global exception handler for unhandled exceptions."""
    logger.error(f"Unhandled exception: {exc}", exc_info=True)
    
    return JSONResponse(
        status_code=500,
        content={
            "error": "Internal server error",
            "message": "An unexpected error occurred",
            "request_id": getattr(request.state, 'request_id', None)
        }
    )


# Health check endpoint
@app.get("/health")
async def health_check():
    """Health check endpoint."""
    try:
        # Check database connection
        db_status = await database.health_check()
        
        # Check monitoring system
        from audio_receiver.monitoring import get_monitor
        monitor = get_monitor()
        monitoring_status = monitor.monitoring_active
        
        overall_status = "healthy" if db_status and monitoring_status else "unhealthy"
        
        return {
            "status": overall_status,
            "timestamp": time.time(),
            "version": "2.0.0",
            "components": {
                "database": "healthy" if db_status else "unhealthy",
                "monitoring": "healthy" if monitoring_status else "unhealthy",
                "api": "healthy"
            }
        }
        
    except Exception as e:
        logger.error(f"Health check failed: {e}")
        return JSONResponse(
            status_code=503,
            content={
                "status": "unhealthy",
                "error": str(e),
                "timestamp": time.time()
            }
        )


# Include routers
app.include_router(
    audio.router,
    prefix="/api/v1/audio",
    tags=["Audio Management"]
)

app.include_router(
    devices.router,
    prefix="/api/v1/devices",
    tags=["Device Management"]
)

app.include_router(
    monitoring.router,
    prefix="/api/v1/monitoring",
    tags=["Monitoring"]
)

app.include_router(
    analytics.router,
    prefix="/api/v1/analytics",
    tags=["Analytics"]
)

app.include_router(
    auth.router,
    prefix="/api/v1/auth",
    tags=["Authentication"]
)

app.include_router(
    system.router,
    prefix="/api/v1/system",
    tags=["System Management"]
)


# Root endpoint
@app.get("/")
async def root():
    """Root endpoint with API information."""
    return {
        "name": "Audio Streaming Platform API",
        "version": "2.0.0",
        "description": "Enterprise-grade audio streaming and monitoring platform",
        "docs": "/docs",
        "health": "/health",
        "status": "operational"
    }


if __name__ == "__main__":
    import uvicorn
    
    uvicorn.run(
        "api.main:app",
        host="0.0.0.0",
        port=8000,
        reload=True,
        log_level="info"
    )