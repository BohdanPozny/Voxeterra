#pragma once

#include "World/Voxel.hpp"
#include <array>
#include <vector>
#include <glm/glm.hpp>

class World;

// 64^3 matches the binary greedy meshing implementation (uint64 columns).
constexpr int CHUNK_SIZE = 64;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

// Vertex layout uploaded to voxel.vert (36 bytes: pos + normal + color).
struct VoxelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

class Chunk {
private:
    glm::ivec3 m_position;  // chunk coordinate (not block coordinate).
    std::array<VoxelType, CHUNK_VOLUME> m_voxels;

    bool m_isDirty = true;  // mesh must be rebuilt before the next draw.

    // Opaque mesh.
    std::vector<VoxelVertex> m_vertices;
    std::vector<uint32_t>    m_indices;

    // Translucent (water) mesh rendered in a separate alpha-blended pass.
    std::vector<VoxelVertex> m_waterVertices;
    std::vector<uint32_t>    m_waterIndices;
    inline int getIndex(int x, int y, int z) const {
        return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
    }

public:
    Chunk(const glm::ivec3& position);
    ~Chunk() = default;

    // Voxel accessors (local chunk coordinates).
    VoxelType getVoxel(int x, int y, int z) const;
    void      setVoxel(int x, int y, int z, VoxelType type);
    bool      isInBounds(int x, int y, int z) const;

    // Rebuild the mesh via binary greedy meshing. Passing the world lets us
    // sample neighbour chunks and suppress seams on shared borders.
    void generateMesh();
    void generateMesh(const World* world);
    
    // Getters
    const glm::ivec3& getPosition() const { return m_position; }
    const std::vector<VoxelVertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    const std::vector<VoxelVertex>& getWaterVertices() const { return m_waterVertices; }
    const std::vector<uint32_t>& getWaterIndices() const { return m_waterIndices; }
    bool isDirty() const { return m_isDirty; }
    void setDirty(bool dirty) { m_isDirty = dirty; }

    // Debug helper: fill the chunk with a half-solid stone column plus random grass.
    void fillTestData();
};
