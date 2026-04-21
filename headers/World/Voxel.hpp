#pragma once

#include <cstdint>
#include <glm/glm.hpp>

// One byte per voxel keeps memory footprint low.
using VoxelType = uint8_t;

// Voxel type identifiers.
namespace VoxelTypes {
    constexpr VoxelType AIR = 0;
    constexpr VoxelType STONE = 1;
    constexpr VoxelType DIRT = 2;
    constexpr VoxelType GRASS = 3;
    constexpr VoxelType SAND = 4;
    constexpr VoxelType WATER = 5;
}

// Non-air voxel occupies its cell.
inline bool isVoxelSolid(VoxelType type) {
    return type != VoxelTypes::AIR;
}

// Translucent voxels (water) go through the alpha-blended pass.
inline bool isVoxelTransparent(VoxelType type) {
    return type == VoxelTypes::WATER;
}

// Opaque voxels occlude everything behind them in the solid pass.
inline bool isVoxelOpaque(VoxelType type) {
    return isVoxelSolid(type) && !isVoxelTransparent(type);
}

// Fixed palette colour per voxel type. Computed on the CPU and baked into vertices.
inline glm::vec3 voxelTypeToColor(VoxelType type) {
    switch (type) {
        case VoxelTypes::STONE: return glm::vec3(0.5f, 0.5f, 0.5f);
        case VoxelTypes::DIRT:  return glm::vec3(0.55f, 0.35f, 0.2f);
        case VoxelTypes::GRASS: return glm::vec3(0.3f, 0.7f, 0.3f);
        case VoxelTypes::SAND:  return glm::vec3(0.9f, 0.85f, 0.6f);
        case VoxelTypes::WATER: return glm::vec3(0.2f, 0.4f, 0.8f);
        default:                return glm::vec3(1.0f, 1.0f, 1.0f);
    }
}

