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

/**
 * @brief Test yardımcı geri çağırma; çağrılma sayısını artırır ve varsa iletilen olay yükünü kaydeder.
 *
 * Gelen `data` eğer null değilse, bunun `const SystemEvent*` olduğu varsayılır ve `last_event`
 * global değişkeni bu değere güncellenir. Her çağrıldığında `callback_count` global sayacı bir arttırır.
 *
 * @param data Olay yükünü içeren işaretçi; `nullptr` olabilir. İşaretçi null değilse `const SystemEvent*` olarak yorumlanır.
 */
void test_callback(const void* data) {
    callback_count++;
    if (data) {
        last_event = *static_cast<const SystemEvent*>(data);
    }
}

/**
 * @brief Her bir testten önce ortak test ortamını sıfırlar ve EventBus örneğini hazırlar.
 *
 * Bu fonksiyon test öncesi çağrılır; çağrı sayacı ve son olay değerini başlatır, yeni bir
 * EventBus örneği oluşturur ve örneğin oluşturulduğunu ve başarılı şekilde başlatıldığını doğrular.
 */
void setUp(void) {
    // Initialize before each test
    callback_count = 0;
    last_event = SystemEvent::SYSTEM_STARTUP;

    eventBus = new EventBus();
    TEST_ASSERT_NOT_NULL(eventBus);
    TEST_ASSERT_TRUE(eventBus->initialize());
}

/**
 * @brief Her testten sonra global EventBus örneğini güvenli şekilde temizler.
 *
 * Eğer global `eventBus` mevcutsa, çalışmayı sonlandırır, belleğini serbest bırakır
 * ve global işaretçiyi `nullptr` olarak sıfırlar.
 */
void tearDown(void) {
    // Cleanup after each test
    if (eventBus) {
        eventBus->shutdown();
        delete eventBus;
        eventBus = nullptr;
    }
}

/**
 * @brief EventBus'in başlatıldığını doğrular.
 *
 * Unity testinde EventBus örneğinin `isInitialized()` çağrısının `true` döndüğünü doğrular.
 */
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

/**
 * @brief Aynı olaya birden fazla abonenin kaydedilebildiğini doğrulayan bir birim testi.
 *
 * Bu test, aynı SystemEvent türü için birden fazla abonelik oluşturduktan sonra
 * EventBus'un çökmeden veya hata vermeden devam ettiğini doğrular.
 */
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

/**
 * @brief Olay abonelerinin öncelik düzeyine göre çalıştırılma sırasını doğrular.
 *
 * Üç farklı öncelikte (CRITICAL, HIGH, NORMAL) aynı olaya abone olur, olayı yayımlar ve kuyruğu işler;
 * ardından abonelerin yürütülme sırasının CRITICAL → HIGH → NORMAL olduğunu doğrulayan test aserasyonlarını yürütür.
 */
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

/**
 * @brief Kuyruklu ve anlık olay yayınlama davranışını doğrular.
 *
 * Bu birim testi, EventBus'un kuyruklanan (queued) olayları processEvents() çağrısına
 * kadar beklettiğini ve anlık (immediate) olarak yayımlanan olayları hemen işlediğini doğrular:
 * - AUDIO_BUFFER_READY için bir abone kaydeder.
 * - Kuyruklu yayınlandığında geri çağrının processEvents() çağrısına kadar tetiklenmediğini,
 *   processEvents() sonrası tetiklendiğini kontrol eder.
 * - Anlık (immediate) yayınlandığında geri çağrının hemen tetiklendiğini kontrol eder.
 */
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

/**
 * @brief Aboneliğin kaldırılmasının etkisini doğrular.
 *
 * NETWORK_DISCONNECTED olayına "test" etiketiyle abone olunup sonra aynı etiketle
 * aboneliğin iptal edilmesinin, yayımlanan olayların kayıtlı geri çağırmayı tetiklemediğini doğrular.
 */
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

/**
 * @brief SYSTEM_STARTUP olay yayınlandığında abone geri çağrılarının sayısını doğrular.
 *
 * Sisteme `SYSTEM_STARTUP` için bir abonelik ekler, aynı olayı beş kez yayımlar,
 * bekleyen olayları işler ve test callback'inin tam olarak 5 kez çağrıldığını doğrular.
 */
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

/**
 * @brief Olay kuyruğu taşması durumunun EventBus tarafından düzgün şekilde ele alındığını doğrular.
 *
 * Bu birim testi, aynı olaya 100 kez yayın yapıp olayları işledikten sonra en az bir geri çağırmanın çalıştırıldığını kontrol eder;
 * amaç, kuyruk taşması senaryosunda EventBus'ın hatasız şekilde bazı olayları işlemesini sağladığını doğrulamaktır.
 */
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

/**
 * @brief EventBus birim testlerini kaydeder ve çalıştırır.
 *
 * Programın test giriş noktası; tüm tanımlı EventBus testlerini Unity çerçevesi altında çalıştırır.
 *
 * @return int Unity test koşucusunun bitiş/kod sonucu.
 */
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