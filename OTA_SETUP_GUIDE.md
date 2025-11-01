# ESP32 OTA Update - Complete Setup Guide

**Device IP**: 192.168.1.24
**Date**: 2025-01-01
**Status**: OTA Configuration Ready (Code Implementation Required)

---

## ⚠️ Current Status

**OTA Upload Attempted**: ❌ FAILED
**Reason**: ArduinoOTA not running on the ESP32

The ESP32 at `192.168.1.24` is **reachable on the network** (ping successful), but it doesn't have OTA code running yet.

---

## 📡 What is OTA (Over-The-Air) Update?

OTA allows you to update ESP32 firmware wirelessly over WiFi without needing a USB connection.

### How It Works

```
┌─────────────┐                    ┌─────────────┐
│   Computer  │ ──WiFi Network──> │   ESP32     │
│ PlatformIO  │                    │ ArduinoOTA  │
└─────────────┘                    └─────────────┘
       │                                  │
       │ 1. Sends new firmware            │
       │────────────────────────────────> │
       │                                  │ 2. Receives & writes to flash
       │                                  │ 3. Reboots with new firmware
       │                                  │
       │ 4. Confirms success              │
       │ <─────────────────────────────── │
```

---

## 🔧 Step 1: Add OTA Code to Your Project

### 1.1 Update `src/main.cpp`

Add ArduinoOTA includes and initialization:

```cpp
// At the top of main.cpp (with other includes)
#include <ArduinoOTA.h>

// In setup() function - AFTER WiFi is connected
void setup() {
    // ... existing code (Serial, WiFi, etc.) ...

    // ===== OTA Configuration =====
    ArduinoOTA.setHostname("ESP32-AudioStreamer");
    // Optional: Add password for security
    // ArduinoOTA.setPassword("your_ota_password");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {  // U_SPIFFS
            type = "filesystem";
        }
        LOG_INFO("OTA Update Start: %s", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        LOG_INFO("OTA Update Complete - Rebooting");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int lastPercent = 0;
        unsigned int percent = (progress / (total / 100));
        if (percent != lastPercent && percent % 10 == 0) {
            LOG_INFO("OTA Progress: %u%%", percent);
            lastPercent = percent;
        }
    });

    ArduinoOTA.onError([](ota_error_t error) {
        LOG_ERROR("OTA Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            LOG_ERROR("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            LOG_ERROR("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            LOG_ERROR("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            LOG_ERROR("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            LOG_ERROR("End Failed");
        }
    });

    ArduinoOTA.begin();
    LOG_INFO("OTA Ready - IP: %s", WiFi.localIP().toString().c_str());

    // ... rest of setup code ...
}

// In loop() function
void loop() {
    ArduinoOTA.handle();  // Handle OTA update requests

    // ... rest of loop code ...
}
```

### 1.2 Where to Place OTA Code

**In `setup()`**:
- Add OTA initialization **AFTER** WiFi connection succeeds
- Add **BEFORE** entering main application logic

**In `loop()`**:
- Add `ArduinoOTA.handle()` at the **very beginning** of the loop
- This should be called frequently (every loop iteration)

### 1.3 Recommended Placement in Your Project

Based on your current `main.cpp` structure, add OTA initialization:
1. **After line ~80-100** (after WiFi is connected)
2. **Before** entering the main streaming loop

---

## 🔧 Step 2: PlatformIO Configuration

I've already added an OTA environment to your `platformio.ini`:

```ini
[env:esp32dev-ota]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =

build_flags =
    -DCORE_DEBUG_LEVEL=3

; OTA Upload Configuration
upload_protocol = espota
upload_port = 192.168.1.24
upload_flags =
    --port=3232

monitor_filters = esp32_exception_decoder
```

### To Use Different IP Address

If your ESP32 IP changes, update line 36:
```ini
upload_port = YOUR_ESP32_IP_HERE
```

---

## 🚀 Step 3: First Upload (USB Required)

Since OTA isn't running yet, you need to do **one USB upload** first:

### 3.1 Connect ESP32 via USB

### 3.2 Upload with OTA Code

```bash
# Upload via USB with OTA code included
python -m platformio run --environment esp32dev --target upload
```

### 3.3 Verify OTA is Running

Check serial monitor for:
```
OTA Ready - IP: 192.168.1.24
```

---

## 📡 Step 4: Perform OTA Updates (Future Updates)

Once OTA is running on the device, you can update wirelessly:

### 4.1 Check ESP32 is Reachable

```bash
ping 192.168.1.24
```

Should show replies like:
```
Reply from 192.168.1.24: bytes=32 time=104ms TTL=64
```

### 4.2 Upload via OTA

