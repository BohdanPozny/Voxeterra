#pragma once

#include "World/Voxel.hpp"
#include <cstddef>
#include <glm/glm.hpp>

// 64^3 matches the binary greedy meshing implementation (uint64 columns).
constexpr int CHUNK_SIZE = 64;
// World height in voxels (25.6 m); a full-detail column is 4 chunks tall.
constexpr int WORLD_HEIGHT = 256;

// A column of chunks in XZ at a level of detail: level L covers (64 << L) voxels per
// side with cells of (1 << L) voxels, so every level meshes the same number of cells.
struct ColumnKey {
    int level = 0;
    int x = 0;
    int z = 0;

    bool operator==(const ColumnKey&) const = default;
    int cellSize() const { return 1 << level; }
    int sizeVoxels() const { return CHUNK_SIZE << level; }
    int chunkCount() const {
        const int cells = WORLD_HEIGHT >> level;
        return cells > CHUNK_SIZE ? cells / CHUNK_SIZE : 1;
    }
};

struct ColumnKeyHash {
    std::size_t operator()(const ColumnKey& k) const {
        return (static_cast<std::size_t>(k.x) * 73856093u) ^
               (static_cast<std::size_t>(k.z) * 83492791u) ^
               (static_cast<std::size_t>(k.level) * 19349663u);
    }
};

// Bits of the open-side mask: sides whose neighbour is not a same-level column, so
// border faces must be emitted (they double as skirts over LOD seams).
enum : unsigned { SIDE_NEG_X = 1, SIDE_POS_X = 2, SIDE_NEG_Z = 4, SIDE_POS_Z = 8 };

// Full-detail chunk containing a position given in voxel units.
inline glm::ivec3 worldToChunkCoord(const glm::vec3& voxelPos) {
    return glm::ivec3(glm::floor(voxelPos / static_cast<float>(CHUNK_SIZE)));
}
