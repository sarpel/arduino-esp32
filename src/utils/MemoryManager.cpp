#include "MemoryManager.h"
#include "../core/SystemManager.h"
#include "EnhancedLogger.h"
#include <memory>
#include "../core/EventBus.h"

// MemoryPool implementation
MemoryPool::MemoryPool(size_t block_size, size_t pool_size)
    : block_size(block_size), pool_size(pool_size), free_blocks(pool_size) {
    
    blocks.reserve(pool_size);
    
    // Allocate all blocks
    for (size_t i = 0; i < pool_size; i++) {
        Block block;
        block.data = malloc(block_size);
        block.in_use = false;
        block.size = block_size;
        blocks.push_back(block);
    }
}

MemoryPool::~MemoryPool() {
    // Free all allocated blocks
    for (auto& block : blocks) {
        if (block.data) {
            free(block.data);
            block.data = nullptr;
        }
    }
}

void* MemoryPool::allocate() {
    if (free_blocks == 0) {
        return nullptr;  // Pool is full
    }
    
    // Find first free block
    for (auto& block : blocks) {
        if (!block.in_use) {
            block.in_use = true;
            free_blocks--;
            return block.data;
        }
    }
    
    return nullptr;  // Should not reach here
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) return;
    
    // Find the block and mark it as free
    for (auto& block : blocks) {
        if (block.data == ptr) {
            if (block.in_use) {
                block.in_use = false;
                free_blocks++;
                return;
            }
            break;
        }
    }
}

bool MemoryPool::owns(void* ptr) const {
    if (!ptr) {
        return false;
    }
    for (const auto& block : blocks) {
        if (block.data == ptr) {
            return true;
        }
    }
    return false;
}

// MemoryManager implementation
MemoryManager::MemoryManager() : initialized(false), emergency_mode(false), emergency_cleanups(0) {}

MemoryManager::~MemoryManager() {
    shutdown();
}

bool MemoryManager::initialize(const MemoryConfig& cfg) {
    if (initialized) {
        return true;
    }
    
    config = cfg;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Initializing MemoryManager");
    }
    
    // Calculate pool sizes based on typical usage
    size_t audio_buffer_size = I2S_BUFFER_SIZE;  // Typical audio buffer size
    size_t network_buffer_size = TCP_CHUNK_SIZE;  // Typical network buffer size
    size_t general_buffer_size = 4096;  // General purpose buffer size
    
    // Initialize memory pools
    audio_buffer_pool = std::unique_ptr<MemoryPool>(new MemoryPool(audio_buffer_size, config.audio_buffer_pool_size));
    network_buffer_pool = std::unique_ptr<MemoryPool>(new MemoryPool(network_buffer_size, config.network_buffer_pool_size));
    general_buffer_pool = std::unique_ptr<MemoryPool>(new MemoryPool(general_buffer_size, 4));  // 4 general buffers
    
    // Reset statistics
    resetStatistics();
    
    initialized = true;
    emergency_mode = false;
    
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Memory pools initialized:");
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "  Audio pool: %u blocks of %u bytes",
                   config.audio_buffer_pool_size, audio_buffer_size);
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "  Network pool: %u blocks of %u bytes",
                   config.network_buffer_pool_size, network_buffer_size);
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "  General pool: 4 blocks of %u bytes", general_buffer_size);
    }
    
    return true;
}

void MemoryManager::shutdown() {
    if (!initialized) {
        return;
    }
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Shutting down MemoryManager");
        printStatistics();

        // Check for memory leaks
        if (stats.current_allocations > 0) {
            logger->log(LogLevel::LOG_WARN, "MemoryManager", __FILE__, __LINE__, "Warning: %u allocations still active at shutdown",
                       stats.current_allocations);
            dumpAllocations();
        }
    }
    
    // Clean up pools
    audio_buffer_pool.reset();
    network_buffer_pool.reset();
    general_buffer_pool.reset();
    
    // Clear tracking
    active_allocations.clear();
    allocation_sources.clear();
    
    initialized = false;
}

