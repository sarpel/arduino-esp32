# ESP32S3 Build Instructions - UART Mode

## Critical: Use the Correct Environment

You must use the `seeed_xiao_esp32s3_uart` environment for UART mode (same as ESP32dev):

### Step 1: Clean Build
```bash
pio run --target clean --environment seeed_xiao_esp32s3_uart
```

### Step 2: Build for UART Environment
```bash
pio run --environment seeed_xiao_esp32s3_uart
```

### Step 3: Upload
```bash
# Replace COM4 with your actual COM port
pio device upload --environment seeed_xiao_esp32s3_uart --port COM4
```

### Step 4: Open Serial Monitor
```bash
# Replace COM4 with your actual COM port
pio device monitor --port COM4 --baud 115200
```

## Key Difference: UART Environment

The `seeed_xiao_esp32s3_uart` environment is **guaranteed** to use UART mode:
```ini
[env:seeed_xiao_esp32s3_uart]
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_CDC_ON_BOOT=0   # UART mode
    -DARDUINO_USB_MODE=0          # UART mode
    -DARDUINO_USB_CDC_ON_BOOT=0   # Double-confirmed
```

## Expected Output

With UART mode, you should see:
```
=== ESP32-S3 BOOT START ===
Serial initialized

=== LOGGER INITIALIZED ===
Board: Seeed XIAO ESP32-S3
Free Heap: [bytes]
USB CDC: DISABLED          <-- Critical: Must show DISABLED
Logger Level: 3
========================
```

## If Still No Output

1. **Check COM Port**:
   ```bash
   pio device list
   ```

2. **Verify Driver**:
   - Windows: Device Manager should show "USB-Serial CH340" or "CP2102"
   - Install drivers if missing: https://www.silabs.com/developers/vcc-driver

3. **Test with Simple Sketch**:
   - Open `test_serial.ino`
   - Use `seeed_xiao_esp32s3_uart` environment
   - Build and upload
   - Open serial monitor

## Troubleshooting the 10-Blink Pattern

If you still see 10 blinks (panic/reboot):

### Check 1: I2S Microphone Connection
The INMP441 microphone must be connected to:
- WS (L/R): GPIO 3
- SCK: GPIO 2
- SD (DATA): GPIO 9
- VCC: 3.3V
- GND: GND

### Check 2: WiFi Connection
In `config.h`, verify:
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

### Check 3: Server Connection
```cpp
#define SERVER_HOST "192.168.1.50"
#define SERVER_PORT 9000
```

## Build Verification

After successful upload, check that:
- ✅ Serial output appears immediately
- ✅ "USB CDC: DISABLED" message
- ✅ No 10-blink pattern
- ✅ Device stays running

## Environment Comparison

| Environment | Serial Method | Status |
|------------|---------------|--------|
| `esp32dev` | UART0 | ✅ Working |
| `seeed_xiao_esp32s3` | UART0 | Should work |
| `seeed_xiao_esp32s3_uart` | UART0 | **Recommended** |

**Use `seeed_xiao_esp32s3_uart` for guaranteed UART mode!**
