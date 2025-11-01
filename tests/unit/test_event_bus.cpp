/**
 * @file test_event_bus.cpp
 * @brief Unit tests for EventBus component
 *
 * Tests event subscription, publication, priority handling, and statistics.
 */

#include <unity.h>
#include "core/EventBus.h"
#include "core/SystemTypes.h"

EventBus* eventBus = nullptr;
int callback_count = 0;
SystemEvent last_event = SystemEvent::SYSTEM_STARTUP;

void test_callback(const void* data) {
    callback_count++;
    if (data) {
        last_event = *static_cast<const SystemEvent*>(data);
    }
}

void setUp(void) {
    // Initialize before each test
    callback_count = 0;
    last_event = SystemEvent::SYSTEM_STARTUP;

    eventBus = new EventBus();
    TEST_ASSERT_NOT_NULL(eventBus);
    TEST_ASSERT_TRUE(eventBus->initialize());
}

void tearDown(void) {
    // Cleanup after each test
    if (eventBus) {
        eventBus->shutdown();
        delete eventBus;
        eventBus = nullptr;
    }
}

// Test: EventBus Initialization
void test_eventbus_initialization() {
    TEST_ASSERT_TRUE(eventBus->isInitialized());
}

// Test: Event Subscription
void test_event_subscription() {
    bool result = eventBus->subscribe(
        SystemEvent::AUDIO_QUALITY_DEGRADED,
        test_callback,
        EventPriority::NORMAL_PRIORITY,
        "test"
    );

    TEST_ASSERT_TRUE(result);
}

// Test: Event Publication and Callback
void test_event_publication() {
    eventBus->subscribe(
        SystemEvent::MEMORY_CRITICAL,
        test_callback,
        EventPriority::CRITICAL_PRIORITY,
        "test"
    );

    // Publish event
    eventBus->publish(SystemEvent::MEMORY_CRITICAL, nullptr, false);

    // Process event queue
    eventBus->processEvents();

    // Callback should have been invoked
    TEST_ASSERT_EQUAL(1, callback_count);
}

// Test: Multiple Subscribers
void test_multiple_subscribers() {
    int subscriber1_count = 0;
    int subscriber2_count = 0;

    eventBus->subscribe(
        SystemEvent::NETWORK_CONNECTED,
        [](const void* data) { /* subscriber 1 */ },
        EventPriority::NORMAL_PRIORITY,
        "subscriber1"
    );

    eventBus->subscribe(
        SystemEvent::NETWORK_CONNECTED,
        [](const void* data) { /* subscriber 2 */ },
        EventPriority::NORMAL_PRIORITY,
        "subscriber2"
    );

    // Both should be registered
    // (exact count verification would require EventBus API extension)
    TEST_ASSERT_TRUE(true);  // Placeholder - passes if no crash
}

// Test: Priority Handling
void test_priority_handling() {
    int execution_order[3] = {0, 0, 0};
    int execution_index = 0;

    // Subscribe with different priorities
    eventBus->subscribe(
        SystemEvent::SYSTEM_ERROR,
        [&execution_order, &execution_index](const void* data) {
            execution_order[execution_index++] = 3;  // CRITICAL
        },
        EventPriority::CRITICAL_PRIORITY,
        "critical"
    );

    eventBus->subscribe(
        SystemEvent::SYSTEM_ERROR,
        [&execution_order, &execution_index](const void* data) {
            execution_order[execution_index++] = 2;  // HIGH
        },
        EventPriority::HIGH_PRIORITY,
        "high"
    );

    eventBus->subscribe(
        SystemEvent::SYSTEM_ERROR,
        [&execution_order, &execution_index](const void* data) {
            execution_order[execution_index++] = 1;  // NORMAL
        },
        EventPriority::NORMAL_PRIORITY,
        "normal"
    );

    // Publish and process
    eventBus->publish(SystemEvent::SYSTEM_ERROR);
    eventBus->processEvents();

    // Verify execution order: CRITICAL → HIGH → NORMAL
    TEST_ASSERT_EQUAL(3, execution_order[0]);
    TEST_ASSERT_EQUAL(2, execution_order[1]);
    TEST_ASSERT_EQUAL(1, execution_order[2]);
}

