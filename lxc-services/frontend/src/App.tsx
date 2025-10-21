import React from 'react'
import { Routes, Route, Navigate } from 'react-router-dom'
import { Box, AppBar, Toolbar, Typography, Drawer, List, ListItem, ListItemIcon, ListItemText } from '@mui/material'
import {
  Dashboard as DashboardIcon,
  MusicNote as AudioIcon,
  Devices as DevicesIcon,
  Assessment as AnalyticsIcon,
  Settings as SettingsIcon,
} from '@mui/icons-material'

import Dashboard from './pages/Dashboard'
import AudioManagement from './pages/AudioManagement'
import DeviceManagement from './pages/DeviceManagement'
import Analytics from './pages/Analytics'
import SystemSettings from './pages/SystemSettings'

const drawerWidth = 240

const menuItems = [
  { text: 'Dashboard', icon: <DashboardIcon />, path: '/' },
  { text: 'Audio Management', icon: <AudioIcon />, path: '/audio' },
  { text: 'Devices', icon: <DevicesIcon />, path: '/devices' },
  { text: 'Analytics', icon: <AnalyticsIcon />, path: '/analytics' },
  { text: 'Settings', icon: <SettingsIcon />, path: '/settings' },
]

function App() {
  return (
    <Box sx={{ display: 'flex' }}>
      <AppBar
        position="fixed"
        sx={{ zIndex: (theme) => theme.zIndex.drawer + 1 }}
      >
        <Toolbar>
          <Typography variant="h6" noWrap component="div">
            Audio Streaming Platform
          </Typography>
        </Toolbar>
      </AppBar>
      
      <Drawer
        variant="permanent"
        sx={{
          width: drawerWidth,
          flexShrink: 0,
          [`& .MuiDrawer-paper`]: { 
            width: drawerWidth, 
            boxSizing: 'border-box',
            backgroundColor: '#1e1e1e',
            borderRight: '1px solid #333'
          },
        }}
      >
        <Toolbar />
        <Box sx={{ overflow: 'auto' }}>
          <List>
            {menuItems.map((item) => (
              <ListItem 
                button 
                key={item.text}
                component="a"
                href={item.path}
                sx={{
                  '&:hover': {
                    backgroundColor: 'rgba(25, 118, 210, 0.08)',
                  },
                }}
              >
                <ListItemIcon sx={{ color: '#fff' }}>
                  {item.icon}
                </ListItemIcon>
                <ListItemText primary={item.text} sx={{ color: '#fff' }} />
              </ListItem>
            ))}
          </List>
        </Box>
      </Drawer>
      
      <Box component="main" sx={{ flexGrow: 1, p: 3 }}>
        <Toolbar />
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/audio" element={<AudioManagement />} />
          <Route path="/devices" element={<DeviceManagement />} />
          <Route path="/analytics" element={<Analytics />} />
          <Route path="/settings" element={<SystemSettings />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </Box>
    </Box>
  )
}

export default App