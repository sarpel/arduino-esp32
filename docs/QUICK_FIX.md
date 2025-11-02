# ESP32S3 Serial Fix - Quick Reference

## Problem
ESP32S3 blinks 10 times and reconnects to PC, no visible logs (ESP32 works fine)

## Root Cause
ESP32S3 was not configured for UART mode (same as ESP32dev)

## Solution Applied ✅

### Configuration Changed (platformio.ini)
```ini
[env:seeed_xiao_esp32s3]
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=0   # UART mode (not USB CDC)
    -DARDUINO_USB_MODE=0
```

This makes ESP32S3 work exactly like ESP32dev with UART0.

## Next Steps

### 1. Build and Upload
```bash
pio run --target clean
pio run --environment seeed_xiao_esp32s3
pio device upload --environment seeed_xiao_esp32s3 --port COM4
```

### 2. Open Serial Monitor
```bash
pio device monitor --port COM4 --baud 115200
```

### 3. Expected Output
```
=== ESP32-S3 BOOT START ===
Serial initialized

=== LOGGER INITIALIZED ===
Board: Seeed XIAO ESP32-S3
Free Heap: [bytes]
USB CDC: DISABLED          <-- Shows UART mode
Logger Level: 3
========================
```

### 4. Success Indicators
- ✅ "USB CDC: DISABLED" visible
- ✅ No 10-blink panic pattern
- ✅ Device stays running
- ✅ Same behavior as ESP32dev

## Troubleshooting

**If still no output:**
1. Check Device Manager for "USB-Serial CH340" or "CP2102"
2. Install CH340/CP2102 drivers if missing
3. Try different USB port
4. Verify baud rate is 115200

**Full documentation:** `docs/esp32s3_serial_troubleshooting.md`
