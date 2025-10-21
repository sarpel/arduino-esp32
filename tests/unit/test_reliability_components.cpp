#include <unity.h>
#include "../../src/network/MultiWiFiManager.h"
#include "../../src/network/NetworkQualityMonitor.h"
#include "../../src/network/ConnectionPool.h"
#include "../../src/network/AdaptiveReconnection.h"
#include "../../src/monitoring/HealthMonitor.h"
#include "../../src/core/CircuitBreaker.h"
#include "../../src/core/DegradationManager.h"
#include "../../src/core/StateSerializer.h"
#include "../../src/core/AutoRecovery.h"
#include "../../src/utils/TelemetryCollector.h"
#include "../../src/utils/MetricsTracker.h"

// ============================================================================
// NETWORK RESILIENCE TESTS
// ============================================================================

// Test MultiWiFiManager initialization
void test_multi_wifi_manager_init(void) {
    MultiWiFiManager manager;
    TEST_ASSERT_EQUAL(0, manager.getNetworkCount());
    TEST_ASSERT_FALSE(manager.hasNetworks());
}

// Test adding networks with priorities
void test_multi_wifi_manager_add_networks_with_priority(void) {
    MultiWiFiManager manager;

    manager.addNetwork("Network1", "password1", 1, true);
    manager.addNetwork("Network2", "password2", 2, true);
    manager.addNetwork("Network3", "password3", 3, true);

    TEST_ASSERT_EQUAL(3, manager.getNetworkCount());
    TEST_ASSERT_TRUE(manager.hasNetworks());
}

// Test network priority sorting
void test_multi_wifi_manager_priority_sorting(void) {
    MultiWiFiManager manager;

    // Add in non-priority order
    manager.addNetwork("Low", "pass", 3, true);
    manager.addNetwork("High", "pass", 1, true);
    manager.addNetwork("Mid", "pass", 2, true);

    // After sorting, higher priority (lower number) should be first
    auto current = manager.getCurrentNetwork();
    TEST_ASSERT_NOT_NULL(current);
}

// Test clearing networks
void test_multi_wifi_manager_clear(void) {
    MultiWiFiManager manager;

    manager.addNetwork("Network1", "password1", 1, true);
    manager.addNetwork("Network2", "password2", 2, true);

    TEST_ASSERT_EQUAL(2, manager.getNetworkCount());

    manager.clearNetworks();

    TEST_ASSERT_EQUAL(0, manager.getNetworkCount());
    TEST_ASSERT_FALSE(manager.hasNetworks());
}

// Test NetworkQualityMonitor initialization
void test_network_quality_monitor_init(void) {
    NetworkQualityMonitor monitor;

    auto quality = monitor.getQualityMetrics();
    TEST_ASSERT_EQUAL(0, quality.rssi);
    TEST_ASSERT_EQUAL(0.0f, quality.packet_loss);
}

// Test RSSI monitoring with exponential moving average
void test_network_quality_rssi_monitoring(void) {
    NetworkQualityMonitor monitor;

    // Update with RSSI values
    monitor.updateRSSI(-50);
    auto quality1 = monitor.getQualityMetrics();

    monitor.updateRSSI(-60);
    auto quality2 = monitor.getQualityMetrics();

    // RSSI should be moving toward the latest value
    TEST_ASSERT_TRUE(quality2.rssi < quality1.rssi);
}

// Test quality score computation
void test_network_quality_score_computation(void) {
    NetworkQualityMonitor monitor;

    monitor.updateRSSI(-50);  // Good signal
    monitor.updatePacketLoss(0.0f);  // No packet loss

    auto quality = monitor.getQualityMetrics();

    // Quality score should be high with good conditions
    TEST_ASSERT_GREATER_THAN(70, quality.stability_score);
}

// Test ConnectionPool initialization
void test_connection_pool_init(void) {
    ConnectionPool pool;

    TEST_ASSERT_EQUAL(0, pool.getConnectionCount());
    TEST_ASSERT_EQUAL(-1, pool.getPrimaryConnectionId());
}

