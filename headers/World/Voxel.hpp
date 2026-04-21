#pragma once

#include <cstdint>

// Тип воксела (8 біт)
using VoxelType = uint8_t;

// Константи типів вокселів
namespace VoxelTypes {
    constexpr VoxelType AIR = 0;
    constexpr VoxelType STONE = 1;
    constexpr VoxelType DIRT = 2;
    constexpr VoxelType GRASS = 3;
    constexpr VoxelType SAND = 4;
    constexpr VoxelType WATER = 5;
}

// Перевірка чи воксель прозорий (повітря)
inline bool isVoxelSolid(VoxelType type) {
    return type != VoxelTypes::AIR;
}

// Структура воксела з додатковими даними (якщо потрібно)
struct Voxel {
    VoxelType type;
    
    Voxel() : type(VoxelTypes::AIR) {}
    explicit Voxel(VoxelType t) : type(t) {}
    
    bool isSolid() const { return isVoxelSolid(type); }
    bool isAir() const { return type == VoxelTypes::AIR; }
};
