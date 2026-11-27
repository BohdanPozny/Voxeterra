#include "World/Terrain.hpp"
#include "utils/Profiler.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

inline uint32_t hash2i(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

inline uint32_t hash3i(int x, int y, int z) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u
               + static_cast<uint32_t>(z) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Scales the hash output into [0, 1).
inline float hashToUnit(uint32_t h) {
    return (h & 0xFFFFFF) / static_cast<float>(0x1000000);
}

inline float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Bilinearly interpolated value noise with smoothstep, in [0, 1].
float valueNoise2D(float x, float y) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const float sx = smoothstep(x - xi);
    const float sy = smoothstep(y - yi);

    const float ab = lerp(hashToUnit(hash2i(xi, yi)),     hashToUnit(hash2i(xi + 1, yi)),     sx);
    const float cd = lerp(hashToUnit(hash2i(xi, yi + 1)), hashToUnit(hash2i(xi + 1, yi + 1)), sx);
    return lerp(ab, cd, sy);
}

// Trilinearly interpolated value noise with smoothstep, in [-1, 1].
float valueNoise3D(float x, float y, float z) {
    const int xi = static_cast<int>(std::floor(x));
    const int yi = static_cast<int>(std::floor(y));
    const int zi = static_cast<int>(std::floor(z));
    const float sx = smoothstep(x - xi);
    const float sy = smoothstep(y - yi);
    const float sz = smoothstep(z - zi);

    auto corner = [&](int dx, int dy, int dz) { return hashToUnit(hash3i(xi + dx, yi + dy, zi + dz)); };
    const float x00 = lerp(corner(0, 0, 0), corner(1, 0, 0), sx);
    const float x10 = lerp(corner(0, 1, 0), corner(1, 1, 0), sx);
    const float x01 = lerp(corner(0, 0, 1), corner(1, 0, 1), sx);
    const float x11 = lerp(corner(0, 1, 1), corner(1, 1, 1), sx);
    return lerp(lerp(x00, x10, sy), lerp(x01, x11, sy), sz) * 2.0f - 1.0f;
}

// Fractional Brownian motion summation of value noise.
float fbm2D(float x, float y, int octaves) {
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise2D(x * freq, y * freq);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
}

// Terrain tunables in metres; the world column is 25.6 m tall.
constexpr float BASE_HEIGHT_M = 4.5f;
constexpr float HILL_AMP_M    = 3.0f;    // small-scale hills
constexpr float MOUNT_AMP_M   = 12.0f;   // gated mountains
constexpr float CONT_AMP_M    = 3.0f;    // continental drift

constexpr float CONT_SCALE  = 1.0f / 200.0f;  // very large features
constexpr float HILL_SCALE  = 1.0f / 25.0f;   // small hills
constexpr float MOUNT_SCALE = 1.0f / 80.0f;   // mountain ranges

// 6.4 m is a multiple of every coarse cell size, so water surfaces line up across LODs.
constexpr int SEA_LEVEL  = 64;
constexpr int BEACH      = 3;
constexpr int DIRT_DEPTH = 6;

// Spaghetti caves: tunnels run where two independent noise fields are both near zero.
constexpr float CAVE_SCALE     = 1.0f / 12.0f;  // per metre
constexpr float CAVE_Y_STRETCH = 1.8f;          // flattens tunnels
constexpr float CAVE_THRESHOLD = 0.014f;
constexpr int CAVE_MIN_Y       = 5;             // solid floor at the bottom of the world
constexpr int CAVE_SEA_COVER   = 15;            // rock kept under the sea floor
constexpr int CAVE_FADE        = 12;            // tunnels narrow towards the surface
constexpr int CAVE_MAX_LEVEL   = 0;             // caves are invisible at coarser levels

// Coarse sampling grids (in cells); both fields are smooth at this resolution.
constexpr int HEIGHT_STEP = 4;
constexpr int CAVE_STEP   = 8;
constexpr int CAVE_STEP_Y = 4;

constexpr int P = ColumnVolume::PAD;

