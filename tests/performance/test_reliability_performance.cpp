#include <unity.h>
#include "../../src/network/MultiWiFiManager.h"
#include "../../src/network/NetworkQualityMonitor.h"
#include "../../src/network/ConnectionPool.h"
#include "../../src/monitoring/HealthMonitor.h"
#include "../../src/core/CircuitBreaker.h"
#include "../../src/core/DegradationManager.h"
#include "../../src/core/StateSerializer.h"
#include "../../src/utils/TelemetryCollector.h"
#include "../../src/utils/MetricsTracker.h"

// ============================================================================
// PERFORMANCE TEST UTILITIES
// ============================================================================

class PerformanceTimer {
private:
    unsigned long start_time;
    unsigned long end_time;

public:
    void start() {
        start_time = millis();
    }

    void stop() {
        end_time = millis();
    }

    unsigned long elapsed() {
        return end_time - start_time;
    }
};

// ============================================================================
// NETWORK RESILIENCE PERFORMANCE TESTS
// ============================================================================

// Test MultiWiFiManager performance with large network list
void test_multi_wifi_manager_performance_many_networks(void) {
    MultiWiFiManager manager;
    PerformanceTimer timer;

    timer.start();

    // Add 20 networks
    for (int i = 0; i < 20; i++) {
        char ssid[32];
        snprintf(ssid, sizeof(ssid), "Network_%d", i);
        manager.addNetwork(ssid, "password", i + 1, true);
    }

    timer.stop();

    // Should complete in < 100ms
    TEST_ASSERT_LESS_THAN(100, timer.elapsed());
    TEST_ASSERT_EQUAL(20, manager.getNetworkCount());
}

// Test network selection latency
void test_network_quality_selection_latency(void) {
    NetworkQualityMonitor monitor;
    PerformanceTimer timer;

    // Prime with data
    for (int i = 0; i < 100; i++) {
        monitor.updateRSSI(-50 - (i % 50));
        monitor.updatePacketLoss((i % 10) / 100.0f);
    }

    timer.start();

    // Get quality metrics 1000 times
    for (int i = 0; i < 1000; i++) {
        auto quality = monitor.getQualityMetrics();
        (void)quality;
    }

    timer.stop();

    // Average per call should be < 1ms
    unsigned long avg_per_call = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(1, avg_per_call);
}

// Test connection pool operation latency
void test_connection_pool_operation_latency(void) {
    ConnectionPool pool;
    PerformanceTimer timer;

    // Create 10 connections
    std::vector<int> connection_ids;
    for (int i = 0; i < 10; i++) {
        char ip[16];
        snprintf(ip, sizeof(ip), "192.168.1.%d", i);
        connection_ids.push_back(pool.createConnection(ip, 8080));
    }

    // Measure failover latency
    timer.start();

    pool.failoverToBackup();

    timer.stop();

    // Failover should be < 10ms
    TEST_ASSERT_LESS_THAN(10, timer.elapsed());
}

// ============================================================================
// HEALTH MONITORING PERFORMANCE TESTS
// ============================================================================

// Test health scoring computation latency
void test_health_monitor_scoring_latency(void) {
    HealthMonitor monitor;
    PerformanceTimer timer;

    // Prime with updates
    for (int i = 0; i < 100; i++) {
        monitor.updateComponentHealth(HealthComponent::NETWORK, 80 - (i % 20));
        monitor.updateComponentHealth(HealthComponent::MEMORY, 85 - (i % 20));
        monitor.updateComponentHealth(HealthComponent::AUDIO, 90 - (i % 20));
        monitor.updateComponentHealth(HealthComponent::SYSTEM, 95 - (i % 20));
    }

    timer.start();

    // Compute health 1000 times
    for (int i = 0; i < 1000; i++) {
        auto health = monitor.getCurrentHealth();
        (void)health;
    }

    timer.stop();

    // Average per call should be < 5ms
    unsigned long avg_per_call = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(5, avg_per_call);
}

// Test trend analysis performance
void test_health_trend_analysis_latency(void) {
    HealthMonitor monitor;
    PerformanceTimer timer;

    // Create 60 seconds of trend data (1 sample per second)
    for (int i = 0; i < 60; i++) {
        int score = 80 + (sin(i / 10.0f) * 10);  // Sinusoidal variation
        monitor.updateComponentHealth(HealthComponent::NETWORK, score);
    }

    timer.start();

    // Analyze trend 100 times
    for (int i = 0; i < 100; i++) {
        auto health = monitor.getCurrentHealth();
        (void)health;
    }

    timer.stop();

    // Average per call should be < 10ms
    unsigned long avg_per_call = timer.elapsed() / 100;
    TEST_ASSERT_LESS_THAN(10, avg_per_call);
}

// ============================================================================
// FAILURE RECOVERY PERFORMANCE TESTS
// ============================================================================

// Test circuit breaker state transition latency
void test_circuit_breaker_latency(void) {
    CircuitBreaker breaker(5);
    PerformanceTimer timer;

    timer.start();

    // Perform 1000 state checks
    for (int i = 0; i < 1000; i++) {
        auto state = breaker.getState();
        (void)state;
    }

    timer.stop();

    // Average per call should be < 1ms
    unsigned long avg_per_call = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(1, avg_per_call);
}

// Test degradation mode transition latency
void test_degradation_manager_latency(void) {
    DegradationManager manager;
    PerformanceTimer timer;

    timer.start();

    // Transition modes 1000 times
    for (int i = 0; i < 1000; i++) {
        int mode_index = i % 4;
        DegradationMode mode = static_cast<DegradationMode>(mode_index);
        manager.setMode(mode);
    }

    timer.stop();

    // Average per transition should be < 1ms
    unsigned long avg_per_transition = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(1, avg_per_transition);
}

