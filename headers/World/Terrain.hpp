#pragma once

#include "World/Chunk.hpp"
#include <cstddef>
#include <vector>

// Cells of one column plus a one-cell border: x and z span [-1, 64], y spans
// [-1, chunks * 64]. The border lets the mesher cull faces without neighbour chunks.
struct ColumnVolume {
    static constexpr int PAD = CHUNK_SIZE + 2;

    std::vector<VoxelType> cells;
    int chunks = 0;
    int filledTop = 0;  // cell layers at or above this are all air

    static std::size_t index(int x, int y, int z) {
        return (static_cast<std::size_t>(y + 1) * PAD + static_cast<std::size_t>(z + 1)) * PAD
               + static_cast<std::size_t>(x + 1);
    }
};

// Deterministic terrain with caves. Each cell takes the voxel at its centre, so every
// level of detail samples the same world; borders on openSides are left as air.
void generateColumn(const ColumnKey& key, unsigned openSides, ColumnVolume& out);

// Spawn position in metres: the nearest dry land to the origin, a few metres above ground.
glm::vec3 findSpawnPoint();
