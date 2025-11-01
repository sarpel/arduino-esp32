/**
 * @file test_memory_manager.cpp
 * @brief Unit tests for MemoryManager component
 *
 * Tests memory pool allocation, deallocation, and emergency cleanup.
 */

#include <unity.h>
#include "utils/MemoryManager.h"

MemoryManager* memManager = nullptr;

/**
 * @brief Her testten önce global MemoryManager örneğini oluşturur ve başlatır.
 *
 * MemoryManager için bir örnek ayırır, ses havuzu için 4 blok ve ağ havuzu için 2 blok olacak şekilde
 * MemoryConfig yapılandırmasını uygular, istatistik toplamayı etkinleştirir ve yöneticiyi başlatır.
 * Başlatmanın başarılı olduğunu test doğrulamalarıyla garantiler.
 */
void setUp(void) {
    // Initialize before each test
    memManager = new MemoryManager();
    TEST_ASSERT_NOT_NULL(memManager);

    MemoryConfig config;
    config.audio_buffer_pool_size = 4;
    config.network_buffer_pool_size = 2;
    config.enable_statistics = true;

    TEST_ASSERT_TRUE(memManager->initialize(config));
}

/**
 * @brief Her testten sonra test ortamını temizler.
 *
 * Eğer küresel `memManager` örneği mevcutsa, önce `shutdown()` çağrısı ile kapatır,
 * ardından belleği serbest bırakır ve `memManager` işaretçisini `nullptr` olarak ayarlar.
 */
void tearDown(void) {
    // Cleanup after each test
    if (memManager) {
        memManager->shutdown();
        delete memManager;
        memManager = nullptr;
    }
}

// Test: Memory Manager Initialization
void test_memory_manager_initialization() {
    TEST_ASSERT_TRUE(memManager->isInitialized());
    TEST_ASSERT_EQUAL(4, memManager->getAudioPoolFreeBlocks());
    TEST_ASSERT_EQUAL(2, memManager->getNetworkPoolFreeBlocks());
}

// Test: Audio Buffer Allocation
void test_audio_buffer_allocation() {
    void* buffer = memManager->allocateAudioBuffer(4096, "test");
    TEST_ASSERT_NOT_NULL(buffer);

    // Pool should have one less free block
    TEST_ASSERT_EQUAL(3, memManager->getAudioPoolFreeBlocks());

    memManager->deallocate(buffer);

    // Pool should be restored
    TEST_ASSERT_EQUAL(4, memManager->getAudioPoolFreeBlocks());
}

// Test: Network Buffer Allocation
void test_network_buffer_allocation() {
    void* buffer = memManager->allocateNetworkBuffer(2048, "test");
    TEST_ASSERT_NOT_NULL(buffer);

    TEST_ASSERT_EQUAL(1, memManager->getNetworkPoolFreeBlocks());

    memManager->deallocate(buffer);
    TEST_ASSERT_EQUAL(2, memManager->getNetworkPoolFreeBlocks());
}

/**
 * @brief Ses havuzu tükendiğinde bellek yöneticisinin davranışını doğrular.
 *
 * Havuzdaki tüm bloklar ayrıldığında pool'un dolu olarak raporlandığını, ek bir
 * ayrılmanın yığına (heap) geri düştüğünü ve tüm bloklar serbest bırakıldıktan
 * sonra havuzun tekrar dolu olmadığını doğrular.
 */
void test_pool_exhaustion() {
    void* buffers[5];

    // Allocate all 4 pool blocks
    for (int i = 0; i < 4; i++) {
        buffers[i] = memManager->allocateAudioBuffer(4096, "test");
        TEST_ASSERT_NOT_NULL(buffers[i]);
    }

    TEST_ASSERT_TRUE(memManager->isAudioPoolFull());

    // 5th allocation should fallback to heap
    buffers[4] = memManager->allocateAudioBuffer(4096, "test");
    TEST_ASSERT_NOT_NULL(buffers[4]);

    // Cleanup
    for (int i = 0; i < 5; i++) {
        memManager->deallocate(buffers[i]);
    }

    TEST_ASSERT_FALSE(memManager->isAudioPoolFull());
}

