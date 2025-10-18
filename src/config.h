#ifndef CONFIG_H
#define CONFIG_H

// ===== WiFi Configuration =====
#define WIFI_SSID           "Sarpel_2.4GHz"
#define WIFI_PASSWORD       "penguen1988"
#define WIFI_RETRY_DELAY    500      // milliseconds
#define WIFI_MAX_RETRIES    20
#define WIFI_TIMEOUT        30000    // milliseconds

// ===== WiFi Static IP (Optional) =====
// Uncomment to use static IP instead of DHCP
// #define USE_STATIC_IP
#define STATIC_IP           192, 168, 1, 100
#define GATEWAY_IP          192, 168, 1, 1
#define SUBNET_MASK         255, 255, 255, 0
#define DNS_IP              192, 168, 1, 1

// ===== Server Configuration =====
#define SERVER_HOST         "192.168.1.50"
#define SERVER_PORT         9000
#define SERVER_RECONNECT_MIN   5000   // milliseconds
#define SERVER_RECONNECT_MAX   60000  // milliseconds
#define TCP_WRITE_TIMEOUT   5000      // milliseconds

// ===== I2S Hardware Pins =====
#define I2S_WS_PIN          15
#define I2S_SD_PIN          32
#define I2S_SCK_PIN         14

// ===== I2S Parameters =====
#define I2S_PORT            I2S_NUM_0
#define I2S_SAMPLE_RATE     16000
#define I2S_BUFFER_SIZE     4096
#define I2S_DMA_BUF_COUNT   8
#define I2S_DMA_BUF_LEN     256

// ===== Reliability Thresholds =====
#define MEMORY_WARN_THRESHOLD    40000  // bytes
#define MEMORY_CRITICAL_THRESHOLD 20000 // bytes
#define RSSI_WEAK_THRESHOLD      -80    // dBm
#define MAX_CONSECUTIVE_FAILURES  10
#define I2S_MAX_READ_RETRIES     3

// ===== Timing Configuration =====
#define MEMORY_CHECK_INTERVAL    60000  // 1 minute
#define RSSI_CHECK_INTERVAL      10000  // 10 seconds
#define STATS_PRINT_INTERVAL     300000 // 5 minutes

#endif // CONFIG_H
