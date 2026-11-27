#include "World/World.hpp"
#include "utils/Profiler.hpp"
#include <cmath>
#include <cstdint>
#include <algorithm>

Chunk* World::getChunk(const glm::ivec3& position) {
    auto it = m_chunks.find(position);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Chunk* World::getChunk(const glm::ivec3& position) const {
    auto it = m_chunks.find(position);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk* World::createChunk(const glm::ivec3& position) {
    auto chunk = std::make_unique<Chunk>(position);
    Chunk* ptr = chunk.get();
    m_chunks[position] = std::move(chunk);
    return ptr;
}

VoxelType World::getVoxel(int x, int y, int z) {
    // World -> chunk coordinate (floor division that also handles negatives).
    glm::ivec3 chunkPos(
        x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE,
        y >= 0 ? y / CHUNK_SIZE : (y - CHUNK_SIZE + 1) / CHUNK_SIZE,
        z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE
    );
    
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        return VoxelTypes::AIR;
    }
    
    // Offset within the chunk.
    int localX = x - chunkPos.x * CHUNK_SIZE;
    int localY = y - chunkPos.y * CHUNK_SIZE;
    int localZ = z - chunkPos.z * CHUNK_SIZE;
    
    return chunk->getVoxel(localX, localY, localZ);
}

void World::setVoxel(int x, int y, int z, VoxelType type) {
    glm::ivec3 chunkPos(
        x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE,
        y >= 0 ? y / CHUNK_SIZE : (y - CHUNK_SIZE + 1) / CHUNK_SIZE,
        z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE
    );
    
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        chunk = createChunk(chunkPos);
    }
    
    int localX = x - chunkPos.x * CHUNK_SIZE;
    int localY = y - chunkPos.y * CHUNK_SIZE;
    int localZ = z - chunkPos.z * CHUNK_SIZE;
    
    chunk->setVoxel(localX, localY, localZ, type);
}

// Deterministic fBm value-noise used for terrain heightmaps.
namespace {

inline uint32_t hash2i(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Scales the hash output into [0, 1).
inline float hashToUnit(int x, int y) {
    return (hash2i(x, y) & 0xFFFFFF) / static_cast<float>(0x1000000);
}

inline float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

// Bilinearly interpolated value noise with smoothstep.
float valueNoise2D(float x, float y) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float fx = x - xi;
    float fy = y - yi;

    float a = hashToUnit(xi,     yi);
    float b = hashToUnit(xi + 1, yi);
    float c = hashToUnit(xi,     yi + 1);
    float d = hashToUnit(xi + 1, yi + 1);

    float sx = smoothstep(fx);
    float sy = smoothstep(fy);
    float ab = a + (b - a) * sx;
    float cd = c + (d - c) * sx;
    return ab + (cd - ab) * sy;
}

// Fractional Brownian motion summation of value noise.
float fbm2D(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) {
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise2D(x * freq, y * freq);
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / norm;
}

} // namespace

void World::generateChunkData(Chunk* chunk) {
    ZoneScoped;
    if (!chunk) return;

    const glm::ivec3& cpos = chunk->getPosition();

    // Terrain tunables in metres; the world column is 4 chunks (25.6 m) tall.
    constexpr float SEA_LEVEL_M   = 6.0f;
    constexpr float BASE_HEIGHT_M = 4.5f;
    constexpr float HILL_AMP_M    = 3.0f;    // small-scale hills
    constexpr float MOUNT_AMP_M   = 12.0f;   // gated mountains
    constexpr float CONT_AMP_M    = 3.0f;    // continental drift
    constexpr float BEACH_M       = 0.3f;

    constexpr float CONT_SCALE  = 1.0f / 200.0f;  // very large features
    constexpr float HILL_SCALE  = 1.0f / 25.0f;   // small hills
    constexpr float MOUNT_SCALE = 1.0f / 80.0f;   // mountain ranges

    constexpr int SEA_LEVEL = static_cast<int>(SEA_LEVEL_M / VOXEL_SIZE);
    constexpr int BEACH     = static_cast<int>(BEACH_M / VOXEL_SIZE);
    constexpr int DIRT_DEPTH = 6;

    int chunkMinY = cpos.y * CHUNK_SIZE;

    // Early-out when the chunk is entirely above any possible terrain column.
    constexpr int maxPossibleTop = static_cast<int>(
        (BASE_HEIGHT_M + HILL_AMP_M + MOUNT_AMP_M + CONT_AMP_M) / VOXEL_SIZE);
    if (chunkMinY > std::max(maxPossibleTop, SEA_LEVEL)) {
        chunk->setDirty(true);
        return;
    }

    auto computeHeight = [&](int worldX, int worldZ) -> int {
        const float x = worldX * VOXEL_SIZE;
        const float z = worldZ * VOXEL_SIZE;
        // Continental drift: [-1, 1].
        float c = fbm2D(x * CONT_SCALE, z * CONT_SCALE, 4) * 2.0f - 1.0f;
        // Rolling hills: [0, 1].
        float h = fbm2D(x * HILL_SCALE + 17.3f, z * HILL_SCALE + 41.1f, 4);
        // Mountains: gated by the continental value so oceans stay flat.
        float m = fbm2D(x * MOUNT_SCALE + 123.0f, z * MOUNT_SCALE + 77.0f, 5);
        m = std::pow(m, 2.5f);
        float mountainMask = std::max(0.0f, c - 0.1f) * 1.3f;
        if (mountainMask > 1.0f) mountainMask = 1.0f;

        float height = BASE_HEIGHT_M
                     + c * CONT_AMP_M
                     + h * HILL_AMP_M
                     + m * mountainMask * MOUNT_AMP_M;
        return static_cast<int>(height / VOXEL_SIZE);
    };

    for (int cz = 0; cz < CHUNK_SIZE; ++cz) {
        for (int cx = 0; cx < CHUNK_SIZE; ++cx) {
            int worldX = cpos.x * CHUNK_SIZE + cx;
            int worldZ = cpos.z * CHUNK_SIZE + cz;

            int terrainHeight = computeHeight(worldX, worldZ);

            // Biome-ish selection based on surface height vs. sea level.
            bool underwater = terrainHeight < SEA_LEVEL;

            for (int cy = 0; cy < CHUNK_SIZE; ++cy) {
                int worldY = chunkMinY + cy;
                VoxelType type = VoxelTypes::AIR;

                if (worldY < terrainHeight - DIRT_DEPTH) {
                    type = VoxelTypes::STONE;
                } else if (worldY < terrainHeight - 1) {
                    type = VoxelTypes::DIRT;
                } else if (worldY < terrainHeight) {
                    // Top layer: sand on beaches/underwater, grass otherwise.
                    type = (underwater || worldY <= SEA_LEVEL + BEACH)
                         ? VoxelTypes::SAND
                         : VoxelTypes::GRASS;
                } else if (worldY < SEA_LEVEL && underwater) {
                    // Fill the air column above submerged terrain with water.
                    type = VoxelTypes::WATER;
                }

                if (type != VoxelTypes::AIR) {
                    chunk->setVoxel(cx, cy, cz, type);
                }
            }
        }
    }

    chunk->setDirty(true);
}

void World::removeChunk(const glm::ivec3& position) {
    m_chunks.erase(position);
}