// Test: Memory Statistics
void test_memory_statistics() {
    const MemoryStats& stats_before = memManager->getStatistics();
    uint32_t alloc_count_before = stats_before.total_allocations;

    void* buffer = memManager->allocateAudioBuffer(4096, "test");
    TEST_ASSERT_NOT_NULL(buffer);

    const MemoryStats& stats_after = memManager->getStatistics();
    TEST_ASSERT_EQUAL(alloc_count_before + 1, stats_after.total_allocations);
    TEST_ASSERT_EQUAL(1, stats_after.current_allocations);

    memManager->deallocate(buffer);

    const MemoryStats& stats_final = memManager->getStatistics();
    TEST_ASSERT_EQUAL(0, stats_final.current_allocations);
}

// Test: Emergency Cleanup
void test_emergency_cleanup() {
    // Allocate some buffers
    void* buffer1 = memManager->allocateAudioBuffer(4096, "test1");
    void* buffer2 = memManager->allocateNetworkBuffer(2048, "test2");

    TEST_ASSERT_NOT_NULL(buffer1);
    TEST_ASSERT_NOT_NULL(buffer2);

    // Trigger emergency cleanup
    memManager->emergencyCleanup();

    TEST_ASSERT_TRUE(memManager->isInEmergencyMode());

    // Cleanup
    memManager->deallocate(buffer1);
    memManager->deallocate(buffer2);
}

// Test: Null Pointer Handling
void test_null_pointer_handling() {
    // Should not crash
    memManager->deallocate(nullptr);

    // Statistics should not change
    const MemoryStats& stats = memManager->getStatistics();
    TEST_ASSERT_EQUAL(0, stats.total_deallocations);
}

// Test: Mixed Allocation Types
void test_mixed_allocations() {
    void* audio_buf = memManager->allocateAudioBuffer(4096, "audio");
    void* network_buf = memManager->allocateNetworkBuffer(2048, "network");
    void* general_buf = memManager->allocateGeneralBuffer(512, "general");

    TEST_ASSERT_NOT_NULL(audio_buf);
    TEST_ASSERT_NOT_NULL(network_buf);
    TEST_ASSERT_NOT_NULL(general_buf);

    // All three pools should have reduced free blocks
    TEST_ASSERT_EQUAL(3, memManager->getAudioPoolFreeBlocks());
    TEST_ASSERT_EQUAL(1, memManager->getNetworkPoolFreeBlocks());

    // Cleanup in different order
    memManager->deallocate(network_buf);
    memManager->deallocate(audio_buf);
    memManager->deallocate(general_buf);

    // All pools should be restored
    TEST_ASSERT_EQUAL(4, memManager->getAudioPoolFreeBlocks());
    TEST_ASSERT_EQUAL(2, memManager->getNetworkPoolFreeBlocks());
}

/**
 * @brief Bellek sızıntısı algılama mekanizmasının doğru çalıştığını doğrular.
 *
 * Bu test, bir ses tamponu ayırıp kasıtlı olarak hemen serbest bırakarak sızıntı senaryosu oluşturur,
 * ardından checkForLeaks çağrısının bir sızıntı raporladığını doğrular ve sonunda testi gerçek sızıntıya
 * yol açmayacak şekilde ayrılan belleği serbest bırakır.
 */
void test_memory_leak_detection() {
    void* leaked_buffer = memManager->allocateAudioBuffer(4096, "leaked");
    TEST_ASSERT_NOT_NULL(leaked_buffer);

    // Don't deallocate - simulate leak

    bool has_leaks = memManager->checkForLeaks();
    TEST_ASSERT_TRUE(has_leaks);

    // Cleanup to prevent actual leak in test
    memManager->deallocate(leaked_buffer);
}

/**
 * @brief Unity test framework kullanarak tüm MemoryManager birim testlerini çalıştırır.
 *
 * @param argc Komut satırından geçirilen argüman sayısı.
 * @param argv Komut satırından geçirilen argümanların dizisi.
 * @return int Unity test koşucusunun çıkış kodu.
 */
int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_memory_manager_initialization);
    RUN_TEST(test_audio_buffer_allocation);
    RUN_TEST(test_network_buffer_allocation);
    RUN_TEST(test_pool_exhaustion);
    RUN_TEST(test_memory_statistics);
    RUN_TEST(test_emergency_cleanup);
    RUN_TEST(test_null_pointer_handling);
    RUN_TEST(test_mixed_allocations);
    RUN_TEST(test_memory_leak_detection);

    return UNITY_END();
}