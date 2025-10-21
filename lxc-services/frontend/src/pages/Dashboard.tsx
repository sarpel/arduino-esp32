import React, { useState, useEffect } from 'react'
import {
  Grid,
  Card,
  CardContent,
  Typography,
  Box,
  LinearProgress,
  Chip,
  Alert,
} from '@mui/material'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  AreaChart,
  Area,
} from 'recharts'
import { useQuery } from 'react-query'

// API service functions
import { getSystemMetrics, getHealthStatus, getRecentAlerts } from '../services/api'

interface SystemMetrics {
  timestamp: number
  cpu_percent: number
  memory_percent: number
  disk_usage_percent: number
  network_io: { bytes_sent: number; bytes_recv: number }
}

interface HealthStatus {
  status: string
  components: {
    database: string
    monitoring: string
    api: string
  }
}

interface Alert {
  id: string
  level: 'info' | 'warning' | 'error' | 'critical'
  message: string
  timestamp: number
}

const Dashboard: React.FC = () => {
  const [metricsHistory, setMetricsHistory] = useState<SystemMetrics[]>([])

  // Fetch real-time data
  const { data: healthStatus, isLoading: healthLoading } = useQuery(
    'healthStatus',
    getHealthStatus,
    { refetchInterval: 30000 }
  )

  const { data: systemMetrics, isLoading: metricsLoading } = useQuery(
    'systemMetrics',
    getSystemMetrics,
    { refetchInterval: 5000 }
  )

  const { data: alerts, isLoading: alertsLoading } = useQuery(
    'recentAlerts',
    getRecentAlerts,
    { refetchInterval: 10000 }
  )

  // Update metrics history
  useEffect(() => {
    if (systemMetrics) {
      setMetricsHistory(prev => {
        const newHistory = [...prev, systemMetrics]
        return newHistory.slice(-20) // Keep last 20 data points
      })
    }
  }, [systemMetrics])

  const getAlertColor = (level: string) => {
    switch (level) {
      case 'critical': return 'error'
      case 'error': return 'error'
      case 'warning': return 'warning'
      case 'info': return 'info'
      default: return 'default'
    }
  }

  const formatBytes = (bytes: number) => {
    if (bytes === 0) return '0 B'
    const k = 1024
    const sizes = ['B', 'KB', 'MB', 'GB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }

  if (healthLoading || metricsLoading) {
    return (
      <Box sx={{ width: '100%', mt: 2 }}>
        <LinearProgress />
      </Box>
    )
  }

  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Dashboard
      </Typography>

      {/* System Status Cards */}
      <Grid container spacing={3} sx={{ mb: 3 }}>
        <Grid item xs={12} sm={6} md={3}>
          <Card>
            <CardContent>
              <Typography color="textSecondary" gutterBottom>
                System Status
              </Typography>
              <Typography variant="h5">
                <Chip
                  label={healthStatus?.status || 'Unknown'}
                  color={healthStatus?.status === 'healthy' ? 'success' : 'error'}
                  sx={{ fontWeight: 'bold' }}
                />
              </Typography>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} sm={6} md={3}>
          <Card>
            <CardContent>
              <Typography color="textSecondary" gutterBottom>
                CPU Usage
              </Typography>
              <Typography variant="h5">
                {systemMetrics?.cpu_percent.toFixed(1) || 0}%
              </Typography>
              <LinearProgress
                variant="determinate"
                value={systemMetrics?.cpu_percent || 0}
                sx={{ mt: 1 }}
              />
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} sm={6} md={3}>
          <Card>
            <CardContent>
              <Typography color="textSecondary" gutterBottom>
                Memory Usage
              </Typography>
              <Typography variant="h5">
                {systemMetrics?.memory_percent.toFixed(1) || 0}%
              </Typography>
              <LinearProgress
                variant="determinate"
                value={systemMetrics?.memory_percent || 0}
                sx={{ mt: 1 }}
              />
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} sm={6} md={3}>
          <Card>
            <CardContent>
              <Typography color="textSecondary" gutterBottom>
                Disk Usage
              </Typography>
              <Typography variant="h5">
                {systemMetrics?.disk_usage_percent.toFixed(1) || 0}%
              </Typography>
              <LinearProgress
                variant="determinate"
                value={systemMetrics?.disk_usage_percent || 0}
                sx={{ mt: 1 }}
              />
            </CardContent>
          </Card>
        </Grid>
      </Grid>

      {/* Charts and Alerts */}
      <Grid container spacing={3}>
        <Grid item xs={12} md={8}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                System Performance
              </Typography>
              <ResponsiveContainer width="100%" height={300}>
                <LineChart data={metricsHistory}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis
                    dataKey="timestamp"
                    tickFormatter={(value) => new Date(value * 1000).toLocaleTimeString()}
                  />
                  <YAxis />
                  <Tooltip
                    labelFormatter={(value) => new Date(value * 1000).toLocaleString()}
                  />
                  <Line
                    type="monotone"
                    dataKey="cpu_percent"
                    stroke="#8884d8"
                    name="CPU %"
                    strokeWidth={2}
                  />
                  <Line
                    type="monotone"
                    dataKey="memory_percent"
                    stroke="#82ca9d"
                    name="Memory %"
                    strokeWidth={2}
                  />
                </LineChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12} md={4}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Recent Alerts
              </Typography>
              <Box sx={{ maxHeight: 300, overflowY: 'auto' }}>
                {alerts?.length === 0 ? (
                  <Typography color="textSecondary">
                    No recent alerts
                  </Typography>
                ) : (
                  alerts?.slice(0, 10).map((alert: Alert) => (
                    <Alert
                      key={alert.id}
                      severity={getAlertColor(alert.level) as any}
                      sx={{ mb: 1 }}
                    >
                      <Typography variant="body2">
                        {alert.message}
                      </Typography>
                      <Typography variant="caption" color="textSecondary">
                        {new Date(alert.timestamp * 1000).toLocaleString()}
                      </Typography>
                    </Alert>
                  ))
                )}
              </Box>
            </CardContent>
          </Card>
        </Grid>

        <Grid item xs={12}>
          <Card>
            <CardContent>
              <Typography variant="h6" gutterBottom>
                Network I/O
              </Typography>
              <ResponsiveContainer width="100%" height={200}>
                <AreaChart data={metricsHistory}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis
                    dataKey="timestamp"
                    tickFormatter={(value) => new Date(value * 1000).toLocaleTimeString()}
                  />
                  <YAxis tickFormatter={(value) => formatBytes(value)} />
                  <Tooltip
                    labelFormatter={(value) => new Date(value * 1000).toLocaleString()}
                    formatter={(value: any) => [formatBytes(value), 'Bytes']}
                  />
                  <Area
                    type="monotone"
                    dataKey="network_io.bytes_sent"
                    stackId="1"
                    stroke="#8884d8"
                    fill="#8884d8"
                    name="Bytes Sent"
                  />
                  <Area
                    type="monotone"
                    dataKey="network_io.bytes_recv"
                    stackId="1"
                    stroke="#82ca9d"
                    fill="#82ca9d"
                    name="Bytes Received"
                  />
                </AreaChart>
              </ResponsiveContainer>
            </CardContent>
          </Card>
        </Grid>
      </Grid>
    </Box>
  )
}

export default Dashboard