// Terrain surface height in voxels at a position given in metres.
float terrainHeight(float x, float z) {
    const float c = fbm2D(x * CONT_SCALE, z * CONT_SCALE, 4) * 2.0f - 1.0f;
    const float h = fbm2D(x * HILL_SCALE + 17.3f, z * HILL_SCALE + 41.1f, 4);
    float m = fbm2D(x * MOUNT_SCALE + 123.0f, z * MOUNT_SCALE + 77.0f, 5);
    m = std::pow(m, 2.5f);
    const float mountainMask = std::min(1.0f, std::max(0.0f, c - 0.1f) * 1.3f);

    const float height = BASE_HEIGHT_M + c * CONT_AMP_M + h * HILL_AMP_M + m * mountainMask * MOUNT_AMP_M;
    return height / VOXEL_SIZE;
}

float caveField(float x, float y, float z) {
    x *= CAVE_SCALE;
    y *= CAVE_SCALE * CAVE_Y_STRETCH;
    z *= CAVE_SCALE;
    const float a = (valueNoise3D(x, y, z) + 0.5f * valueNoise3D(x * 2.0f + 31.7f, y * 2.0f, z * 2.0f)) / 1.5f;
    const float b = (valueNoise3D(x + 91.3f, y + 17.1f, z + 53.9f)
                   + 0.5f * valueNoise3D(x * 2.0f + 7.9f, y * 2.0f + 61.3f, z * 2.0f)) / 1.5f;
    return a * a + b * b;
}

// Number of cells whose centre lies below a height given in voxels.
inline int cellsBelow(int voxels, int cellSize) {
    const int half = cellSize >> 1;
    return voxels <= half ? 0 : (voxels - half - 1) / cellSize + 1;
}

struct ColumnInfo {
    int stoneTop;   // cells below are stone
    int top;        // cells below are solid; cell top - 1 is the surface
    int waterTop;   // cells in [top, waterTop) are water
    bool underwater;
    VoxelType surface;
};

