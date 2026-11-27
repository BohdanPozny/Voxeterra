#pragma once

#include <cstdint>

// One byte per voxel keeps memory footprint low.
using VoxelType = uint8_t;

// World units are metres; voxel grid coordinates are scaled by this on render.
constexpr float VOXEL_SIZE = 0.1f;

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