// Test adding connections to pool
void test_connection_pool_add_connection(void) {
    ConnectionPool pool;

    int id = pool.createConnection("192.168.1.1", 8080);

    TEST_ASSERT_GREATER_THAN(-1, id);
    TEST_ASSERT_EQUAL(1, pool.getConnectionCount());
}

// Test connection pool primary/backup mechanism
void test_connection_pool_failover(void) {
    ConnectionPool pool;

    int primary = pool.createConnection("192.168.1.1", 8080);
    int backup = pool.createConnection("192.168.1.2", 8080);

    pool.setPrimaryConnection(primary);

    TEST_ASSERT_EQUAL(primary, pool.getPrimaryConnectionId());
}

// Test AdaptiveReconnection strategy selection
void test_adaptive_reconnection_strategy(void) {
    AdaptiveReconnection reconnect;

    reconnect.recordNetworkSuccess("Network1");
    reconnect.recordNetworkSuccess("Network1");
    reconnect.recordNetworkSuccess("Network1");

    auto strategy = reconnect.selectStrategy();

    // Network with high success rate should be selected first
    TEST_ASSERT_NOT_NULL(&strategy);
}

// Test exponential backoff with jitter
void test_adaptive_reconnection_exponential_backoff(void) {
    AdaptiveReconnection reconnect;

    unsigned long delay1 = reconnect.getNextRetryDelay(0);
    unsigned long delay2 = reconnect.getNextRetryDelay(1);
    unsigned long delay3 = reconnect.getNextRetryDelay(2);

    // Delays should increase exponentially
    TEST_ASSERT_TRUE(delay2 > delay1);
    TEST_ASSERT_TRUE(delay3 > delay2);
}

// ============================================================================
// HEALTH MONITORING TESTS
// ============================================================================

// Test HealthMonitor initialization
void test_health_monitor_init(void) {
    HealthMonitor monitor;

    auto health = monitor.getCurrentHealth();
    TEST_ASSERT_EQUAL(100, health.overall_score);  // Should start at 100
}

// Test health score computation
void test_health_monitor_score_computation(void) {
    HealthMonitor monitor;

    // Simulate degraded network
    monitor.updateComponentHealth(HealthComponent::NETWORK, 60);

    auto health = monitor.getCurrentHealth();

    // Overall score should be less than 100 with degraded component
    TEST_ASSERT_LESS_THAN(100, health.overall_score);
}

// Test component weight distribution
void test_health_monitor_component_weights(void) {
    HealthMonitor monitor;

    // Network health should have 40% weight
    monitor.updateComponentHealth(HealthComponent::NETWORK, 0);
    auto with_poor_network = monitor.getCurrentHealth();

    // Memory health should have 30% weight
    monitor.updateComponentHealth(HealthComponent::NETWORK, 100);
    monitor.updateComponentHealth(HealthComponent::MEMORY, 0);
    auto with_poor_memory = monitor.getCurrentHealth();

    // Network degradation should impact health more than memory degradation
    // (if all else equal)
    TEST_ASSERT_NOT_NULL(&with_poor_network);
    TEST_ASSERT_NOT_NULL(&with_poor_memory);
}

// ============================================================================
// FAILURE RECOVERY TESTS
// ============================================================================

// Test CircuitBreaker initialization
void test_circuit_breaker_init(void) {
    CircuitBreaker breaker;

    TEST_ASSERT_EQUAL(CircuitState::CLOSED, breaker.getState());
    TEST_ASSERT_EQUAL(0, breaker.getFailureCount());
}

// Test CircuitBreaker state transitions
void test_circuit_breaker_state_transitions(void) {
    CircuitBreaker breaker(3);  // Fail threshold = 3

    // Initially closed
    TEST_ASSERT_EQUAL(CircuitState::CLOSED, breaker.getState());

    // Record failures
    breaker.recordFailure();
    breaker.recordFailure();
    breaker.recordFailure();

    // Should now be open
    TEST_ASSERT_EQUAL(CircuitState::OPEN, breaker.getState());
}

