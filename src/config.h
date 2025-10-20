#ifndef CONFIG_H
#define CONFIG_H

// ===== WiFi Configuration =====
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define WIFI_RETRY_DELAY 500 // milliseconds
#define WIFI_MAX_RETRIES 20
#define WIFI_TIMEOUT 30000 // milliseconds

// ===== WiFi Static IP (Optional) =====
// Uncomment to use static IP instead of DHCP
// #define USE_STATIC_IP
#define STATIC_IP 0, 0, 0, 0
#define GATEWAY_IP 0, 0, 0, 0
#define SUBNET_MASK 0, 0, 0, 0
#define DNS_IP 0, 0, 0, 0

// ===== Server Configuration =====
#define SERVER_HOST ""
#define SERVER_PORT 0
#define SERVER_RECONNECT_MIN 5000  // milliseconds
#define SERVER_RECONNECT_MAX 60000 // milliseconds
#define TCP_WRITE_TIMEOUT 5000     // milliseconds

// ===== Board Detection =====
#ifdef ARDUINO_SEEED_XIAO_ESP32S3
    #define BOARD_XIAO_ESP32S3
    #define BOARD_NAME "Seeed XIAO ESP32-S3"
#else
    #define BOARD_ESP32DEV
    #define BOARD_NAME "ESP32-DevKit"
#endif

// ===== I2S Hardware Pins =====
#ifdef BOARD_XIAO_ESP32S3
    #define I2S_WS_PIN 3
    #define I2S_SD_PIN 9
    #define I2S_SCK_PIN 2
#else
    #define I2S_WS_PIN 15
    #define I2S_SD_PIN 32
    #define I2S_SCK_PIN 14
#endif

// ===== I2S Parameters =====
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE 16000
#define I2S_BUFFER_SIZE 4096
#define I2S_DMA_BUF_COUNT 8
#define I2S_DMA_BUF_LEN 256

// ===== Reliability Thresholds =====
#define MEMORY_WARN_THRESHOLD 40000     // bytes
#define MEMORY_CRITICAL_THRESHOLD 20000 // bytes
#define RSSI_WEAK_THRESHOLD -80         // dBm
#define MAX_CONSECUTIVE_FAILURES 10
#define I2S_MAX_READ_RETRIES 3

// ===== Timing Configuration =====
#define MEMORY_CHECK_INTERVAL 60000 // 1 minute
#define RSSI_CHECK_INTERVAL 10000   // 10 seconds
#define STATS_PRINT_INTERVAL 300000 // 5 minutes

// ===== System Initialization & Timeouts =====
#define SERIAL_INIT_DELAY 1000          // milliseconds - delay after serial init
#define GRACEFUL_SHUTDOWN_DELAY 100     // milliseconds - delay between shutdown steps
#define ERROR_RECOVERY_DELAY 5000       // milliseconds - delay before recovery attempt
#define TASK_YIELD_DELAY 1              // milliseconds - delay in main loop for background tasks

// ===== TCP Keepalive Configuration =====
#define TCP_KEEPALIVE_IDLE 5            // seconds - idle time before keepalive probe
#define TCP_KEEPALIVE_INTERVAL 5        // seconds - interval between keepalive probes
#define TCP_KEEPALIVE_COUNT 3           // count - number of keepalive probes before disconnect

// ===== Logger Configuration =====
#define LOGGER_BUFFER_SIZE 256          // bytes - circular buffer for log messages

// ===== Watchdog Configuration =====
#define WATCHDOG_TIMEOUT_SEC 10         // seconds - watchdog timeout (ESP32 feeds it in loop)

// ===== Task Priorities =====
#define TASK_PRIORITY_HIGH 5            // reserved for critical tasks
#define TASK_PRIORITY_NORMAL 3          // default priority
#define TASK_PRIORITY_LOW 1             // background tasks

// ===== State Machine Timeouts =====
#define STATE_CHANGE_DEBOUNCE 100       // milliseconds - debounce state transitions

#endif // CONFIG_H
