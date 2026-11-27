#include "Vulkan/MeshArena.hpp"
#include "Vulkan/Device.hpp"
#include <iostream>
#include <iterator>

bool MeshArena::init(Device& device, uint32_t blockVertices) {
    m_device = &device;
    m_blockVertices = blockVertices;
    return addBlock();
}

void MeshArena::cleanup() {
    m_blocks.clear();
    m_usedVertices = 0;
}

bool MeshArena::addBlock() {
    Block block;
    block.buffer = std::make_unique<Buffer>();
    if (!block.buffer->init(*m_device, static_cast<size_t>(m_blockVertices) * sizeof(uint32_t),
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        std::cerr << "[MeshArena] Failed to allocate vertex block" << std::endl;
        return false;
    }
    block.mapped = static_cast<uint32_t*>(block.buffer->map());
    block.freeRanges.emplace(0, m_blockVertices);
    m_blocks.push_back(std::move(block));
    return true;
}

MeshArena::Allocation MeshArena::allocate(uint32_t count) {
    if (count == 0 || count > m_blockVertices) return {};

    for (int attempt = 0; attempt < 2; ++attempt) {
        for (uint32_t b = 0; b < m_blocks.size(); ++b) {
            auto& ranges = m_blocks[b].freeRanges;
            for (auto it = ranges.begin(); it != ranges.end(); ++it) {
                if (it->second < count) continue;
                const uint32_t offset = it->first;
                const uint32_t rest = it->second - count;
                ranges.erase(it);
                if (rest > 0) ranges.emplace(offset + count, rest);
                m_usedVertices += count;
                return {b, offset, count};
            }
        }
        if (attempt == 0 && !addBlock()) break;
    }
    return {};
}

void MeshArena::free(const Allocation& allocation) {
    if (!allocation.valid()) return;
    auto& ranges = m_blocks[allocation.block].freeRanges;
    uint32_t offset = allocation.offset;
    uint32_t length = allocation.count;

    // Merge with the free range after and before this one.
    auto next = ranges.lower_bound(offset);
    if (next != ranges.end() && offset + length == next->first) {
        length += next->second;
        next = ranges.erase(next);
    }
    if (next != ranges.begin()) {
        auto prev = std::prev(next);
        if (prev->first + prev->second == offset) {
            prev->second += length;
            m_usedVertices -= allocation.count;
            return;
        }
    }
    ranges.emplace(offset, length);
    m_usedVertices -= allocation.count;
}

uint32_t* MeshArena::data(const Allocation& allocation) const {
    return m_blocks[allocation.block].mapped + allocation.offset;
}
