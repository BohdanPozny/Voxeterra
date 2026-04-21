#pragma once

#include "World/Chunk.hpp"
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

// Hash specialization for glm::ivec3 keys.
struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        std::size_t h1 = std::hash<int>{}(v.x);
        std::size_t h2 = std::hash<int>{}(v.y);
        std::size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
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

    // No-op with streaming enabled; retained for backward compatibility.
    void generateTestWorld();

    // Release a chunk's memory.
    void removeChunk(const glm::ivec3& position);

    // Rebuild meshes for every dirty chunk.
    void updateChunks();
    const auto& getChunks() const { return m_chunks; }
};
