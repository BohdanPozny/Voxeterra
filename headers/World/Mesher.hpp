#pragma once

#include "World/Terrain.hpp"
#include <cstdint>
#include <vector>

// Packed vertex, 4 per quad (the index pattern is shared):
//   bits 0-6 x, 7-13 z, 14-22 y (column cells), 23-25 normal (axis*2 + negative), 26-31 type.
struct ColumnMesh {
    std::vector<uint32_t> solid;
    std::vector<uint32_t> water;
    int minY = 0;  // cell range spanned by the quads, for culling
    int maxY = 0;
};

// Binary greedy meshing of every chunk in the column; borders come from the padding.
void meshColumn(const ColumnVolume& volume, ColumnMesh& out);
