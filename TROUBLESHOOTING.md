# ESP32 Audio Streamer - Troubleshooting Guide

Comprehensive solutions for common issues and problems.

---

## Startup Issues

### System Fails Configuration Validation

**Error Message:**
```
Configuration validation failed - cannot start system
Please check config.h and fix the issues listed above
```

**Possible Issues:**
- WiFi SSID is empty
- WiFi password is empty
- SERVER_HOST is empty
- SERVER_PORT is 0 or missing
- Invalid timeout values

**Solution:**
1. Open `src/config.h`
2. Look at the validation output - it lists exactly what's missing
3. Fill in all required fields:
   ```cpp
   #define WIFI_SSID       "YourNetwork"
   #define WIFI_PASSWORD   "YourPassword"
   #define SERVER_HOST     "192.168.1.100"
   #define SERVER_PORT     9000
   ```
4. Rebuild and upload: `pio run && pio run --target upload`

---

### "I2S Initialization Failed"

**Error Message:**
```
I2S initialization failed - cannot continue
```

**Possible Causes:**
- INMP441 microphone not connected
- Wrong GPIO pins configured
- Pin conflict with other peripherals
- Bad solder joints on INMP441

**Troubleshooting Steps:**

1. **Verify wiring** - Double-check INMP441 connections:
   ```
   INMP441 → ESP32
   VDD → 3.3V
   GND → GND
   SCK → GPIO 14 (ESP32-Dev) or GPIO 2 (XIAO)
   WS  → GPIO 15 (ESP32-Dev) or GPIO 3 (XIAO)
   SD  → GPIO 32 (ESP32-Dev) or GPIO 9 (XIAO)
   L/R → GND (force left channel)
   ```

2. **Check for pin conflicts:**
   - GPIO 14/15/32 shouldn't be used by other code
   - Verify no serial or other peripherals on these pins

3. **Test with meter:**
   - Measure 3.3V at INMP441 VDD pin
   - Confirm GND connections are solid

4. **Try XIAO board (if using ESP32-Dev):**
   - Different pins might resolve the issue
   - Change board in `platformio.ini`

5. **Replace INMP441:**
   - Microphone may be defective
   - Try a fresh module

---

### Watchdog Resets Every 10 Seconds

**Symptoms:**
- System restarts repeatedly
- Serial monitor shows "…" patterns
- Watchdog timeout message

**Root Cause:**
The main loop is blocked for more than 10 seconds without feeding the watchdog timer.

**Solutions:**

1. **Check for blocking delays:**
   - Search code for `delay(X)` where X > 10000
   - Replace with non-blocking timers using `NonBlockingTimer`

2. **Increase watchdog timeout** (temporary debug only):
   ```cpp
   #define WATCHDOG_TIMEOUT_SEC 20  // Increase to 20 sec
   ```

3. **Debug serial output:**
   - Add more LOG_INFO messages to find where code blocks
   - Monitor with: `pio device monitor --baud 115200`

4. **Most common culprit**: WiFi connection attempt timing out
   - Verify WIFI_TIMEOUT < WATCHDOG_TIMEOUT_SEC
   - Current: WiFi timeout 30s, Watchdog 10s = CONFLICT!
   - Fix: Set WATCHDOG_TIMEOUT_SEC to 40 or higher

---

## WiFi Connection Issues

### WiFi SSID Not Found / Connection Fails

**Symptoms:**
- "Connecting to WiFi..." but never connects
- Frequent timeout errors
- "WiFi lost" messages after brief connection

**Checklist:**

1. **Verify SSID is correct:**
   ```cpp
   #define WIFI_SSID "ExactSSIDName"  // Case-sensitive!
   ```
   - Check your phone's WiFi list for exact name
   - Ensure no typos (copy-paste from phone)

2. **Verify password is correct:**
   ```cpp
   #define WIFI_PASSWORD "YourPassword"  // Must be exact
   ```
   - Try connecting from laptop first to verify password works
   - Common issue: accidentally including spaces

3. **Router must be 2.4GHz:**
   - ESP32 does NOT support 5GHz
   - Check router settings - many routers have both bands
   - Disable 5GHz band or create 2.4GHz-only SSID

4. **Check signal strength:**
   - Move ESP32 closer to router
   - Try without walls/obstacles in between
   - Target: -50 to -70 dBm (good signal)

