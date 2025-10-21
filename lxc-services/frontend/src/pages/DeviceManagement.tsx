import React from 'react'
import { Typography, Box, Card, CardContent } from '@mui/material'

const DeviceManagement: React.FC = () => {
  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        Device Management
      </Typography>
      <Card>
        <CardContent>
          <Typography>
            ESP32 device configuration, monitoring, and management will be implemented here.
          </Typography>
        </CardContent>
      </Card>
    </Box>
  )
}

export default DeviceManagement