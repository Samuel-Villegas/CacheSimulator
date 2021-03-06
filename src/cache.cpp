#include <iostream>
#include <ctime>
#include <random>
#include "cache.hpp"

Cache::Cache(CacheInfo cache_info) : cache_info{cache_info} {
    srand(time(0));
    this->data = new std::list<struct CacheEntry>*[cache_info.numberSets]();
    for(unsigned int i = 0; i < cache_info.numberSets; i++) {
        this->data[i] = new std::list<struct CacheEntry>();
        for(unsigned int j = 0; j < cache_info.associativity; j++) {
            this->data[i]->emplace_front();
        }
    }
};

Cache::~Cache() {
    delete[] this->data;
}

void Cache::access(CacheResponse* response, bool isWrite, unsigned int setIndex, unsigned long int tag, int numBytes) {
    // Index into cache set
    std::list<struct CacheEntry>* set = this->data[setIndex];
    bool is_prefetched = ((
            (tag << (this->cache_info.numSetIndexBits + this->cache_info.numByteOffsetBits))
            | (setIndex << this->cache_info.numByteOffsetBits)
        )
        + this->cache_info.blockSize
        == this->prefetchAddress
    );
    this->prefetchAddress = ((tag << (this->cache_info.numByteOffsetBits + this->cache_info.numSetIndexBits)) | (setIndex << this->cache_info.numByteOffsetBits));

    // Loop through and check blocks
    bool allFull = true;
    for(auto & ablock : *set) {
        if(ablock.valid && ablock.tag == tag) {
            response->hits++;
            if(isWrite) {
                // Modify the data in the cache
                std::cout << "->";
                response->cycles+= this->cache_info.cacheAccessCycles;
                ablock.valid = true;
                ablock.dirty = true;
                ablock.tag = tag;

                // Write modified data to main memory if using write-through
                if(ablock.dirty && this->cache_info.wp == WritePolicy::WriteBack) {
                    std::cout << "=>";
                    if(is_prefetched) response->cycles++;
                    else response->cycles+= this->cache_info.memoryAccessCycles;
                    ablock.dirty = false;
                    ablock.valid = false;
                }
            } else {
                // Read data from cache
                std::cout << "<-";
                response->cycles+= this->cache_info.cacheAccessCycles;
            }
            std::cout << std::endl;
            return;
        } else allFull = false;
    }
    response->misses++;

    // If no available blocks, must get a block to evict
    struct CacheEntry& nextAvailableBlock = set->front(); // Init reference with non-null value
    if(allFull) {
        // Rotate list randomly if using random replacement
        if(this->cache_info.rp == ReplacementPolicy::Random) {
            int index = rand() % this->cache_info.associativity;
            for(int count = 0; count < index; count++) {
                struct CacheEntry block = set->front();
                set->pop_front();
                set->push_back(block);
            }
        }

        // Keep rotating until one or zero avaiable blocks found
        for(unsigned int i = 0; (i < this->cache_info.associativity) && (set->front().valid); i++) {
            struct CacheEntry block = set->front();
            set->pop_front();
            set->push_back(block);
        }

        // Get the next available block (LRU unless randomized)
        nextAvailableBlock = set->front();

        // Save updates now if using write-back
        if(nextAvailableBlock.dirty && this->cache_info.wp == WritePolicy::WriteBack) {
            // Block to evict has been modified, write-back data to main memory
            std::cout << "=>";
            if(is_prefetched) response->cycles++;
            else response->cycles+= this->cache_info.memoryAccessCycles;
            nextAvailableBlock.dirty = false;
            response->dirtyEvictions++;
        }

        // Evict the block
        nextAvailableBlock.valid = false;
        nextAvailableBlock.dirty = false;
        response->evictions++;
    }

    // Check for evictions
    if(nextAvailableBlock.valid) {
        // Save updates now if using write-back
        if(nextAvailableBlock.dirty && this->cache_info.wp == WritePolicy::WriteBack) {
            // Block to evict has been modified, write-back data to main memory
            std::cout << "=>";
            if(is_prefetched) response->cycles++;
            else response->cycles+= this->cache_info.memoryAccessCycles;
            nextAvailableBlock.dirty = false;
            response->dirtyEvictions++;
        }

        // Evict the block
        nextAvailableBlock.valid = false;
        nextAvailableBlock.dirty = false;
        if(!is_prefetched) response->evictions++;
    }

    // Load block into cache
    std::cout << "<=";
    if(is_prefetched) response->cycles++;
    else response->cycles+= this->cache_info.memoryAccessCycles;
    nextAvailableBlock.valid = true;
    nextAvailableBlock.tag = tag;

    if(isWrite) {
        // Modify the data in the cache
        std::cout << "->";
        response->cycles+= this->cache_info.cacheAccessCycles;
        nextAvailableBlock.valid = true;
        nextAvailableBlock.dirty = true;
        nextAvailableBlock.tag = tag;

        // Write modified data to main memory if using write-through
        if(nextAvailableBlock.dirty && this->cache_info.wp == WritePolicy::WriteBack) {
            std::cout << "=>";
            if(is_prefetched) response->cycles++;
            else response->cycles+= this->cache_info.memoryAccessCycles;
            nextAvailableBlock.dirty = false;
            nextAvailableBlock.valid = false;
        }
    } else {
        // Read data from cache
        std::cout << "<-";
        response->cycles+= this->cache_info.cacheAccessCycles;
    }

    // Move block to end of LRU/Random list
    struct CacheEntry block = set->front();
    set->pop_front();
    set->push_back(block);

    std::cout << std::endl;
}

void Cache::print() {
    //    std::cout << "L1" << std::endl
    //        << "----" << std::endl 
    //        << "Number of Offset Bits: " << this->cache_info.numByteOffsetBits << std::endl
    //        << "Number of Index Bits: " << this->cache_info.numSetIndexBits << std::endl
    //        << "Number of Sets: " << this->cache_info.numberSets << std::endl
    //        << "Bytes per Block: " << this->cache_info.blockSize << std::endl
    //        << "Associativity: " << this->cache_info.associativity << std::endl
    //        << "Replacement Policy: " << ((this->cache_info.rp == ReplacementPolicy::Random) ? "Random":"Least Recently Used (LRU)") << std::endl
    //        << "Write Policy: " << ((this->cache_info.wp == WritePolicy::WriteBack) ? "Write Back":"Write Through") << std::endl
    //        << "Cycles per Cache Access: " << this->cache_info.cacheAccessCycles << std::endl
    //        << "Cycles per Memory Access: " << this->cache_info.memoryAccessCycles << std::endl
    //        << "----" << std::endl 
    //        << std::endl;
    for(unsigned int i = 0; i < this->cache_info.numberSets; i++) {
        for(auto ablock : *this->data[i]) {
            std::cout << '[' << ablock.valid << ablock.dirty << '|' << ablock.tag << ']';
        }
        std::cout << std::endl;
    }
    std::cout << "----" << std::endl;
}