5. **Restart router:**
   - Power cycle the WiFi router
   - Wait for full boot (30-60 seconds)
   - Try connecting again

6. **Update WiFi settings:**
   - Some routers use WEP (very old, unsupported)
   - Switch to WPA2 (standard, secure)
   - Ensure WiFi is on and broadcasting SSID

---

### "WiFi Lost During Streaming"

**Symptoms:**
- Connects successfully, then disconnects
- Frequent reconnections (every 1-5 minutes)
- Works briefly then stops

**Troubleshooting:**

1. **Improve signal strength:**
   - Move ESP32 closer to router
   - Remove obstacles (metal, water, thick walls)
   - Try a WiFi extender
   - Current signal shown in logs: `-XX dBm`

2. **Check for interference:**
   - Other WiFi networks operating on same channel
   - Use WiFi analyzer app to find empty channel
   - Configure router to use channel 1, 6, or 11

3. **Reduce reconnection aggressiveness:**
   - If reconnecting constantly, may be hurting signal
   - Increase WIFI_RETRY_DELAY to give signal time:
   ```cpp
   #define WIFI_RETRY_DELAY 2000  // Wait 2 sec between attempts
   ```

4. **Check for weak network:**
   - Many devices connected to same router
   - Router may be older/underpowered
   - Try with fewer connected devices

5. **Update router firmware:**
   - Older firmware may have WiFi bugs
   - Check manufacturer's website for updates

6. **Try static IP** (might improve stability):
   ```cpp
   #define USE_STATIC_IP
   #define STATIC_IP 192, 168, 1, 100
   ```

---

## Server Connection Issues

### Can't Connect to Server

**Symptoms:**
- WiFi connects fine
- Never reaches server
- Constant reconnection attempts

**Verification Steps:**

1. **Test server from PC/phone:**
   ```bash
   # Windows CMD
   telnet 192.168.1.100 9000

   # Linux/Mac
   nc -zv 192.168.1.100 9000
   ```
   - If this works on PC, server is reachable

2. **Verify SERVER_HOST:**
   ```cpp
   #define SERVER_HOST "192.168.1.100"  // Not "192.168.1.100:9000"
   #define SERVER_PORT 9000              // Port separate!
   ```
   - Don't include port in hostname
   - Numeric IP is more reliable than domain names

3. **Check SERVER_PORT:**
   ```cpp
   #define SERVER_PORT 9000  // Must be numeric, not "9000"
   ```

4. **Firewall blocking:**
   - Check Windows Defender / antivirus
   - Add exception for port 9000
   - Temporarily disable firewall to test

5. **Server not running:**
   - Verify server process is actually running
   - Check server logs for errors
   - Test: Can you connect from another PC?

6. **Wrong IP address:**
   - Use `ipconfig` (Windows) or `ifconfig` (Linux) to find server IP
   - Don't use 127.0.0.1 - that's localhost only
   - Must be on same network as ESP32

7. **Network isolation:**
   - Check if guest network is isolated
   - Check if ESP32 device is on trusted network
   - Routers often isolate IoT devices

---

### Server Connects Then Disconnects

**Symptoms:**
- Brief connection, then "Server connection lost"
- Rapid reconnection loop
- Data sent but then disconnected

**Causes & Solutions:**

1. **Server closing connection intentionally:**
   - Check server logs for why it closed
   - May be protocol mismatch or invalid data format

2. **Network timeout:**
   - Increase TCP_WRITE_TIMEOUT:
   ```cpp
   #define TCP_WRITE_TIMEOUT 10000  // 10 seconds
   ```
   - May help if data transmission is slow

3. **Keepalive not working:**
   - Server may close idle connections
   - Current system has TCP keepalive enabled
   - Verify server supports keepalive

4. **Intermittent network issues:**
   - Check for packet loss: `ping -t 192.168.1.100`
   - Look for timeouts in ping output
   - May indicate bad cable or interference

---

## Audio/I2S Issues

### No Audio Data Received at Server

**Symptoms:**
- System connects successfully
- No data reaching server
- Error logs show "I2S read failed"

**Debugging Steps:**

1. **Check I2S error count:**
   - Every 5 minutes, system prints statistics
   - Look for: `I2S errors: X`
   - If increasing, I2S is failing

2. **Verify microphone is working:**
   - Connect multimeter to INMP441 SD (data) pin
   - Should see signal activity (voltage fluctuations)
   - No activity = microphone not producing signal