void* MemoryManager::allocateAudioBuffer(size_t size, const char* source) {
    if (!initialized) {
        return nullptr;
    }
    
    // Try pool allocation first
    if (size <= audio_buffer_pool->getBlockSize() && !audio_buffer_pool->isFull()) {
        void* ptr = audio_buffer_pool->allocate();
        if (ptr) {
            recordAllocation(ptr, size, source);
            stats.pool_allocations++;
            return ptr;
        }
    }
    
    // Fall back to heap allocation
    return allocateFromHeap(size, source);
}

void* MemoryManager::allocateNetworkBuffer(size_t size, const char* source) {
    if (!initialized) {
        return nullptr;
    }
    
    // Try pool allocation first
    if (size <= network_buffer_pool->getBlockSize() && !network_buffer_pool->isFull()) {
        void* ptr = network_buffer_pool->allocate();
        if (ptr) {
            recordAllocation(ptr, size, source);
            stats.pool_allocations++;
            return ptr;
        }
    }
    
    // Fall back to heap allocation
    return allocateFromHeap(size, source);
}

void* MemoryManager::allocateGeneralBuffer(size_t size, const char* source) {
    if (!initialized) {
        return nullptr;
    }
    
    // Try pool allocation first
    if (size <= general_buffer_pool->getBlockSize() && !general_buffer_pool->isFull()) {
        void* ptr = general_buffer_pool->allocate();
        if (ptr) {
            recordAllocation(ptr, size, source);
            stats.pool_allocations++;
            return ptr;
        }
    }
    
    // Fall back to heap allocation
    return allocateFromHeap(size, source);
}

void* MemoryManager::allocate(size_t size, const char* source) {
    if (!initialized) {
        return nullptr;
    }
    
    // Align size to word boundary
    size = alignSize(size);
    
    // Check size limits
    if (size > config.max_heap_allocation) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_ERROR, "MemoryManager", __FILE__, __LINE__, "Allocation size %u exceeds maximum %u",
                       size, config.max_heap_allocation);
        }
        return nullptr;
    }
    
    // Try appropriate pool based on size
    if (size <= audio_buffer_pool->getBlockSize() && !audio_buffer_pool->isFull()) {
        return allocateAudioBuffer(size, source);
    } else if (size <= network_buffer_pool->getBlockSize() && !network_buffer_pool->isFull()) {
        return allocateNetworkBuffer(size, source);
    } else if (size <= general_buffer_pool->getBlockSize() && !general_buffer_pool->isFull()) {
        return allocateGeneralBuffer(size, source);
    }
    
    // Fall back to heap allocation
    return allocateFromHeap(size, source);
}

void* MemoryManager::allocateFromHeap(size_t size, const char* source) {
    void* ptr = malloc(size);
    if (ptr) {
        recordAllocation(ptr, size, source);
        stats.heap_allocations++;
    } else {
        stats.allocation_failures++;
        
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_ERROR, "MemoryManager", __FILE__, __LINE__, "Heap allocation failed for size %u from %s",
                       size, source);
        }
        
        // Try emergency cleanup
        if (!emergency_mode) {
            emergencyCleanup();
            ptr = malloc(size);
            if (ptr) {
                recordAllocation(ptr, size, source);
                stats.heap_allocations++;
            }
        }
    }
    
    return ptr;
}

void MemoryManager::recordAllocation(void* ptr, size_t size, const char* source) {
    if (!ptr) return;
    
    active_allocations[ptr] = size;
    allocation_sources[ptr] = source;
    
    stats.total_allocations++;
    stats.current_allocations++;
    stats.total_bytes_allocated += size;
    stats.current_bytes_allocated += size;
    
    if (stats.current_allocations > stats.peak_allocations) {
        stats.peak_allocations = stats.current_allocations;
    }
    
    if (stats.current_bytes_allocated > stats.peak_bytes_allocated) {
        stats.peak_bytes_allocated = stats.current_bytes_allocated;
    }
    
    // Check for critical memory condition
    if (getFreeMemory() < config.critical_memory_threshold) {
        auto logger = SystemManager::getInstance().getLogger();
        if (logger) {
            logger->log(LogLevel::LOG_CRITICAL, "MemoryManager", __FILE__, __LINE__, "Critical memory condition - free: %u bytes",
                       getFreeMemory());
        }
        
        // Publish memory critical event
        auto eventBus = SystemManager::getInstance().getEventBus();
        if (eventBus) {
            eventBus->publish(SystemEvent::MEMORY_CRITICAL);
        }
    }
}

