#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include <cstdint>

enum class EncryptionMethod {
    NONE = 0,
    XOR_SIMPLE = 1,
    AES_128_CBC = 2,
    CHACHA20 = 3
};

enum class AuthenticationMethod {
    NONE = 0,
    HMAC_SHA256 = 1,
    AES_CMAC = 2,
    CHACHA20_POLY1305 = 3
};

struct SecurityAuditLog {
    unsigned long timestamp;
    uint8_t event_type;
    uint32_t event_code;
    const char* description;
    bool severity;
    
    SecurityAuditLog() : timestamp(0), event_type(0), event_code(0), 
                        description(nullptr), severity(false) {}
};

enum class SecurityEvent {
    AUTH_SUCCESS = 0,
    AUTH_FAILURE = 1,
    ENCRYPTION_ERROR = 2,
    DECRYPTION_ERROR = 3,
    CHECKSUM_FAILURE = 4,
    UNAUTHORIZED_ACCESS = 5,
    REPLAY_ATTACK_DETECTED = 6,
    CERTIFICATE_EXPIRED = 7,
    CERTIFICATE_INVALID = 8
};

class SecurityManager {
private:
    EncryptionMethod encryption_method;
    AuthenticationMethod auth_method;
    
    std::vector<uint8_t> encryption_key;
    std::vector<uint8_t> authentication_key;
    
    std::vector<SecurityAuditLog> audit_logs;
    static constexpr size_t MAX_AUDIT_LOGS = 100;
    
    uint32_t total_auth_attempts;
    uint32_t successful_auth;
    uint32_t failed_auth;
    uint32_t encryption_errors;
    uint32_t decryption_errors;
    uint32_t checksum_failures;
    uint32_t unauthorized_attempts;
    
    bool initialized;
    bool audit_enabled;
    
    uint16_t calculateSimpleChecksum(const uint8_t* data, size_t length);
    void rotateEncryptionKey();
    void logSecurityEvent(SecurityEvent event, const char* description);
    
public:
    SecurityManager();
    ~SecurityManager();
    
    bool initialize(EncryptionMethod enc = EncryptionMethod::AES_128_CBC,
                   AuthenticationMethod auth = AuthenticationMethod::HMAC_SHA256);
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    bool setEncryptionKey(const uint8_t* key, size_t key_size);
    bool setAuthenticationKey(const uint8_t* key, size_t key_size);
    
    bool encryptData(const uint8_t* plaintext, size_t plaintext_size,
                    uint8_t* ciphertext, size_t& ciphertext_size);
    bool decryptData(const uint8_t* ciphertext, size_t ciphertext_size,
                    uint8_t* plaintext, size_t& plaintext_size);
    
    bool authenticateData(const uint8_t* data, size_t data_size,
                         const uint8_t* mac, size_t mac_size);
    bool generateMAC(const uint8_t* data, size_t data_size,
                    uint8_t* mac, size_t& mac_size);
    
    bool validateCertificate(const uint8_t* cert_data, size_t cert_size);
    
    const std::vector<SecurityAuditLog>& getAuditLogs() const { return audit_logs; }
    void clearAuditLogs();
    void enableAudit(bool enable) { audit_enabled = enable; }
    bool isAuditEnabled() const { return audit_enabled; }
    
    uint32_t getAuthAttempts() const { return total_auth_attempts; }
    uint32_t getSuccessfulAuth() const { return successful_auth; }
    uint32_t getFailedAuth() const { return failed_auth; }
    uint32_t getEncryptionErrors() const { return encryption_errors; }
    uint32_t getDecryptionErrors() const { return decryption_errors; }
    uint32_t getChecksumFailures() const { return checksum_failures; }
    uint32_t getUnauthorizedAttempts() const { return unauthorized_attempts; }
    
    float getAuthSuccessRate() const;
    void printSecurityStatus() const;
    void printAuditLog() const;
};

#endif