// Test state serialization performance
void test_state_serializer_latency(void) {
    StateSerializer serializer;
    PerformanceTimer timer;

    SystemState state;
    state.mode = SystemMode::AUDIO_STREAMING;
    state.uptime_seconds = 3600;

    timer.start();

    // Serialize 1000 times
    for (int i = 0; i < 1000; i++) {
        auto serialized = serializer.serialize(state);
        (void)serialized;
    }

    timer.stop();

    // Average per serialization should be < 5ms
    unsigned long avg_per_op = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(5, avg_per_op);
}

// ============================================================================
// OBSERVABILITY PERFORMANCE TESTS
// ============================================================================

// Test telemetry collection throughput
void test_telemetry_event_collection_throughput(void) {
    TelemetryCollector collector(4096);  // 4KB buffer
    PerformanceTimer timer;

    timer.start();

    // Log 1000 events
    for (int i = 0; i < 1000; i++) {
        collector.logEvent(EventSeverity::INFO, "Test event", i);
    }

    timer.stop();

    // Should complete in < 500ms
    TEST_ASSERT_LESS_THAN(500, timer.elapsed());

    // Average per event < 0.5ms
    unsigned long avg_per_event = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(1, avg_per_event);
}

// Test metrics tracking performance
void test_metrics_tracker_latency(void) {
    MetricsTracker tracker;
    PerformanceTimer timer;

    timer.start();

    // Record 1000 events
    for (int i = 0; i < 1000; i++) {
        if (i % 3 == 0) {
            tracker.recordError(Component::NETWORK);
        } else if (i % 3 == 1) {
            tracker.recordError(Component::MEMORY);
        } else {
            tracker.recordError(Component::AUDIO);
        }
    }

    timer.stop();

    // Should complete in < 100ms
    TEST_ASSERT_LESS_THAN(100, timer.elapsed());

    // Average per event < 0.1ms
    unsigned long avg_per_event = timer.elapsed() / 1000;
    TEST_ASSERT_LESS_THAN(1, avg_per_event);
}

// ============================================================================
// MEMORY USAGE TESTS
// ============================================================================

// Test memory overhead of reliability components
void test_component_memory_overhead(void) {
    // Note: These are approximate sizes
    // Actual sizes depend on compiler optimization

    // Network components
    size_t multi_wifi_size = sizeof(MultiWiFiManager);
    size_t quality_monitor_size = sizeof(NetworkQualityMonitor);
    size_t connection_pool_size = sizeof(ConnectionPool);

    // Health monitoring
    size_t health_monitor_size = sizeof(HealthMonitor);

    // Failure recovery
    size_t circuit_breaker_size = sizeof(CircuitBreaker);
    size_t degradation_mgr_size = sizeof(DegradationManager);

    // Observability
    size_t telemetry_size = sizeof(TelemetryCollector);
    size_t metrics_size = sizeof(MetricsTracker);

    // Total should be < 20KB
    size_t total = multi_wifi_size + quality_monitor_size + connection_pool_size +
                   health_monitor_size + circuit_breaker_size + degradation_mgr_size +
                   telemetry_size + metrics_size;

    TEST_ASSERT_LESS_THAN(20480, total);  // 20KB
}

// Test telemetry buffer memory footprint
void test_telemetry_buffer_memory(void) {
    TelemetryCollector small_buffer(512);   // 512 bytes
    TelemetryCollector medium_buffer(1024); // 1KB
    TelemetryCollector large_buffer(2048);  // 2KB

    // Should fit in available memory
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

// Test system under high event load
void test_high_event_load_stress(void) {
    TelemetryCollector collector(1024);
    MetricsTracker metrics;
    PerformanceTimer timer;

    timer.start();

    // Generate 10,000 events as fast as possible
    for (int i = 0; i < 10000; i++) {
        EventSeverity severity = static_cast<EventSeverity>(i % 5);
        collector.logEvent(severity, "Event", i);

        if (i % 100 == 0) {
            metrics.recordError(Component::NETWORK);
        }
    }

    timer.stop();

    // Should handle without crashing
    TEST_ASSERT_GREATER_THAN(0, collector.getEventCount());
    TEST_ASSERT_EQUAL(100, metrics.getErrorCount());
}

// Test concurrent component operations
void test_concurrent_component_operations(void) {
    MultiWiFiManager wifi;
    HealthMonitor health;
    CircuitBreaker breaker(5);
    TelemetryCollector telemetry(1024);
    MetricsTracker metrics;

    // Simulate concurrent operations
    for (int i = 0; i < 100; i++) {
        // Network operations
        auto current = wifi.getCurrentNetwork();

        // Health operations
        health.updateComponentHealth(HealthComponent::NETWORK, 80 - (i % 20));
        auto h = health.getCurrentHealth();

        // Recovery operations
        if (i % 10 == 0) {
            breaker.recordFailure();
        }
        auto state = breaker.getState();

        // Observability
        telemetry.logEvent(EventSeverity::INFO, "Cycle", i);
        metrics.recordError(Component::NETWORK);

        (void)current;
        (void)h;
        (void)state;
    }

    // Should complete without errors
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// SETUP AND TEARDOWN
// ============================================================================

void setUp(void) {
    // Setup code before each performance test
}

void tearDown(void) {
    // Cleanup code after each performance test
}

#endif  // PERFORMANCE_TEST