void MemoryManager::deallocate(void* ptr) {
    if (!ptr || !initialized) {
        return;
    }
    
    recordDeallocation(ptr);
    
    // Check if it's a pool allocation
    bool found_in_pool = false;
    
    // Try each pool
    if (audio_buffer_pool && audio_buffer_pool->owns(ptr)) {
        audio_buffer_pool->deallocate(ptr);
        found_in_pool = true;
    } else if (network_buffer_pool && network_buffer_pool->owns(ptr)) {
        network_buffer_pool->deallocate(ptr);
        found_in_pool = true;
    } else if (general_buffer_pool && general_buffer_pool->owns(ptr)) {
        general_buffer_pool->deallocate(ptr);
        found_in_pool = true;
    }
    
    // If not in pools, free from heap
    if (!found_in_pool) {
        free(ptr);
    }
}

void MemoryManager::recordDeallocation(void* ptr) {
    if (!ptr) return;
    
    auto alloc_it = active_allocations.find(ptr);
    if (alloc_it != active_allocations.end()) {
        size_t size = alloc_it->second;
        
        stats.total_deallocations++;
        stats.current_allocations--;
        stats.total_bytes_deallocated += size;
        stats.current_bytes_allocated -= size;
        
        active_allocations.erase(alloc_it);
        allocation_sources.erase(ptr);
    }
}

void MemoryManager::emergencyCleanup() {
    emergency_cleanups++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_CRITICAL, "MemoryManager", __FILE__, __LINE__, "Emergency cleanup initiated (#%u)",
                   emergency_cleanups);
    }
    
    enterEmergencyMode();
    
    // Force garbage collection by allocating and freeing large blocks
    const size_t cleanup_size = 4096;
    void* cleanup_ptr = malloc(cleanup_size);
    if (cleanup_ptr) {
        free(cleanup_ptr);
    }
    
    // Perform defragmentation if enabled
    if (config.enable_defragmentation) {
        performDefragmentation();
    }
    
    // Log results
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Emergency cleanup completed - free memory: %u bytes",
                   getFreeMemory());
    }
    
    exitEmergencyMode();
}

void MemoryManager::enterEmergencyMode() {
    emergency_mode = true;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_WARN, "MemoryManager", __FILE__, __LINE__, "Entering emergency memory mode");
    }
}

void MemoryManager::exitEmergencyMode() {
    emergency_mode = false;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Exiting emergency memory mode");
    }
}

void MemoryManager::performDefragmentation() {
    stats.defragmentation_runs++;
    
    auto logger = SystemManager::getInstance().getLogger();
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Performing memory defragmentation");
    }
    
    // Simple defragmentation strategy
    // In a real implementation, this would be more sophisticated
    
    // Force some allocations and deallocations to encourage consolidation
    const size_t temp_size = 1024;
    void* temp_ptrs[4];
    
    for (int i = 0; i < 4; i++) {
        temp_ptrs[i] = malloc(temp_size);
    }
    
    for (int i = 0; i < 4; i++) {
        if (temp_ptrs[i]) {
            free(temp_ptrs[i]);
        }
    }
    
    if (logger) {
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Defragmentation completed");
    }
}

bool MemoryManager::shouldDefragment() const {
    if (!config.enable_defragmentation) {
        return false;
    }
    
    // Check fragmentation ratio
    float fragmentation = getFragmentationRatio();
    return fragmentation > 0.3f;  // Defragment if >30% fragmented
}

void MemoryManager::resetStatistics() {
    stats = MemoryStats();
}