3. **Check INMP441 power:**
   - Measure 3.3V at VDD pin
   - Measure GND connection
   - Both must be solid (use volt meter)

4. **Verify clock signals:**
   - SCK (clock) pin should show ~1 MHz square wave
   - WS (sync) pin should show ~16 kHz square wave
   - Requires oscilloscope to verify

5. **Try increasing I2S buffer:**
   ```cpp
   #define I2S_DMA_BUF_COUNT 16  // More DMA buffers
   #define I2S_BUFFER_SIZE 8192  // Larger main buffer
   ```

6. **Reduce other processing:**
   - High CPU load may cause I2S to miss data
   - Check memory usage - if low, increase warning threshold

---

### I2S Read Errors After Hours of Operation

**Symptoms:**
- Works fine initially
- After 1+ hours, I2S errors start
- Eventually stops receiving audio

**Likely Cause:**
Memory leak causing I2S buffers to fragment.

**Solution:**

1. **Check memory statistics:**
   - Look at stats output every 5 min
   - Watch free heap trend
   - If constantly decreasing = memory leak

2. **Increase check intervals** to monitor better:
   ```cpp
   #define MEMORY_CHECK_INTERVAL 30000  // Check every 30 sec
   #define STATS_PRINT_INTERVAL 120000  // Print every 2 min
   ```

3. **Identify leak source:**
   - May be in I2S, WiFi, or TCP code
   - Check if error count increases with I2S failures
   - Compare memory before/after disconnect

4. **Workaround**: Periodic restart:
   - Automatic restart if heap < 20KB (built-in)
   - Or schedule daily restart via code

---

## Memory & Performance Issues

### "Memory Low" Warnings Appearing

**Symptoms:**
```
Memory low: 35000 bytes
```

**Not Critical But Monitor:**

1. **Check what's using memory:**
   - Larger I2S buffers use more RAM
   - Multiple network connections use more RAM
   - Logging buffers use more RAM

2. **Reduce non-essential buffers:**
   ```cpp
   #define I2S_BUFFER_SIZE 2048   // Reduce from 4096
   #define I2S_DMA_BUF_COUNT 4    // Reduce from 8
   ```

3. **Increase check frequency:**
   - See if memory is stable or trending down
   - Stable = normal operation
   - Decreasing = potential leak

---

### "Critical Low Memory" - System Restarting

**Symptoms:**
- System constantly restarting
- Memory reaching < 20KB
- "Memory critically low - initiating graceful restart"

**Solution:**

This is a safety feature - system is protecting itself from crash.

1. **Immediate action:**
   - Disconnect from WiFi
   - Recompile without I2S
   - Identify memory leak

2. **Find the leak:**
   - Check for unbounded allocations
   - Look for string concatenations in loops
   - Verify no circular queue buildup

3. **Temporary workaround:**
   - Increase critical threshold (not recommended):
   ```cpp
   #define MEMORY_CRITICAL_THRESHOLD 10000  // More aggressive
   ```
   - Better: Fix the actual leak

4. **Use memory profiling:**
   - Add memory tracking at key points
   - Print heap before/after sections
   - Narrow down leak source

---

## Build & Upload Issues

### "Board Not Found" During Upload

**Error:**
```
Error: No device found on COM port
```

**Solutions:**

1. **Check USB connection:**
   - Try different USB port
   - Try different USB cable (some are charge-only)
   - Ensure device is powered

2. **Install drivers:**
   - Windows: Download CH340 driver
   - Mac/Linux: Usually automatic

3. **Identify COM port:**
   ```bash
   # Windows - list COM ports
   mode

   # Linux
   ls /dev/ttyUSB*
   ```

4. **Check platformio.ini:**
   ```ini
   [env:esp32dev]
   upload_port = COM3  # or /dev/ttyUSB0
   monitor_port = COM3
   ```

5. **Reset ESP32:**
   - Press RESET button on board
   - Try upload again immediately

---

### Compilation Errors

**Common error: "CONFIG_VALIDATION not found"**

Make sure you included the validator in main.cpp:
```cpp
#include "config_validator.h"
```

**Rebuild:**
```bash
pio run --target clean
pio run
```

---

### Very Slow Build Times

If build takes > 10 minutes:

1. **Clear build cache:**
   ```bash
   pio run --target clean
   pio run
   ```

