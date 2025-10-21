import axios from 'axios'

// Create axios instance with default configuration
const api = axios.create({
  baseURL: '/api/v1',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
})

// Request interceptor for adding auth token
api.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('authToken')
    if (token) {
      config.headers.Authorization = `Bearer ${token}`
    }
    return config
  },
  (error) => {
    return Promise.reject(error)
  }
)

// Response interceptor for error handling
api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.status === 401) {
      // Handle unauthorized access
      localStorage.removeItem('authToken')
      window.location.href = '/login'
    }
    return Promise.reject(error)
  }
)

// System and Health APIs
export const getHealthStatus = async () => {
  const response = await api.get('/health')
  return response.data
}

export const getSystemMetrics = async () => {
  const response = await api.get('/system/metrics')
  return response.data
}

export const getSystemInfo = async () => {
  const response = await api.get('/system/info')
  return response.data
}

// Monitoring APIs
export const getRecentAlerts = async () => {
  const response = await api.get('/monitoring/alerts')
  return response.data
}

export const getSystemMetricsHistory = async (timeRange: string = '1h') => {
  const response = await api.get(`/monitoring/metrics?range=${timeRange}`)
  return response.data
}

export const getActiveConnections = async () => {
  const response = await api.get('/monitoring/connections')
  return response.data
}

// Audio Management APIs
export const getAudioFiles = async (params?: any) => {
  const response = await api.get('/audio/files', { params })
  return response.data
}

export const uploadAudioFile = async (formData: FormData) => {
  const response = await api.post('/audio/upload', formData, {
    headers: {
      'Content-Type': 'multipart/form-data',
    },
  })
  return response.data
}

export const processAudioFile = async (fileId: string, processingOptions: any) => {
  const response = await api.post(`/audio/${fileId}/process`, processingOptions)
  return response.data
}

export const getAudioStatistics = async () => {
  const response = await api.get('/audio/statistics')
  return response.data
}

export const streamAudioFile = async (fileId: string) => {
  const response = await api.get(`/audio/${fileId}/stream`, {
    responseType: 'blob',
  })
  return response.data
}

// Device Management APIs
export const getDevices = async () => {
  const response = await api.get('/devices')
  return response.data
}

export const getDeviceDetails = async (deviceId: string) => {
  const response = await api.get(`/devices/${deviceId}`)
  return response.data
}

export const updateDeviceConfiguration = async (deviceId: string, config: any) => {
  const response = await api.put(`/devices/${deviceId}/config`, config)
  return response.data
}

export const getDeviceStatistics = async (deviceId: string) => {
  const response = await api.get(`/devices/${deviceId}/statistics`)
  return response.data
}

// Analytics APIs
export const getAnalyticsData = async (query: any) => {
  const response = await api.post('/analytics/query', query)
  return response.data
}

export const getDashboardSummary = async () => {
  const response = await api.get('/analytics/dashboard')
  return response.data
}

export const getPerformanceMetrics = async (timeRange: string = '24h') => {
  const response = await api.get(`/analytics/performance?range=${timeRange}`)
  return response.data
}

export const getUsageStatistics = async () => {
  const response = await api.get('/analytics/usage')
  return response.data
}

// Authentication APIs
export const login = async (credentials: { username: string; password: string }) => {
  const response = await api.post('/auth/login', credentials)
  return response.data
}

export const logout = async () => {
  const response = await api.post('/auth/logout')
  return response.data
}

export const refreshToken = async () => {
  const response = await api.post('/auth/refresh')
  return response.data
}

export const getCurrentUser = async () => {
  const response = await api.get('/auth/me')
  return response.data
}

// WebSocket connection for real-time updates
export class WebSocketManager {
  private ws: WebSocket | null = null
  private reconnectAttempts = 0
  private maxReconnectAttempts = 5
  private reconnectDelay = 1000

  connect(url: string) {
    try {
      this.ws = new WebSocket(url)
      
      this.ws.onopen = () => {
        console.log('WebSocket connected')
        this.reconnectAttempts = 0
      }

      this.ws.onclose = () => {
        console.log('WebSocket disconnected')
        this.attemptReconnect(url)
      }

      this.ws.onerror = (error) => {
        console.error('WebSocket error:', error)
      }

    } catch (error) {
      console.error('Failed to connect WebSocket:', error)
      this.attemptReconnect(url)
    }
  }

  private attemptReconnect(url: string) {
    if (this.reconnectAttempts < this.maxReconnectAttempts) {
      setTimeout(() => {
        this.reconnectAttempts++
        console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})`)
        this.connect(url)
      }, this.reconnectDelay * this.reconnectAttempts)
    }
  }

  send(data: any) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(data))
    }
  }

  onMessage(callback: (data: any) => void) {
    if (this.ws) {
      this.ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data)
          callback(data)
        } catch (error) {
          console.error('Failed to parse WebSocket message:', error)
        }
      }
    }
  }

  disconnect() {
    if (this.ws) {
      this.ws.close()
      this.ws = null
    }
  }
}

export const wsManager = new WebSocketManager()

export default api