void MemoryManager::printStatistics() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "=== Memory Manager Statistics ===");
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Total allocations: %u", stats.total_allocations);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Total deallocations: %u", stats.total_deallocations);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Current allocations: %u", stats.current_allocations);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Peak allocations: %u", stats.peak_allocations);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Allocation failures: %u", stats.allocation_failures);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Pool allocations: %u", stats.pool_allocations);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Heap allocations: %u", stats.heap_allocations);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Total bytes allocated: %u", stats.total_bytes_allocated);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Current bytes allocated: %u", stats.current_bytes_allocated);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Peak bytes allocated: %u", stats.peak_bytes_allocated);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Emergency cleanups: %u", emergency_cleanups);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Defragmentation runs: %u", stats.defragmentation_runs);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Free memory: %u bytes", getFreeMemory());
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Fragmentation ratio: %.1f%%", getFragmentationRatio() * 100);
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "================================");
}

size_t MemoryManager::getFreeMemory() const {
    return ESP.getFreeHeap();
}

size_t MemoryManager::getTotalMemory() const {
    return ESP.getHeapSize();
}

size_t MemoryManager::getUsedMemory() const {
    return getTotalMemory() - getFreeMemory();
}

size_t MemoryManager::getLargestFreeBlock() const {
    return ESP.getMaxAllocHeap();
}

float MemoryManager::getFragmentationRatio() const {
    size_t free_mem = getFreeMemory();
    size_t largest_block = getLargestFreeBlock();
    
    if (free_mem == 0) return 0.0f;
    return 1.0f - (static_cast<float>(largest_block) / free_mem);
}

size_t MemoryManager::getAudioPoolFreeBlocks() const {
    return audio_buffer_pool ? audio_buffer_pool->getFreeBlocks() : 0;
}

size_t MemoryManager::getNetworkPoolFreeBlocks() const {
    return network_buffer_pool ? network_buffer_pool->getFreeBlocks() : 0;
}

size_t MemoryManager::getGeneralPoolFreeBlocks() const {
    return general_buffer_pool ? general_buffer_pool->getFreeBlocks() : 0;
}

bool MemoryManager::isAudioPoolFull() const {
    return audio_buffer_pool ? audio_buffer_pool->isFull() : true;
}

bool MemoryManager::isNetworkPoolFull() const {
    return network_buffer_pool ? network_buffer_pool->isFull() : true;
}

bool MemoryManager::isGeneralPoolFull() const {
    return general_buffer_pool ? general_buffer_pool->isFull() : true;
}

bool MemoryManager::validateMemory() const {
    // Basic validation
    if (!initialized) return false;
    
    // Check for obvious corruption
    if (stats.current_allocations > stats.total_allocations) return false;
    if (stats.current_bytes_allocated > stats.total_bytes_allocated) return false;
    
    return true;
}

bool MemoryManager::checkForLeaks() const {
    return stats.current_allocations > 0;
}

void MemoryManager::dumpAllocations() const {
    auto logger = SystemManager::getInstance().getLogger();
    if (!logger) return;
    
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "=== Active Memory Allocations ===");
    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "Total active allocations: %u", active_allocations.size());
    
    for (const auto& pair : active_allocations) {
        void* ptr = pair.first;
        size_t size = pair.second;
        const char* source = "unknown";
        
        auto source_it = allocation_sources.find(ptr);
        if (source_it != allocation_sources.end()) {
            source = source_it->second;
        }
        
        logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "  %p: %u bytes from %s", ptr, size, source);
    }

    logger->log(LogLevel::LOG_INFO, "MemoryManager", __FILE__, __LINE__, "=================================");
}

size_t MemoryManager::alignSize(size_t size) {
    // Align to 4-byte boundary
    return (size + 3) & ~3;
}

const char* MemoryManager::getAllocationType(void* ptr) {
    if (!ptr) return "null";
    
    // Static function can't access instance members
    // Return generic allocation type
    return "unknown";
}

bool MemoryManager::isPointerValid(void* ptr) {
    if (!ptr) return false;
    
    // Basic pointer validation
    // Check if it's in valid memory range
    return true;  // Simplified for now
}