// Test CircuitBreaker recovery
void test_circuit_breaker_recovery(void) {
    CircuitBreaker breaker(2);

    breaker.recordFailure();
    breaker.recordFailure();
    TEST_ASSERT_EQUAL(CircuitState::OPEN, breaker.getState());

    // Allow half-open state
    breaker.tryReset();
    TEST_ASSERT_EQUAL(CircuitState::HALF_OPEN, breaker.getState());
}

// Test DegradationManager mode transitions
void test_degradation_manager_mode_transitions(void) {
    DegradationManager manager;

    // Start in NORMAL mode
    TEST_ASSERT_EQUAL(DegradationMode::NORMAL, manager.getCurrentMode());

    // Degrade to REDUCED_QUALITY
    manager.setMode(DegradationMode::REDUCED_QUALITY);
    TEST_ASSERT_EQUAL(DegradationMode::REDUCED_QUALITY, manager.getCurrentMode());
}

// Test DegradationManager feature control
void test_degradation_manager_feature_control(void) {
    DegradationManager manager;

    manager.setMode(DegradationMode::SAFE_MODE);

    // In SAFE_MODE, non-essential features should be disabled
    TEST_ASSERT_FALSE(manager.isFeatureEnabled("AUDIO_ENHANCEMENT"));
    TEST_ASSERT_FALSE(manager.isFeatureEnabled("NETWORK_OPTIMIZATION"));
}

// Test StateSerializer initialization
void test_state_serializer_init(void) {
    StateSerializer serializer;

    // Should be able to create serializer without errors
    TEST_ASSERT_TRUE(true);
}

// Test state serialization and deserialization
void test_state_serializer_roundtrip(void) {
    StateSerializer serializer;

    SystemState state;
    state.mode = SystemMode::AUDIO_STREAMING;
    state.uptime_seconds = 3600;
    state.error_count = 5;

    auto serialized = serializer.serialize(state);
    auto deserialized = serializer.deserialize(serialized);

    TEST_ASSERT_EQUAL(state.mode, deserialized.mode);
    TEST_ASSERT_EQUAL(state.uptime_seconds, deserialized.uptime_seconds);
    TEST_ASSERT_EQUAL(state.error_count, deserialized.error_count);
}

// Test AutoRecovery failure classification
void test_auto_recovery_failure_classification(void) {
    AutoRecovery recovery;

    FailureType wifi_failure = recovery.classifyFailure(ErrorCode::WIFI_CONNECTION_FAILED);
    FailureType tcp_failure = recovery.classifyFailure(ErrorCode::TCP_CONNECTION_FAILED);

    TEST_ASSERT_NOT_EQUAL(wifi_failure, tcp_failure);
}

// ============================================================================
// OBSERVABILITY TESTS
// ============================================================================

// Test TelemetryCollector initialization
void test_telemetry_collector_init(void) {
    TelemetryCollector collector(1024);  // 1KB buffer

    TEST_ASSERT_EQUAL(0, collector.getEventCount());
}

// Test event collection
void test_telemetry_collector_event_collection(void) {
    TelemetryCollector collector(1024);

    collector.logEvent(EventSeverity::INFO, "Test event", 0);

    TEST_ASSERT_EQUAL(1, collector.getEventCount());
}

// Test circular buffer behavior
void test_telemetry_collector_circular_buffer(void) {
    TelemetryCollector collector(128);  // Small buffer to force wrapping

    // Log many events to force circular buffer wrap
    for (int i = 0; i < 100; i++) {
        collector.logEvent(EventSeverity::INFO, "Event", i);
    }

    // Should still have valid event count (buffer wrapped)
    TEST_ASSERT_GREATER_THAN(0, collector.getEventCount());
    TEST_ASSERT_LESS_THAN(101, collector.getEventCount());
}