```bash
# Upload new firmware wirelessly
python -m platformio run --environment esp32dev-ota --target upload
```

### 4.3 Monitor Upload Progress

You should see:
```
Starting on 0.0.0.0:48780
Upload size: 788384
Sending invitation to 192.168.1.24
Uploading: [████████████████████] 100%
```

### 4.4 Check Serial Output (Optional)

If still connected via USB or using network monitoring:
```
OTA Update Start: sketch
OTA Progress: 10%
OTA Progress: 20%
...
OTA Progress: 100%
OTA Update Complete - Rebooting
```

---

## 🔐 Security Best Practices

### Add OTA Password (Recommended)

In `main.cpp` setup:
```cpp
ArduinoOTA.setPassword("your_secure_password_here");
```

In `platformio.ini`:
```ini
upload_flags =
    --port=3232
    --auth=your_secure_password_here
```

### Why Use Password?

- Prevents unauthorized firmware uploads
- Secures your device from network attacks
- Essential for production deployments

---

## 🐛 Troubleshooting

### "No response from the ESP"

**Symptoms**:
```
Sending invitation to 192.168.1.24 ..........
[ERROR]: No response from the ESP
```

**Possible Causes**:
1. ❌ **ArduinoOTA not running** (most common)
   - Solution: Upload OTA code via USB first
2. ❌ **Wrong IP address**
   - Solution: Check `WiFi.localIP()` in serial monitor
3. ❌ **Firewall blocking port 3232**
   - Solution: Allow port 3232 in Windows Firewall
4. ❌ **ESP32 crashed or rebooted**
   - Solution: Check serial monitor for errors
5. ❌ **`ArduinoOTA.handle()` not called in loop**
   - Solution: Ensure handle() is in main loop

### Check OTA is Running

Connect via USB serial and look for:
```
OTA Ready - IP: 192.168.1.24
```

If you don't see this message, OTA is not initialized.

### Firewall Issues (Windows)

Add firewall rule for port 3232:
```bash
netsh advfirewall firewall add rule name="ESP32 OTA" dir=in action=allow protocol=TCP localport=3232
```

### ESP32 Keeps Rebooting During OTA

**Possible Causes**:
- Not enough free flash space
- Corrupted firmware binary
- Power supply issues during upload

**Solutions**:
- Check flash usage in build output
- Ensure stable power supply (quality USB cable)
- Try slower upload (add `--timeout=30` to upload_flags)

---

## 📊 Current Build Status

**Last Build**:
```
Environment: esp32dev-ota
Status: SUCCESS
Build Time: 105.96 seconds
Binary Size: 788,384 bytes (770 KB)
RAM Usage: 15.0% (49,232 / 327,680 bytes)
Flash Usage: 59.6% (781,805 / 1,310,720 bytes)
```

**OTA Upload**: ❌ FAILED (ArduinoOTA not running on device)

---

## ✅ Next Steps Checklist

To enable OTA on your ESP32:

- [ ] **Step 1**: Add ArduinoOTA code to `src/main.cpp`
  - [ ] Add `#include <ArduinoOTA.h>` at top
  - [ ] Add OTA initialization in `setup()` after WiFi
  - [ ] Add `ArduinoOTA.handle()` in `loop()`

- [ ] **Step 2**: Upload via USB (one-time)
  - [ ] Connect ESP32 via USB
  - [ ] Run: `pio run -e esp32dev --target upload`
  - [ ] Verify "OTA Ready" message in serial monitor

- [ ] **Step 3**: Test OTA Upload
  - [ ] Ping 192.168.1.24 (should reply)
  - [ ] Run: `pio run -e esp32dev-ota --target upload`
  - [ ] Verify successful wireless upload

- [ ] **Step 4** (Optional): Add OTA password for security

---

## 📝 Quick Reference Commands

### USB Upload (with OTA code)
```bash
python -m platformio run --environment esp32dev --target upload
```

### OTA Upload (wireless)
```bash
python -m platformio run --environment esp32dev-ota --target upload
```

### Monitor Serial Output
```bash
python -m platformio device monitor --environment esp32dev
```

### Build Only (no upload)
```bash
python -m platformio run --environment esp32dev
```

### Check Device Reachability
```bash
ping 192.168.1.24
```

---

## 🔗 Additional Resources

- **ArduinoOTA Documentation**: https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update
- **ESP32 OTA Guide**: https://randomnerdtutorials.com/esp32-over-the-air-ota-programming/
- **PlatformIO Espota**: https://github.com/platformio/platformio-core/blob/develop/platformio/builder/tools/espota.py

---

**Status**: Configuration complete, awaiting code implementation
**Next Action**: Add ArduinoOTA code to main.cpp and upload via USB