void carveCaves(const ColumnKey& key, const std::array<ColumnInfo, P * P>& info, ColumnVolume& out) {
    ZoneScopedN("Carve caves");
    constexpr int NODES = (CHUNK_SIZE + 2 * CAVE_STEP) / CAVE_STEP + 1;

    // Tunnels stay above the floor and, under the sea, below the rock cover.
    std::array<int, P * P> top, carveTop;
    int maxCarve = 0;
    for (int c = 0; c < P * P; ++c) {
        top[c] = info[c].top;
        carveTop[c] = info[c].underwater ? info[c].top - CAVE_SEA_COVER : info[c].top;
        maxCarve = std::max(maxCarve, carveTop[c]);
    }
    if (maxCarve <= CAVE_MIN_Y) return;

    const int ox = key.x * key.sizeVoxels();
    const int oz = key.z * key.sizeVoxels();
    const int layers = (maxCarve - 1) / CAVE_STEP_Y + 2;

    thread_local std::vector<float> grid;
    grid.resize(static_cast<std::size_t>(layers) * NODES * NODES);
    for (int j = 0; j < layers; ++j) {
        const float y = static_cast<float>(j * CAVE_STEP_Y) * VOXEL_SIZE;
        for (int k = 0; k < NODES; ++k) {
            const float z = static_cast<float>(oz + k * CAVE_STEP - CAVE_STEP) * VOXEL_SIZE;
            for (int i = 0; i < NODES; ++i) {
                const float x = static_cast<float>(ox + i * CAVE_STEP - CAVE_STEP) * VOXEL_SIZE;
                grid[(static_cast<std::size_t>(j) * NODES + k) * NODES + i] = caveField(x, y, z);
            }
        }
    }

    // Grid layers expanded to every cell, separably (x, then z); interpolation weights
    // are the same for both axes and every layer.
    std::array<int, P> node;
    std::array<float, P> frac;
    for (int x = -1; x <= CHUNK_SIZE; ++x) {
        node[x + 1] = (x + CAVE_STEP) / CAVE_STEP;
        frac[x + 1] = static_cast<float>((x + CAVE_STEP) % CAVE_STEP) / CAVE_STEP;
    }
    std::array<float, NODES * P> rows;
    std::array<float, P * P> lower, upper;
    auto expandLayer = [&](int j, std::array<float, P * P>& plane) {
        const float* g = &grid[static_cast<std::size_t>(j) * NODES * NODES];
        for (int k = 0; k < NODES; ++k)
            for (int x = 0; x < P; ++x)
                rows[k * P + x] = lerp(g[k * NODES + node[x]], g[k * NODES + node[x] + 1], frac[x]);
        for (int z = 0; z < P; ++z) {
            const float* a = &rows[node[z] * P];
            const float* b = a + P;
            for (int x = 0; x < P; ++x) plane[z * P + x] = lerp(a[x], b[x], frac[z]);
        }
    };

    // Trilinear interpolation never drops below the smallest of a box's 8 corners, so
    // boxes whose corners all exceed the threshold hold no tunnel and are skipped.
    constexpr int BOXES = NODES - 1;
    std::array<bool, BOXES * BOXES> active;
    for (int j = CAVE_MIN_Y / CAVE_STEP_Y; j * CAVE_STEP_Y < maxCarve; ++j) {
        const float* g0 = &grid[static_cast<std::size_t>(j) * NODES * NODES];
        const float* g1 = g0 + NODES * NODES;
        bool any = false;
        for (int k = 0; k < BOXES; ++k) {
            for (int i = 0; i < BOXES; ++i) {
                const int n = k * NODES + i;
                const float lo = std::min({g0[n], g0[n + 1], g0[n + NODES], g0[n + NODES + 1],
                                           g1[n], g1[n + 1], g1[n + NODES], g1[n + NODES + 1]});
                active[k * BOXES + i] = lo < CAVE_THRESHOLD;
                any |= active[k * BOXES + i];
            }
        }
        if (!any) continue;

        expandLayer(j, lower);
        expandLayer(j + 1, upper);
        const int yEnd = std::min((j + 1) * CAVE_STEP_Y, maxCarve);
        for (int y = std::max(j * CAVE_STEP_Y, CAVE_MIN_Y); y < yEnd; ++y) {
            const float fy = static_cast<float>(y % CAVE_STEP_Y) / CAVE_STEP_Y;
            VoxelType* layer = &out.cells[ColumnVolume::index(-1, y, -1)];

            for (int k = 0; k < BOXES; ++k) {
                const int z0 = std::max(k * CAVE_STEP - CAVE_STEP, -1) + 1;
                const int z1 = std::min(k * CAVE_STEP, CHUNK_SIZE + 1) + 1;
                for (int i = 0; i < BOXES; ++i) {
                    if (!active[k * BOXES + i]) continue;
                    const int x0 = std::max(i * CAVE_STEP - CAVE_STEP, -1) + 1;
                    const int x1 = std::min(i * CAVE_STEP, CHUNK_SIZE + 1) + 1;
                    for (int z = z0; z < z1; ++z) {
                        for (int c = z * P + x0; c < z * P + x1; ++c) {
                            const float fade = std::clamp(static_cast<float>(top[c] - y) / CAVE_FADE, 0.35f, 1.0f);
                            const bool carve = y < carveTop[c] && lerp(lower[c], upper[c], fy) < CAVE_THRESHOLD * fade;
                            layer[c] = carve ? VoxelTypes::AIR : layer[c];
                        }
                    }
                }
            }
        }
    }
}

} // namespace

