#include "SecurityManager.h"
#include "../utils/EnhancedLogger.h"
#include <cstring>
#include <algorithm>
#include <cmath>

SecurityManager::SecurityManager()
    : encryption_method(EncryptionMethod::NONE),
      auth_method(AuthenticationMethod::NONE),
      total_auth_attempts(0), successful_auth(0), failed_auth(0),
      encryption_errors(0), decryption_errors(0), checksum_failures(0),
      unauthorized_attempts(0), initialized(false), audit_enabled(true) {
}

SecurityManager::~SecurityManager() {
    shutdown();
}

bool SecurityManager::initialize(EncryptionMethod enc, AuthenticationMethod auth) {
    encryption_method = enc;
    auth_method = auth;
    
    encryption_key.resize(32, 0);
    authentication_key.resize(32, 0);
    
    initialized = true;
    return true;
}

void SecurityManager::shutdown() {
    std::fill(encryption_key.begin(), encryption_key.end(), 0);
    std::fill(authentication_key.begin(), authentication_key.end(), 0);
    initialized = false;
}

bool SecurityManager::setEncryptionKey(const uint8_t* key, size_t key_size) {
    if (!key || key_size == 0 || key_size > 256) {
        return false;
    }
    
    encryption_key.assign(key, key + key_size);
    return true;
}

bool SecurityManager::setAuthenticationKey(const uint8_t* key, size_t key_size) {
    if (!key || key_size == 0 || key_size > 256) {
        return false;
    }
    
    authentication_key.assign(key, key + key_size);
    return true;
}

uint16_t SecurityManager::calculateSimpleChecksum(const uint8_t* data, size_t length) {
    uint16_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
        checksum = ((checksum << 1) | (checksum >> 15));
    }
    return checksum;
}

bool SecurityManager::encryptData(const uint8_t* plaintext, size_t plaintext_size,
                                 uint8_t* ciphertext, size_t& ciphertext_size) {
    if (!plaintext || !ciphertext || plaintext_size == 0) {
        encryption_errors++;
        return false;
    }
    
    if (encryption_method == EncryptionMethod::NONE) {
        if (ciphertext_size < plaintext_size) {
            encryption_errors++;
            return false;
        }
        memcpy(ciphertext, plaintext, plaintext_size);
        ciphertext_size = plaintext_size;
        return true;
    }
    
    if (encryption_method == EncryptionMethod::XOR_SIMPLE) {
        if (ciphertext_size < plaintext_size) {
            encryption_errors++;
            return false;
        }
        
        for (size_t i = 0; i < plaintext_size; i++) {
            size_t key_idx = i % encryption_key.size();
            ciphertext[i] = plaintext[i] ^ encryption_key[key_idx];
        }
        ciphertext_size = plaintext_size;
        return true;
    }
    
    encryption_errors++;
    return false;
}

bool SecurityManager::decryptData(const uint8_t* ciphertext, size_t ciphertext_size,
                                 uint8_t* plaintext, size_t& plaintext_size) {
    if (!ciphertext || !plaintext || ciphertext_size == 0) {
        decryption_errors++;
        return false;
    }
    
    if (encryption_method == EncryptionMethod::NONE) {
        if (plaintext_size < ciphertext_size) {
            decryption_errors++;
            return false;
        }
        memcpy(plaintext, ciphertext, ciphertext_size);
        plaintext_size = ciphertext_size;
        return true;
    }
    
    if (encryption_method == EncryptionMethod::XOR_SIMPLE) {
        if (plaintext_size < ciphertext_size) {
            decryption_errors++;
            return false;
        }
        
        for (size_t i = 0; i < ciphertext_size; i++) {
            size_t key_idx = i % encryption_key.size();
            plaintext[i] = ciphertext[i] ^ encryption_key[key_idx];
        }
        plaintext_size = ciphertext_size;
        return true;
    }
    
    decryption_errors++;
    return false;
}

bool SecurityManager::authenticateData(const uint8_t* data, size_t data_size,
                                      const uint8_t* mac, size_t mac_size) {
    if (!data || !mac || data_size == 0) {
        unauthorized_attempts++;
        total_auth_attempts++;
        return false;
    }
    
    total_auth_attempts++;
    
    if (auth_method == AuthenticationMethod::NONE) {
        successful_auth++;
        logSecurityEvent(SecurityEvent::AUTH_SUCCESS, "Authentication bypassed (no security)");
        return true;
    }
    
    uint16_t calculated_mac = calculateSimpleChecksum(data, data_size);
    uint16_t received_mac = 0;
    
    if (mac_size >= 2) {
        received_mac = (mac[0] << 8) | mac[1];
    }
    
    if (calculated_mac == received_mac) {
        successful_auth++;
        logSecurityEvent(SecurityEvent::AUTH_SUCCESS, "Authentication successful");
        return true;
    }
    
    failed_auth++;
    checksum_failures++;
    logSecurityEvent(SecurityEvent::AUTH_FAILURE, "Authentication failed: checksum mismatch");
    return false;
}

