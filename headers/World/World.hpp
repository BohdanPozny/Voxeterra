#pragma once

#include "World/Chunk.hpp"
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

// Hash specialization for glm::ivec3 keys.
struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        // std::hash<int> is the identity on libstdc++, so mix with large primes.
        return (static_cast<std::size_t>(v.x) * 73856093u) ^
               (static_cast<std::size_t>(v.y) * 19349663u) ^
               (static_cast<std::size_t>(v.z) * 83492791u);
    }
};

class World {
private:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> m_chunks;

public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Chunk lookup/creation (chunk coordinates).
    Chunk*       getChunk(const glm::ivec3& position);
    const Chunk* getChunk(const glm::ivec3& position) const;
    Chunk*       createChunk(const glm::ivec3& position);

    // Voxel accessors using world (block) coordinates.
    VoxelType getVoxel(int x, int y, int z);
    void      setVoxel(int x, int y, int z, VoxelType type);

    // Deterministic terrain generator seeded by chunk coordinates.
    void generateChunkData(Chunk* chunk);

    // Release a chunk's memory.
    void removeChunk(const glm::ivec3& position);

    const auto& getChunks() const { return m_chunks; }
};
