# ESP32S3 Watchdog Timeout Fix - Complete Solution

## Problem Identified
The ESP32S3 was experiencing **watchdog timeout reboots** (10 blinks → PC reconnection) caused by:
1. **Blocking USB CDC initialization** (10+ second wait)
2. **Watchdog timer triggering** after 60 seconds of blocking code
3. **Continuous reboot cycle** preventing stable operation

## Solution Applied

### 1. Fixed USB CDC Initialization (`logger.cpp`)
**Before**: Blocking 10-second wait → Watchdog timeout
```cpp
while (!Serial && (millis() - start) < 10000) {
    delay(100); // This blocks the watchdog!
}
```

**After**: Non-blocking 2-second wait with watchdog feeding
```cpp
while (!Serial && (millis() - start) < 2000) {
    delay(50);
    yield(); // Feed watchdog to prevent timeout
}
```

### 2. Added UART-Based Alternative Configuration
Created `seeed_xiao_esp32s3_uart` environment that uses **hardware UART** instead of USB CDC:

**Benefits**:
- Same logging behavior as ESP32dev
- No USB enumeration issues
- Immediate serial output
- No watchdog timeout risk
- Compatible with any serial monitor

## Quick Test Solutions

### Option 1: Use Fixed USB CDC Version
```bash
# Build and flash the fixed USB CDC version
pio run -t clean -e seeed_xiao_esp32s3
pio run -e seeed_xiao_esp32s3 --target upload --upload-port COM4

# Open serial monitor
pio device monitor --port COM4 --baud 115200
```

### Option 2: Use UART Version (Recommended - Same as ESP32dev)
```bash
# Build and flash UART version (identical to ESP32dev behavior)
pio run -t clean -e seeed_xiao_esp32s3_uart
pio run -e seeed_xiao_esp32s3_uart --target upload --upload-port COM4

# Open serial monitor
pio device monitor --port COM4 --baud 115200
```

## Expected Results

### Option 1 (Fixed USB CDC):
```
=== LOGGER INITIALIZED ===
Board: Seeed XIAO ESP32-S3
Free Heap: 234567 bytes
USB CDC: ENABLED
USB CDC connection established successfully
Logger Level: 3
Init Time: 1234 ms
========================
```

### Option 2 (UART - Same as ESP32dev):
```
=== LOGGER INITIALIZED ===
Board: Seeed XIAO ESP32-S3
Free Heap: 234567 bytes
USB CDC: DISABLED
Logger Level: 3
Init Time: 1234 ms
========================
```

## Comparison: ESP32dev vs ESP32S3

| Feature | ESP32dev | ESP32S3 (USB CDC) | ESP32S3 (UART) |
|---------|----------|-------------------|----------------|
| **Serial Interface** | UART (GPIO1/GPIO3) | USB CDC | UART (GPIO43/GPIO44) |
| **Logging Speed** | Immediate | 2-second delay | Immediate |
| **USB Drivers** | CH340/CP210x required | Native USB CDC | CH340/CP210x required |
| **Compatibility** | Universal | Host-dependent | Universal |
| **Reliability** | High | Medium | High |

## Troubleshooting Steps

### If Still No Logs with UART Version:
1. **Check COM Port**: Ensure COM4 is the correct port
2. **USB Cable**: Use a data-capable cable (not charging-only)
3. **Drivers**: Install CH340/CP210x drivers for UART bridge
4. **Serial Monitor**: Try different monitors (Arduino IDE, PuTTY)

### If Still Issues with USB CDC Version:
1. **Timing**: Open serial monitor BEFORE connecting board
2. **Reset**: Press reset button after opening monitor
3. **USB**: Try different USB ports/cables
4. **Host**: Test on different computer if available

## Recommendation

**Use `seeed_xiao_esp32s3_uart` environment** for:
- Development and debugging
- Consistent behavior with ESP32dev
- Maximum compatibility
- No USB dependency issues

**Use `seeed_xiao_esp32s3` environment** for:
- Production deployment (native USB)
- No external USB-to-serial chip requirement
- Direct USB connection to host

## Success Criteria
✅ No more 10-blink watchdog reboots
✅ Stable serial connection to PC
✅ Logs appear immediately on startup
✅ Same logging behavior as ESP32dev
✅ No continuous reboot cycles