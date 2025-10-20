# Development Commands & Workflow

## Build & Upload
```bash
# Build project
pio run

# Upload to ESP32 (requires board connected)
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor --baud 115200
```

## Configuration
Edit `src/config.h` before building:
- WiFi credentials: `WIFI_SSID`, `WIFI_PASSWORD`
- Server settings: `SERVER_HOST`, `SERVER_PORT`
- I2S pins for board type (auto-selected via board detection)

## Git Workflow
```bash
git status              # Check changes
git diff                # View changes
git log --oneline       # View commit history
git checkout -b feature/name  # Create feature branch
git add .
git commit -m "message"
git push
```

## File Operations (Windows)
```powershell
dir                     # List directory
type filename           # View file contents
del filename            # Delete file
```

## Testing/Validation
```bash
# After modifications, rebuild and test:
pio run && pio run --target upload
```