void generateColumn(const ColumnKey& key, unsigned openSides, ColumnVolume& out) {
    ZoneScoped;
    const int s = key.cellSize();
    const int half = s >> 1;
    const int ox = key.x * key.sizeVoxels();
    const int oz = key.z * key.sizeVoxels();
    out.chunks = key.chunkCount();
    const int height = out.chunks * CHUNK_SIZE;
    out.cells.resize(static_cast<std::size_t>(height + 2) * P * P);

    // Heights on a coarse grid; bilinear interpolation is indistinguishable at this spacing.
    constexpr int NODES = (CHUNK_SIZE + 2 * HEIGHT_STEP) / HEIGHT_STEP + 1;
    std::array<float, NODES * NODES> heightGrid;
    for (int k = 0; k < NODES; ++k) {
        const float z = static_cast<float>(oz + (k * HEIGHT_STEP - HEIGHT_STEP) * s + half) * VOXEL_SIZE;
        for (int i = 0; i < NODES; ++i) {
            const float x = static_cast<float>(ox + (i * HEIGHT_STEP - HEIGHT_STEP) * s + half) * VOXEL_SIZE;
            heightGrid[k * NODES + i] = terrainHeight(x, z);
        }
    }

    std::array<ColumnInfo, P * P> info;
    int solidBelow = INT_MAX;  // layers below this are stone in every column
    int filledTop = 0;
    for (int z = -1; z <= CHUNK_SIZE; ++z) {
        const int k = (z + HEIGHT_STEP) / HEIGHT_STEP;
        const float fz = static_cast<float>((z + HEIGHT_STEP) % HEIGHT_STEP) / HEIGHT_STEP;
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            const int i = (x + HEIGHT_STEP) / HEIGHT_STEP;
            const float fx = static_cast<float>((x + HEIGHT_STEP) % HEIGHT_STEP) / HEIGHT_STEP;
            const float h = lerp(lerp(heightGrid[k * NODES + i],       heightGrid[k * NODES + i + 1],       fx),
                                 lerp(heightGrid[(k + 1) * NODES + i], heightGrid[(k + 1) * NODES + i + 1], fx), fz);
            const int surface = static_cast<int>(std::floor(h));

            ColumnInfo& col = info[(z + 1) * P + (x + 1)];
            col.underwater = surface < SEA_LEVEL;
            col.top = cellsBelow(surface, s);
            col.stoneTop = cellsBelow(surface - DIRT_DEPTH, s);
            col.waterTop = col.underwater ? std::max(col.top, cellsBelow(SEA_LEVEL, s)) : col.top;
            col.surface = (col.underwater || surface - 1 <= SEA_LEVEL + BEACH) ? VoxelTypes::SAND : VoxelTypes::GRASS;

            solidBelow = std::min(solidBelow, std::min(col.stoneTop, col.top - 1));
            filledTop = std::max(filledTop, col.waterTop);
        }
    }
    out.filledTop = filledTop;

    // Whole layers are memset; only the band where columns differ is filled per cell.
    for (int y = -1; y <= height; ++y) {
        VoxelType* layer = &out.cells[ColumnVolume::index(-1, y, -1)];
        if (y < 0 || y < solidBelow) {
            std::memset(layer, VoxelTypes::STONE, P * P);
        } else if (y >= filledTop) {
            std::memset(layer, VoxelTypes::AIR, P * P);
        } else {
            for (int c = 0; c < P * P; ++c) {
                const ColumnInfo& col = info[c];
                VoxelType t = VoxelTypes::AIR;
                if (y == col.top - 1)       t = col.surface;
                else if (y < col.stoneTop)  t = VoxelTypes::STONE;
                else if (y < col.top)       t = VoxelTypes::DIRT;
                else if (y < col.waterTop)  t = VoxelTypes::WATER;
                layer[c] = t;
            }
        }
    }

    if (key.level <= CAVE_MAX_LEVEL) carveCaves(key, info, out);

    // Open sides read as air, so the mesher emits the border faces.
    for (int y = -1; y <= height; ++y) {
        for (int i = -1; i <= CHUNK_SIZE; ++i) {
            if (openSides & SIDE_NEG_X) out.cells[ColumnVolume::index(-1, y, i)] = VoxelTypes::AIR;
            if (openSides & SIDE_POS_X) out.cells[ColumnVolume::index(CHUNK_SIZE, y, i)] = VoxelTypes::AIR;
            if (openSides & SIDE_NEG_Z) out.cells[ColumnVolume::index(i, y, -1)] = VoxelTypes::AIR;
            if (openSides & SIDE_POS_Z) out.cells[ColumnVolume::index(i, y, CHUNK_SIZE)] = VoxelTypes::AIR;
        }
    }
}

glm::vec3 findSpawnPoint() {
    constexpr float kStep = 6.4f;
    constexpr float kMinLand = (SEA_LEVEL + 20) * VOXEL_SIZE;  // 2 m above sea level
    for (int ring = 0; ring < 400; ++ring) {
        const int samples = std::max(1, ring * 6);
        for (int i = 0; i < samples; ++i) {
            const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(samples);
            const float x = std::cos(angle) * static_cast<float>(ring) * kStep;
            const float z = std::sin(angle) * static_cast<float>(ring) * kStep;
            const float ground = terrainHeight(x, z) * VOXEL_SIZE;
            if (ground > kMinLand) return {x, ground + 3.0f, z};
        }
    }
    return {0.0f, 18.0f, 0.0f};
}
