"""
Authentication API routes.
"""

from fastapi import APIRouter, Depends, HTTPException, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from typing import Dict, Any
from pydantic import BaseModel
from datetime import datetime, timedelta

import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from core.logger import get_logger

logger = get_logger(__name__)
router = APIRouter()
security = HTTPBearer()


# Pydantic models
class LoginRequest(BaseModel):
    """Login request model."""
    username: str
    password: str


class LoginResponse(BaseModel):
    """Login response model."""
    access_token: str
    token_type: str
    expires_in: int
    user: Dict[str, Any]


class User(BaseModel):
    """User model."""
    id: int
    username: str
    email: str
    role: str
    created_at: datetime


# Mock user data
MOCK_USERS = {
    "admin": {
        "id": 1,
        "username": "admin",
        "password": "admin123",  # In production, use hashed passwords
        "email": "admin@example.com",
        "role": "admin",
        "created_at": datetime.now()
    },
    "user": {
        "id": 2,
        "username": "user",
        "password": "user123",
        "email": "user@example.com",
        "role": "user",
        "created_at": datetime.now()
    }
}


# Routes
@router.post("/login", response_model=LoginResponse)
async def login(request: LoginRequest):
    """
    Authenticate user and return access token.
    
    Args:
        request: Login credentials
        
    Returns:
        Access token and user info
    """
    try:
        # Validate credentials
        user = MOCK_USERS.get(request.username)
        if not user or user["password"] != request.password:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid credentials"
            )
        
        # Generate mock token (in production, use JWT)
        token = f"mock_token_{user['id']}_{datetime.now().timestamp()}"
        
        return LoginResponse(
            access_token=token,
            token_type="Bearer",
            expires_in=3600,
            user={
                "id": user["id"],
                "username": user["username"],
                "email": user["email"],
                "role": user["role"]
            }
        )
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Login failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/me", response_model=User)
async def get_current_user(credentials: HTTPAuthorizationCredentials = Depends(security)):
    """
    Get current user information.
    
    Args:
        credentials: Authorization credentials
        
    Returns:
        User information
    """
    try:
        # Mock token validation (in production, validate JWT)
        if not credentials.credentials.startswith("mock_token_"):
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Invalid token"
            )
        
        # Extract user ID from token
        token_parts = credentials.credentials.split("_")
        user_id = int(token_parts[2])
        
        # Find user
        user = None
        for mock_user in MOCK_USERS.values():
            if mock_user["id"] == user_id:
                user = mock_user
                break
        
        if not user:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="User not found"
            )
        
        return User(
            id=user["id"],
            username=user["username"],
            email=user["email"],
            role=user["role"],
            created_at=user["created_at"]
        )
        
    except HTTPException:
        raise
    except Exception as e:
        logger.error(f"Failed to get current user: {e}")
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/logout")
async def logout():
    """
    Logout user.
    
    Returns:
        Logout status
    """
    try:
        # In production, invalidate token
        return {"message": "Successfully logged out"}
        
    except Exception as e:
        logger.error(f"Logout failed: {e}")
        raise HTTPException(status_code=500, detail=str(e))