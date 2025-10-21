#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include <WiFi.h>

enum class ConnectionState {
    IDLE = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    DISCONNECTING = 3,
    FAILED = 4,
    BACKUP = 5
};

struct PooledConnection {
    uint8_t id;
    WiFiClient client;
    ConnectionState state;
    IPAddress server_ip;
    uint16_t server_port;
    unsigned long connection_time;
    unsigned long last_activity_time;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t error_count;
    
    PooledConnection() : id(0), state(ConnectionState::IDLE), 
                        server_port(0), connection_time(0),
                        last_activity_time(0), bytes_sent(0),
                        bytes_received(0), error_count(0) {}
};

class ConnectionPool {
private:
    static constexpr uint8_t MAX_CONNECTIONS = 3;
    std::vector<std::unique_ptr<PooledConnection>> connections;
    
    uint8_t primary_connection_id;
    uint8_t backup_connection_id;
    
    unsigned long connection_timeout;
    unsigned long idle_timeout;
    unsigned long health_check_interval;
    unsigned long last_health_check;
    
    uint32_t total_reconnects;
    uint32_t failovers;
    
    void updateConnectionHealth();
    bool isConnectionHealthy(PooledConnection& conn);
    
public:
    ConnectionPool();
    
    bool initialize();
    void shutdown();
    
    bool addConnection(const IPAddress& server_ip, uint16_t port);
    bool removeConnection(uint8_t connection_id);
    void closeAllConnections();
    
    bool connectPrimary();
    bool connectBackup();
    bool failoverToBackup();
    bool reconnectIfNeeded();
    
    PooledConnection* getPrimaryConnection();
    PooledConnection* getBackupConnection();
    PooledConnection* getHealthiestConnection();
    PooledConnection* getConnection(uint8_t id);
    
    bool sendData(uint8_t connection_id, const uint8_t* data, size_t length);
    bool sendDataPrimary(const uint8_t* data, size_t length);
    
    bool receiveData(uint8_t connection_id, uint8_t* buffer, size_t buffer_size, size_t& bytes_read);
    
    void updateStatistics(uint8_t connection_id, size_t bytes_sent, size_t bytes_received, bool error = false);
    
    ConnectionState getConnectionState(uint8_t connection_id);
    bool isConnectionActive(uint8_t connection_id);
    uint8_t getActiveConnectionCount() const;
    
    uint32_t getTotalReconnects() const { return total_reconnects; }
    uint32_t getFailoverCount() const { return failovers; }
    
    void setConnectionTimeout(unsigned long timeout_ms) { connection_timeout = timeout_ms; }
    void setIdleTimeout(unsigned long timeout_ms) { idle_timeout = timeout_ms; }
    void setHealthCheckInterval(unsigned long interval_ms) { health_check_interval = interval_ms; }
    
    void performHealthCheck();
    void printPoolStatus() const;
};

#endif
