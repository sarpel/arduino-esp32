#!/usr/bin/env python3
"""
ESP32S3 Diagnostic Script
Helps identify why logs/audio sending aren't working
"""

import subprocess
import sys
import time

def run_command(cmd):
    """Run a command and return output"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=10)
        return result.stdout, result.stderr, result.returncode
    except Exception as e:
        return "", str(e), 1

def check_environment():
    """Check which environment is configured"""
    print("=" * 60)
    print("CHECKING PLATFORMIO CONFIGURATION")
    print("=" * 60)

    # Check platformio.ini
    stdout, stderr, code = run_command("find . -name platformio.ini -exec cat {} \\;")
    print("\n1. ESP32S3 Environment Configuration:")
    if "seeed_xiao_esp32s3" in stdout:
        print("   ✅ seeed_xiao_esp32s3 environment found")
        if "ARDUINO_USB_CDC_ON_BOOT=0" in stdout:
            print("   ✅ UART mode configured (ARDUINO_USB_CDC_ON_BOOT=0)")
        else:
            print("   ⚠️  USB CDC may be enabled")
    else:
        print("   ❌ seeed_xiao_esp32s3 environment not found")

    if "seeed_xiao_esp32s3_uart" in stdout:
        print("   ✅ seeed_xiao_esp32s3_uart environment found")
    else:
        print("   ❌ seeed_xiao_esp32s3_uart environment not found")

def check_serial_devices():
    """Check for serial devices"""
    print("\n" + "=" * 60)
    print("CHECKING SERIAL DEVICES")
    print("=" * 60)

    stdout, stderr, code = run_command("pio device list")
    print("\n2. Available Serial Devices:")
    if stdout:
        print(stdout)
        if "COM" in stdout or "/dev/tty" in stdout:
            print("   ✅ Serial devices detected")
        else:
            print("   ⚠️  No COM or tty devices found")
    else:
        print("   ❌ No serial devices detected")
        print("   Make sure ESP32S3 is connected via USB")

def check_drivers():
    """Check for USB drivers (Windows)"""
    print("\n" + "=" * 60)
    print("CHECKING USB DRIVERS")
    print("=" * 60)

    print("\n3. USB Driver Check:")
    print("   Windows: Look for 'USB-Serial CH340' or 'CP2102' in Device Manager")
    print("   Linux: Run 'lsusb' to see USB devices")
    print("   Mac: Check System Information > Hardware > USB")
    print("")
    print("   Required drivers:")
    print("   - CH340 driver: https://www.silabs.com/drivers/vcc-driver")
    print("   - CP2102 driver: https://www.silabs.com/drivers/cp2102")

def suggest_solution():
    """Suggest the correct solution"""
    print("\n" + "=" * 60)
    print("RECOMMENDED SOLUTION")
    print("=" * 60)

    print("\n4. Build Commands (use seeed_xiao_esp32s3_uart environment):")
    print("")
    print("   # Clean build")
    print("   pio run --target clean --environment seeed_xiao_esp32s3_uart")
    print("")
    print("   # Build")
    print("   pio run --environment seeed_xiao_esp32s3_uart")
    print("")
    print("   # Upload (replace COM4 with your port)")
    print("   pio device upload --environment seeed_xiao_esp32s3_uart --port COM4")
    print("")
    print("   # Open serial monitor")
    print("   pio device monitor --port COM4 --baud 115200")
    print("")
    print("5. Expected Output:")
    print("   === ESP32-S3 BOOT START ===")
    print("   Serial initialized")
    print("   === LOGGER INITIALIZED ===")
    print("   Board: Seeed XIAO ESP32-S3")
    print("   USB CDC: DISABLED  ← Must show DISABLED")
    print("   ========================")

def check_config():
    """Check configuration"""
    print("\n" + "=" * 60)
    print("CHECKING CONFIGURATION")
    print("=" * 60)

    print("\n6. Verify config.h settings:")
    stdout, stderr, code = run_command("grep -E \"WIFI_SSID|WIFI_PASSWORD|SERVER_HOST\" src/config.h")
    if stdout:
        print("   WiFi and Server settings:")
        for line in stdout.strip().split('\n'):
            if line.strip():
                print(f"   {line}")

    print("\n7. I2S Pin Configuration:")
    stdout, stderr, code = run_command("grep -A 5 \"I2S Hardware Pins\" src/config.h")
    if stdout:
        print("   " + "\n   ".join(stdout.strip().split('\n')))

def main():
    """Main diagnostic function"""
    print("\n")
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 10 + "ESP32S3 DIAGNOSTIC TOOL" + " " * 23 + "║")
    print("╚" + "=" * 58 + "╝")
    print()

    check_environment()
    check_serial_devices()
    check_drivers()
    check_config()
    suggest_solution()

    print("\n" + "=" * 60)
    print("NEXT STEPS")
    print("=" * 60)
    print("\n1. Connect ESP32S3 via USB cable")
    print("2. Note the COM port frompio device list")
    print("3. Run the build commands above")
    print("4. Open serial monitor")
    print("5. If you see 'USB CDC: DISABLED', UART mode is working!")
    print("6. If you see 10-blink pattern, check I2S microphone connection")
    print("")
    print("For more help, see BUILD_INSTRUCTIONS.md")
    print("")

if __name__ == "__main__":
    main()
