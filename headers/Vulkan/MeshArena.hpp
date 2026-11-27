#pragma once

#include "Vulkan/Buffer.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

class Device;

// Sub-allocates packed 32-bit vertices out of a few large persistently mapped vertex
// buffers, so the number of Vulkan allocations does not grow with the chunk count.
class MeshArena {
public:
    struct Allocation {
        uint32_t block = UINT32_MAX;
        uint32_t offset = 0;  // in vertices
        uint32_t count = 0;
        bool valid() const { return block != UINT32_MAX; }
    };

    bool init(Device& device, uint32_t blockVertices);
    void cleanup();

    Allocation allocate(uint32_t count);
    void free(const Allocation& allocation);

    uint32_t* data(const Allocation& allocation) const;
    VkBuffer buffer(uint32_t block) const { return m_blocks[block].buffer->getBuffer(); }

    uint64_t usedBytes() const { return m_usedVertices * sizeof(uint32_t); }
    uint64_t capacityBytes() const { return static_cast<uint64_t>(m_blocks.size()) * m_blockVertices * sizeof(uint32_t); }

private:
    struct Block {
        std::unique_ptr<Buffer> buffer;
        uint32_t* mapped = nullptr;
        std::map<uint32_t, uint32_t> freeRanges;  // offset -> length, coalesced
    };

    Device* m_device = nullptr;
    uint32_t m_blockVertices = 0;
    uint64_t m_usedVertices = 0;
    std::vector<Block> m_blocks;

    bool addBlock();
};