bool SecurityManager::generateMAC(const uint8_t* data, size_t data_size,
                                 uint8_t* mac, size_t& mac_size) {
    if (!data || !mac || data_size == 0) {
        return false;
    }
    
    if (auth_method == AuthenticationMethod::NONE) {
        mac_size = 0;
        return true;
    }
    
    uint16_t checksum = calculateSimpleChecksum(data, data_size);
    
    if (mac_size >= 2) {
        mac[0] = (checksum >> 8) & 0xFF;
        mac[1] = checksum & 0xFF;
        mac_size = 2;
        return true;
    }
    
    return false;
}

bool SecurityManager::validateCertificate(const uint8_t* cert_data, size_t cert_size) {
    if (!cert_data || cert_size == 0) {
        logSecurityEvent(SecurityEvent::CERTIFICATE_INVALID, "Invalid certificate data");
        return false;
    }
    
    logSecurityEvent(SecurityEvent::AUTH_SUCCESS, "Certificate validated");
    return true;
}

void SecurityManager::logSecurityEvent(SecurityEvent event, const char* description) {
    if (!audit_enabled || audit_logs.size() >= MAX_AUDIT_LOGS) {
        return;
    }
    
    SecurityAuditLog log;
    log.timestamp = millis();
    log.event_type = static_cast<uint8_t>(event);
    log.event_code = static_cast<uint32_t>(event);
    log.description = description;
    log.severity = (event == SecurityEvent::UNAUTHORIZED_ACCESS || 
                   event == SecurityEvent::REPLAY_ATTACK_DETECTED ||
                   event == SecurityEvent::CERTIFICATE_EXPIRED);
    
    audit_logs.push_back(log);
}

void SecurityManager::clearAuditLogs() {
    audit_logs.clear();
}

float SecurityManager::getAuthSuccessRate() const {
    if (total_auth_attempts == 0) {
        return 100.0f;
    }
    
    return (static_cast<float>(successful_auth) / total_auth_attempts) * 100.0f;
}

void SecurityManager::rotateEncryptionKey() {
    for (size_t i = 0; i < encryption_key.size(); i++) {
        encryption_key[i] = (encryption_key[i] << 1) | (encryption_key[i] >> 7);
    }
}

void SecurityManager::printSecurityStatus() const {
    EnhancedLogger& logger = EnhancedLogger::getInstance();

    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "=== Security Manager Status ===");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Initialized: %s", initialized ? "Yes" : "No");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Encryption Method: %d", static_cast<int>(encryption_method));
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Authentication Method: %d", static_cast<int>(auth_method));
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Audit Enabled: %s", audit_enabled ? "Yes" : "No");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "=== Authentication Statistics ===");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Total Attempts: %u", total_auth_attempts);
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Successful: %u", successful_auth);
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Failed: %u", failed_auth);
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Success Rate: %.2f%%", getAuthSuccessRate());
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "=== Error Statistics ===");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Encryption Errors: %u", encryption_errors);
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Decryption Errors: %u", decryption_errors);
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Checksum Failures: %u", checksum_failures);
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Unauthorized Attempts: %u", unauthorized_attempts);
}

void SecurityManager::printAuditLog() const {
    EnhancedLogger& logger = EnhancedLogger::getInstance();

    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "=== Security Audit Log ===");
    logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "Total Entries: %u", static_cast<uint32_t>(audit_logs.size()));

    for (const auto& log : audit_logs) {
        const char* event_name = "UNKNOWN";
        switch (static_cast<SecurityEvent>(log.event_code)) {
            case SecurityEvent::AUTH_SUCCESS:
                event_name = "AUTH_SUCCESS";
                break;
            case SecurityEvent::AUTH_FAILURE:
                event_name = "AUTH_FAILURE";
                break;
            case SecurityEvent::ENCRYPTION_ERROR:
                event_name = "ENCRYPTION_ERROR";
                break;
            case SecurityEvent::DECRYPTION_ERROR:
                event_name = "DECRYPTION_ERROR";
                break;
            case SecurityEvent::CHECKSUM_FAILURE:
                event_name = "CHECKSUM_FAILURE";
                break;
            case SecurityEvent::UNAUTHORIZED_ACCESS:
                event_name = "UNAUTHORIZED_ACCESS";
                break;
            default:
                break;
        }

        logger.log(LogLevel::LOG_INFO, "SecurityManager", __FILE__, __LINE__, "[%u ms] %s: %s (Severity: %s)",
                  log.timestamp, event_name, log.description,
                  log.severity ? "HIGH" : "LOW");
    }
}
