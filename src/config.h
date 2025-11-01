#ifndef CONFIG_H
#define CONFIG_H

// ===== WiFi Configuration =====
#define WIFI_SSID "Sarpel_2G"
#define WIFI_PASSWORD "penguen1988"
#define WIFI_RETRY_DELAY 2000 // milliseconds
#define WIFI_MAX_RETRIES 20
#define WIFI_TIMEOUT 30000 // milliseconds

// ===== WiFi Static IP (Optional) =====
// Uncomment to use static IP instead of DHCP
#define USE_STATIC_IP
#define STATIC_IP 192, 168, 1, 27
#define GATEWAY_IP 192, 168, 1, 1
#define SUBNET_MASK 255, 255, 255, 0
#define DNS_IP 1, 1, 1, 1

// ===== Server Configuration =====
#define SERVER_HOST "192.168.1.50"
#define SERVER_PORT 9000
#define SERVER_RECONNECT_MIN 5000    // milliseconds
#define SERVER_RECONNECT_MAX 60000   // milliseconds
#define SERVER_BACKOFF_JITTER_PCT 20 // percent jitter on backoff (0-100)
#define TCP_WRITE_TIMEOUT 5000       // milliseconds - timeout for send operations
#define TCP_RECEIVE_TIMEOUT 10000    // milliseconds - timeout for receive operations (primarily for protocol compliance)

// TCP chunk size MUST match server's TCP_CHUNK_SIZE expectation for proper streaming
// Server (receiver.py) expects 19200 bytes per chunk:
//   - 9600 samples × 2 bytes/sample = 19200 bytes
//   - Duration: 9600 samples ÷ 16000 Hz = 0.6 seconds = 600ms of audio
//   - Data rate: 19200 bytes ÷ 0.6 sec = 32000 bytes/sec = 32 KB/sec
// This aligns with server's SO_RCVBUF=65536 and socket receive loop optimization
#define TCP_CHUNK_SIZE 19200 // bytes per write() chunk - MUST match server receiver.py

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
#define I2S_WS_PIN 25
#define I2S_SD_PIN 34
#define I2S_SCK_PIN 26
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
#define SERIAL_INIT_DELAY 1000      // milliseconds - delay after serial init
#define GRACEFUL_SHUTDOWN_DELAY 100 // milliseconds - delay between shutdown steps
#define ERROR_RECOVERY_DELAY 5000   // milliseconds - delay before recovery attempt
#define TASK_YIELD_DELAY 1          // milliseconds - delay in main loop for background tasks

// ===== TCP Keepalive Configuration =====
#define TCP_KEEPALIVE_IDLE 5     // seconds - idle time before keepalive probe
#define TCP_KEEPALIVE_INTERVAL 5 // seconds - interval between keepalive probes
#define TCP_KEEPALIVE_COUNT 3    // count - number of keepalive probes before disconnect

// ===== Logger Configuration =====
#define LOGGER_BUFFER_SIZE 64      // bytes - circular buffer for log messages
#define LOGGER_MAX_LINES_PER_SEC 5 // rate limit to avoid log storms
#define LOGGER_BURST_MAX 20        // maximum burst of logs allowed

// ===== Watchdog Configuration =====
#define WATCHDOG_TIMEOUT_SEC 60 // seconds - watchdog timeout (aligned with connection operations)

// ===== Task Priorities =====
#define TASK_PRIORITY_HIGH 7   // reserved for critical tasks
#define TASK_PRIORITY_NORMAL 4 // default priority
#define TASK_PRIORITY_LOW 1    // background tasks

// ===== State Machine Timeouts =====
#define STATE_CHANGE_DEBOUNCE 100 // milliseconds - debounce state transitions

// ===== State Timeout Thresholds =====
// These timeouts detect when a state is stuck and trigger recovery
#define WIFI_CONNECT_TIMEOUT_MS 30000   // 30 seconds - WiFi connection timeout
#define SERVER_CONNECT_TIMEOUT_MS 10000 // 10 seconds - Server connection timeout
#define INITIALIZING_TIMEOUT_MS 10000   // 10 seconds - System initialization timeout
#define ERROR_RECOVERY_TIMEOUT_MS 60000 // 60 seconds - Max time in ERROR state before force reset

// ===== Debug Configuration =====
// Compile-time debug level (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)
// Set to 0 for production (minimal logging), 3+ for development
#define DEBUG_LEVEL 3

// ===== Phase 1: Network Resilience Configuration =====
// Enable/disable reliability features
#define ENABLE_MULTI_WIFI false                 // Multi-WiFi with automatic failover
#define ENABLE_NETWORK_QUALITY_MONITORING false // WiFi quality tracking
#define ENABLE_CONNECTION_POOL false            // Connection pool management
#define ENABLE_ADAPTIVE_RECONNECTION true       // Adaptive reconnection strategies

// Network Resilience Parameters
#define MULTI_WIFI_MAX_NETWORKS 1              // Maximum number of WiFi networks
#define NETWORK_QUALITY_CHECK_INTERVAL 10000   // Quality check every 10 seconds
#define NETWORK_SWITCH_COOLDOWN 30000          // Min interval between network switches (30s)
#define CONNECTION_POOL_SIZE 3                 // Max connections in pool
#define CONNECTION_HEALTH_CHECK_INTERVAL 10000 // Connection health check interval (10s)

// Reconnection Strategy
#define RECONNECT_BASE_DELAY_MS 1000 // Base exponential backoff
#define RECONNECT_MAX_DELAY_MS 60000 // Maximum backoff (60s)
#define RECONNECT_JITTER_PERCENT 20  // Random jitter ±20%

// Network Quality Thresholds
// Quality score is calculated from RSSI, latency, and packet loss metrics
// Score ranges 0-100: 70+ = Good, 50-70 = Fair, <50 = Poor
#define QUALITY_SCORE_THRESHOLD_DEGRADE 50 // Score below which quality degradation is triggered
#define QUALITY_SCORE_THRESHOLD_SWITCH 40  // Score below which network switch is triggered
#define RSSI_CRITICAL_THRESHOLD -85        // Critical signal strength (dBm)
#define PACKET_LOSS_THRESHOLD 10.0f        // Maximum acceptable packet loss before action (%)

#endif // CONFIG_H