2. **Increase build speed:**
   ```bash
   pio run -j 4  # Use 4 parallel jobs
   ```

3. **Check disk space:**
   - `.pio` directory uses ~2GB
   - Ensure you have free space

---

## Performance & Bandwidth Issues

### Slow Data Transmission / Dropped Packets

**Symptoms:**
- Data rate lower than expected (< 32 KB/s)
- Server shows gaps in audio
- TCP write errors in logs

**Solutions:**

1. **Check TCP buffer size:**
   ```cpp
   #define TCP_WRITE_TIMEOUT 5000  // Give more time
   ```
   - Increase from 5s to 10s if timeout errors occur

2. **Reduce other WiFi interference:**
   - Disable other devices briefly
   - Test with just ESP32 on network
   - Move away from other RF sources

3. **Verify network path:**
   - Test PC → Server (should be fast)
   - Then test ESP32 → Server
   - Compare speeds

4. **Check WiFi signal:**
   - Stronger signal = higher bitrate
   - Target: -50 to -70 dBm
   - Move closer to router

5. **Monitor buffer status:**
   - Add logging to track buffer fullness
   - May indicate bottleneck

---

## Serial Monitor Issues

### No Output on Serial Monitor

**Symptoms:**
- Run: `pio device monitor`
- No text appears

**Solutions:**

1. **Check correct COM port:**
   ```bash
   pio device monitor -p COM3 --baud 115200
   ```
   - List ports: `mode` (Windows) or `ls /dev/ttyUSB*` (Linux)

2. **Verify baud rate:**
   ```bash
   pio device monitor --baud 115200  # MUST be 115200
   ```

3. **Reset board during monitor startup:**
   - Press RESET button
   - Quickly switch to monitor terminal
   - Catch startup logs

4. **Check if board is working:**
   - LED should blink (if present)
   - Check board for power indicator

---

### Serial Monitor "Garbage" Output

**Symptoms:**
- See random characters instead of text
```
ÛiÜ¶ÚƒÁûÂÚ
```

**Cause:**
Wrong baud rate.

**Solution:**
```bash
pio device monitor --baud 115200  # Must match config
```

---

## Advanced Debugging

### Enable Verbose Logging

Edit `src/logger.h` to uncomment DEBUG level:

```cpp
#define LOG_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
```

Recompile and watch for detailed messages.

### Add Debug Breakpoints

Modify `main.cpp` to add strategic logging:

```cpp
// In CONNECTED state
LOG_INFO("[DEBUG] About to read I2S...");
if (I2SAudio::readDataWithRetry(audio_buffer, I2S_BUFFER_SIZE, &bytes_read)) {
    LOG_INFO("[DEBUG] I2S read OK: %u bytes", bytes_read);
    // ...
} else {
    LOG_ERROR("[DEBUG] I2S read FAILED");
}
```

### Monitor Real-Time Stats

Run serial monitor and watch stats output every 5 minutes:

```bash
pio device monitor --baud 115200 | grep -E "Statistics|Memory|Error|Reconnect"
```

---

## When All Else Fails

### Factory Reset

```cpp
// Edit src/config.h to default settings
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define SERVER_HOST ""
#define SERVER_PORT 0

// Recompile and upload
pio run && pio run --target upload
```

### USB Reset (Windows)

Uninstall and reinstall USB drivers:
- Device Manager → Ports → CH340
- Right-click → Uninstall device
- Replug USB cable
- Windows auto-installs driver

### Complete Clean Build

```bash
# Remove all build artifacts
pio run --target clean

# Deep clean all libraries
pio pkg update

# Rebuild from scratch
pio run && pio run --target upload
```

---

## Getting Help

1. **Check ERROR_HANDLING.md** - Explains all system states
2. **Check CONFIGURATION_GUIDE.md** - Explains all settings
3. **Review Serial Output** - Often indicates exact problem
4. **Search logs for CRITICAL/ERROR** - Tells you what failed
5. **Check connectivity** - Verify WiFi and server separately

---

## Contact & Reporting Issues

When reporting issues, include:
1. **Serial monitor output** (startup + first 100 lines of operation)
2. **Configuration values** (SSID, SERVER_HOST, timeouts)
3. **Hardware setup** (board type, microphone, wiring)
4. **How long before issue** (immediate vs after hours)
5. **Steps to reproduce** (what you did when it happened)