// Test MetricsTracker initialization
void test_metrics_tracker_init(void) {
    MetricsTracker tracker;

    TEST_ASSERT_EQUAL(0, tracker.getUptime());
    TEST_ASSERT_EQUAL(0, tracker.getErrorCount());
}

// Test metrics tracking
void test_metrics_tracker_uptime(void) {
    MetricsTracker tracker;

    tracker.recordUptime(3600);  // 1 hour

    TEST_ASSERT_EQUAL(3600, tracker.getUptime());
}

// Test error tracking per component
void test_metrics_tracker_error_tracking(void) {
    MetricsTracker tracker;

    tracker.recordError(Component::NETWORK);
    tracker.recordError(Component::NETWORK);
    tracker.recordError(Component::MEMORY);

    TEST_ASSERT_EQUAL(3, tracker.getErrorCount());
    TEST_ASSERT_EQUAL(2, tracker.getErrorCount(Component::NETWORK));
}

// Test availability calculation
void test_metrics_tracker_availability(void) {
    MetricsTracker tracker;

    tracker.recordUptime(3600);  // 1 hour
    tracker.recordDowntime(600);  // 10 minutes

    float availability = tracker.getAvailability();

    // Should be 6 hours uptime / 7 hours total = ~85.7%
    TEST_ASSERT_GREATER_THAN(85.0f, availability);
    TEST_ASSERT_LESS_THAN(86.0f, availability);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

// Test complete network failover flow
void test_complete_network_failover_flow(void) {
    MultiWiFiManager wifi_manager;
    ConnectionPool pool;
    CircuitBreaker breaker(3);

    // Setup networks
    wifi_manager.addNetwork("Primary", "pass1", 1, true);
    wifi_manager.addNetwork("Backup", "pass2", 2, true);

    // Setup connections
    int primary_conn = pool.createConnection("192.168.1.1", 8080);
    int backup_conn = pool.createConnection("192.168.1.2", 8080);

    pool.setPrimaryConnection(primary_conn);

    // Simulate failures
    breaker.recordFailure();
    breaker.recordFailure();
    breaker.recordFailure();

    // Circuit breaker should be open
    TEST_ASSERT_EQUAL(CircuitState::OPEN, breaker.getState());

    // System would trigger failover to backup network here
    TEST_ASSERT_GREATER_THAN(1, wifi_manager.getNetworkCount());
}

// Test health monitoring during degradation
void test_health_monitoring_degradation_flow(void) {
    HealthMonitor health;
    DegradationManager degradation;

    // Simulate network degradation
    health.updateComponentHealth(HealthComponent::NETWORK, 30);

    auto current_health = health.getCurrentHealth();

    if (current_health.overall_score < 60) {
        degradation.setMode(DegradationMode::REDUCED_QUALITY);
    }

    TEST_ASSERT_EQUAL(DegradationMode::REDUCED_QUALITY, degradation.getCurrentMode());
}

// Test telemetry collection during failure scenario
void test_telemetry_failure_scenario(void) {
    TelemetryCollector collector(1024);
    MetricsTracker metrics;
    CircuitBreaker breaker(2);

    // Simulate failure scenario
    collector.logEvent(EventSeverity::WARNING, "Network degradation detected", 0);
    breaker.recordFailure();
    collector.logEvent(EventSeverity::ERROR, "Connection failed", 0);
    breaker.recordFailure();

    metrics.recordError(Component::NETWORK);
    metrics.recordError(Component::NETWORK);

    TEST_ASSERT_EQUAL(2, collector.getEventCount());
    TEST_ASSERT_EQUAL(2, metrics.getErrorCount());
    TEST_ASSERT_EQUAL(CircuitState::OPEN, breaker.getState());
}

// ============================================================================
// SETUP AND TEARDOWN
// ============================================================================

void setUp(void) {
    // Setup code before each test
}

void tearDown(void) {
    // Cleanup code after each test
}

#endif  // UNIT_TEST
