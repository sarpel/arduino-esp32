#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include <map>

// Memory pool for fixed-size allocations
class MemoryPool {
private:
    struct Block {
        void* data;
        bool in_use;
        size_t size;
    };
    
    std::vector<Block> blocks;
    size_t block_size;
    size_t pool_size;
    size_t free_blocks;
    
public:
    MemoryPool(size_t block_size, size_t pool_size);
    ~MemoryPool();
    
    void* allocate();
    void deallocate(void* ptr);
    bool isFull() const { return free_blocks == 0; }
    bool isEmpty() const { return free_blocks == pool_size; }
    size_t getFreeBlocks() const { return free_blocks; }
    size_t getTotalBlocks() const { return pool_size; }
    size_t getBlockSize() const { return block_size; }
};

// Memory allocation statistics
struct MemoryStats {
    uint32_t total_allocations;
    uint32_t total_deallocations;
    uint32_t current_allocations;
    uint32_t peak_allocations;
    uint32_t allocation_failures;
    uint32_t pool_allocations;
    uint32_t heap_allocations;
    size_t total_bytes_allocated;
    size_t total_bytes_deallocated;
    size_t current_bytes_allocated;
    size_t peak_bytes_allocated;
    uint32_t fragmentation_events;
    uint32_t defragmentation_runs;
    
    MemoryStats() : total_allocations(0), total_deallocations(0), current_allocations(0),
                   peak_allocations(0), allocation_failures(0), pool_allocations(0),
                   heap_allocations(0), total_bytes_allocated(0), total_bytes_deallocated(0),
                   current_bytes_allocated(0), peak_bytes_allocated(0),
                   fragmentation_events(0), defragmentation_runs(0) {}
};

// Memory manager configuration
struct MemoryConfig {
    size_t audio_buffer_pool_size;
    size_t network_buffer_pool_size;
    size_t max_heap_allocation;
    bool enable_defragmentation;
    bool enable_statistics;
    uint32_t defragmentation_threshold;
    uint32_t critical_memory_threshold;
    
    MemoryConfig() : audio_buffer_pool_size(10), network_buffer_pool_size(5),
                    max_heap_allocation(65536), enable_defragmentation(true),
                    enable_statistics(true), defragmentation_threshold(4096),
                    critical_memory_threshold(16384) {}
};

class MemoryManager {
private:
    // Memory pools
    std::unique_ptr<MemoryPool> audio_buffer_pool;
    std::unique_ptr<MemoryPool> network_buffer_pool;
    std::unique_ptr<MemoryPool> general_buffer_pool;
    
    // Statistics
    MemoryStats stats;
    MemoryConfig config;
    
    // State
    bool initialized;
    bool emergency_mode;
    uint32_t emergency_cleanups;
    
    // Tracking
    std::map<void*, size_t> active_allocations;
    std::map<void*, const char*> allocation_sources;
    
    // Internal methods
    void* allocateFromPool(size_t size, const char* source);
    void* allocateFromHeap(size_t size, const char* source);
    void recordAllocation(void* ptr, size_t size, const char* source);
    void recordDeallocation(void* ptr);
    bool shouldDefragment();
    void performDefragmentation();
    void enterEmergencyMode();
    void exitEmergencyMode();
    
public:
    MemoryManager();
    ~MemoryManager();
    
    // Lifecycle
    bool initialize(const MemoryConfig& config = MemoryConfig());
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    // Allocation methods
    void* allocateAudioBuffer(size_t size, const char* source = "unknown");
    void* allocateNetworkBuffer(size_t size, const char* source = "unknown");
    void* allocateGeneralBuffer(size_t size, const char* source = "unknown");
    void* allocate(size_t size, const char* source = "unknown");
    
    // Deallocation methods
    void deallocate(void* ptr);
    void deallocateAudioBuffer(void* ptr);
    void deallocateNetworkBuffer(void* ptr);
    
    // Emergency cleanup
    void emergencyCleanup();
    bool isInEmergencyMode() const { return emergency_mode; }
    uint32_t getEmergencyCleanups() const { return emergency_cleanups; }
    
    // Statistics
    const MemoryStats& getStatistics() const { return stats; }
    void resetStatistics();
    void printStatistics() const;
    
    // Memory information
    size_t getFreeMemory() const;
    size_t getTotalMemory() const;
    size_t getUsedMemory() const;
    size_t getLargestFreeBlock() const;
    float getFragmentationRatio() const;
    uint32_t getActiveAllocations() const { return stats.current_allocations; }
    
    // Pool information
    size_t getAudioPoolFreeBlocks() const;
    size_t getNetworkPoolFreeBlocks() const;
    size_t getGeneralPoolFreeBlocks() const;
    bool isAudioPoolFull() const;
    bool isNetworkPoolFull() const;
    bool isGeneralPoolFull() const;
    
    // Memory validation
    bool validateMemory() const;
    bool checkForLeaks() const;
    void dumpAllocations() const;
    
    // Utility
    static size_t alignSize(size_t size);
    static const char* getAllocationType(void* ptr) const;
    static bool isPointerValid(void* ptr);
};

// Global memory manager access
#define MEMORY_MANAGER() (SystemManager::getInstance().getMemoryManager())

// Convenience macros for memory allocation
#define ALLOCATE_AUDIO_BUFFER(size) MEMORY_MANAGER()->allocateAudioBuffer(size, __FUNCTION__)
#define ALLOCATE_NETWORK_BUFFER(size) MEMORY_MANAGER()->allocateNetworkBuffer(size, __FUNCTION__)
#define ALLOCATE_GENERAL_BUFFER(size) MEMORY_MANAGER()->allocateGeneralBuffer(size, __FUNCTION__)
#define DEALLOCATE_BUFFER(ptr) MEMORY_MANAGER()->deallocate(ptr)

#endif // MEMORY_MANAGER_H