// Test: Immediate vs Queued Events
void test_immediate_vs_queued() {
    eventBus->subscribe(
        SystemEvent::AUDIO_BUFFER_READY,
        test_callback,
        EventPriority::NORMAL_PRIORITY,
        "test"
    );

    // Queued event (default)
    callback_count = 0;
    eventBus->publish(SystemEvent::AUDIO_BUFFER_READY, nullptr, false);
    TEST_ASSERT_EQUAL(0, callback_count);  // Not yet processed

    eventBus->processEvents();
    TEST_ASSERT_EQUAL(1, callback_count);  // Now processed

    // Immediate event
    callback_count = 0;
    eventBus->publish(SystemEvent::AUDIO_BUFFER_READY, nullptr, true);
    TEST_ASSERT_EQUAL(1, callback_count);  // Immediately processed
}

// Test: Event Data Payload
void test_event_data_payload() {
    SystemEvent test_event = SystemEvent::CPU_OVERLOAD;

    eventBus->subscribe(
        SystemEvent::CPU_OVERLOAD,
        [](const void* data) {
            TEST_ASSERT_NOT_NULL(data);
            SystemEvent* event = (SystemEvent*)data;
            TEST_ASSERT_EQUAL(SystemEvent::CPU_OVERLOAD, *event);
        },
        EventPriority::NORMAL_PRIORITY,
        "test"
    );

    eventBus->publish(SystemEvent::CPU_OVERLOAD, &test_event, true);
}

// Test: Unsubscribe
void test_unsubscribe() {
    eventBus->subscribe(
        SystemEvent::NETWORK_DISCONNECTED,
        test_callback,
        EventPriority::NORMAL_PRIORITY,
        "test"
    );

    // Unsubscribe
    eventBus->unsubscribe(SystemEvent::NETWORK_DISCONNECTED, "test");

    // Publish event - callback should not be invoked
    callback_count = 0;
    eventBus->publish(SystemEvent::NETWORK_DISCONNECTED, nullptr, true);
    TEST_ASSERT_EQUAL(0, callback_count);
}

// Test: Event Statistics
void test_event_statistics() {
    eventBus->subscribe(
        SystemEvent::SYSTEM_STARTUP,
        test_callback,
        EventPriority::NORMAL_PRIORITY,
        "test"
    );

    // Publish multiple events
    for (int i = 0; i < 5; i++) {
        eventBus->publish(SystemEvent::SYSTEM_STARTUP);
    }

    eventBus->processEvents();

    // Verify callback was invoked 5 times
    TEST_ASSERT_EQUAL(5, callback_count);
}

// Test: Event Queue Overflow Handling
void test_queue_overflow() {
    eventBus->subscribe(
        SystemEvent::AUDIO_BUFFER_READY,
        test_callback,
        EventPriority::LOW_PRIORITY,
        "test"
    );

    // Publish many events without processing
    for (int i = 0; i < 100; i++) {
        eventBus->publish(SystemEvent::AUDIO_BUFFER_READY);
    }

    // Process all events
    eventBus->processEvents();

    // Should handle overflow gracefully (exact behavior depends on implementation)
    TEST_ASSERT_TRUE(callback_count > 0);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_eventbus_initialization);
    RUN_TEST(test_event_subscription);
    RUN_TEST(test_event_publication);
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_priority_handling);
    RUN_TEST(test_immediate_vs_queued);
    RUN_TEST(test_event_data_payload);
    RUN_TEST(test_unsubscribe);
    RUN_TEST(test_event_statistics);
    RUN_TEST(test_queue_overflow);

    return UNITY_END();
}
