#include "World/Chunk.hpp"
#include "World/World.hpp"
#include "utils/BitUtils.hpp"
#include <cstring>
#include <cstdint>
#include <array>
#include <unordered_map>

Chunk::Chunk(const glm::ivec3& position) 
    : m_position(position) {
    std::memset(m_voxels.data(), VoxelTypes::AIR, CHUNK_VOLUME);
}

VoxelType Chunk::getVoxel(int x, int y, int z) const {
    if (!isInBounds(x, y, z)) {
        return VoxelTypes::AIR;
    }
    return m_voxels[getIndex(x, y, z)];
}

void Chunk::setVoxel(int x, int y, int z, VoxelType type) {
    if (!isInBounds(x, y, z)) {
        return;
    }
    m_voxels[getIndex(x, y, z)] = type;
    m_isDirty = true;
}

bool Chunk::isInBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_SIZE &&
           z >= 0 && z < CHUNK_SIZE;
}

void Chunk::fillTestData() {
    for (int y = 0; y < CHUNK_SIZE / 2; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                setVoxel(x, y, z, VoxelTypes::STONE);
            }
        }
    }
    
    for (int i = 0; i < 20; i++) {
        int x = rand() % CHUNK_SIZE;
        int y = CHUNK_SIZE / 2 + rand() % (CHUNK_SIZE / 2);
        int z = rand() % CHUNK_SIZE;
        setVoxel(x, y, z, VoxelTypes::GRASS);
    }
    
    m_isDirty = true;
}

// Binary greedy meshing: for each axis build 64x64 columns of uint64_t bitmasks,
// derive face masks with bitshifts, then greedy-merge faces by type into quads.

namespace {

// Emit one merged quad.
// axis: 0=X, 1=Y, 2=Z (face perpendicular to this)
// dir: 0=positive (+axis), 1=negative (-axis)
// slice: bit position along axis (face sits between slice and slice±1)
// u0, v0: origin on face plane; h, w: extent along u/v respectively
// Mapping:
//   axis 0 (X): u=z, v=y → face spans y=[v0,v0+w], z=[u0,u0+h]
//   axis 1 (Y): u=x, v=z → face spans x=[u0,u0+h], z=[v0,v0+w]
//   axis 2 (Z): u=x, v=y → face spans x=[u0,u0+h], y=[v0,v0+w]
inline void emitQuad(std::vector<VoxelVertex>& verts,
                     std::vector<uint32_t>& indices,
                     int axis, int dir, int slice,
                     int u0, int v0, int h, int w,
                     const glm::vec3& color,
                     const glm::vec3& chunkOrigin) {
    glm::vec3 normal(0.0f);
    glm::vec3 p0, p1, p2, p3;

    // face plane coord along axis
    float a = static_cast<float>(dir == 0 ? slice + 1 : slice);

    if (axis == 0) {
        // +X or -X face: YZ plane
        normal = glm::vec3(dir == 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
        float y0 = static_cast<float>(v0);
        float y1 = static_cast<float>(v0 + w);
        float z0 = static_cast<float>(u0);
        float z1 = static_cast<float>(u0 + h);
        if (dir == 0) {
            // CW viewed from +X
            p0 = {a, y0, z0}; p1 = {a, y0, z1}; p2 = {a, y1, z1}; p3 = {a, y1, z0};
        } else {
            p0 = {a, y0, z1}; p1 = {a, y0, z0}; p2 = {a, y1, z0}; p3 = {a, y1, z1};
        }
    } else if (axis == 1) {
        // +Y or -Y face: XZ plane
        normal = glm::vec3(0.0f, dir == 0 ? 1.0f : -1.0f, 0.0f);
        float x0 = static_cast<float>(u0);
        float x1 = static_cast<float>(u0 + h);
        float z0 = static_cast<float>(v0);
        float z1 = static_cast<float>(v0 + w);
        if (dir == 0) {
            p0 = {x0, a, z0}; p1 = {x1, a, z0}; p2 = {x1, a, z1}; p3 = {x0, a, z1};
        } else {
            p0 = {x0, a, z1}; p1 = {x1, a, z1}; p2 = {x1, a, z0}; p3 = {x0, a, z0};
        }
    } else {
        // +Z or -Z face: XY plane
        normal = glm::vec3(0.0f, 0.0f, dir == 0 ? 1.0f : -1.0f);
        float x0 = static_cast<float>(u0);
        float x1 = static_cast<float>(u0 + h);
        float y0 = static_cast<float>(v0);
        float y1 = static_cast<float>(v0 + w);
        if (dir == 0) {
            p0 = {x0, y0, a}; p1 = {x1, y0, a}; p2 = {x1, y1, a}; p3 = {x0, y1, a};
        } else {
            p0 = {x1, y0, a}; p1 = {x0, y0, a}; p2 = {x0, y1, a}; p3 = {x1, y1, a};
        }
    }

    uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back({chunkOrigin + p0, normal, color});
    verts.push_back({chunkOrigin + p1, normal, color});
    verts.push_back({chunkOrigin + p2, normal, color});
    verts.push_back({chunkOrigin + p3, normal, color});
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

} // namespace

// Single meshing pass parameterised by voxel predicates.
// occupancy(v): voxel contributes to this pass (solid or water).
// blocker(v):   neighbouring voxel hides a face (opaque for solid; opaque or water for water).
// neighbors[axis][dir]: neighbour chunk along +/- axis, may be nullptr.
template <typename OccFn, typename BlockFn>
static void meshPass(const std::array<VoxelType, CHUNK_VOLUME>& voxels,
                     const glm::vec3& chunkOrigin,
                     std::vector<VoxelVertex>& outVerts,
                     std::vector<uint32_t>& outIndices,
                     const Chunk* const neighbors[3][2],
                     OccFn occupancy, BlockFn blocker) {
    constexpr int N = CHUNK_SIZE;
    auto vxl = [&](int x, int y, int z) -> VoxelType {
        return voxels[x + y * N + z * N * N];
    };

    // Returns true if the neighbour chunk blocks this boundary face.
    auto neighborBlocks = [&](int axis, int dir, int u, int v) -> bool {
        const Chunk* n = neighbors[axis][dir];
        if (!n) return false;
        int nx, ny, nz;
        if (axis == 0)      { nx = (dir == 0) ? 0 : N - 1; ny = v; nz = u; }
        else if (axis == 1) { nx = u; ny = (dir == 0) ? 0 : N - 1; nz = v; }
        else                { nx = u; ny = v; nz = (dir == 0) ? 0 : N - 1; }
        return blocker(n->getVoxel(nx, ny, nz));
    };

    for (int axis = 0; axis < 3; ++axis) {
        // occ_col: bit = occupancy; block_col: bit = blocker (superset of occ).
        std::array<uint64_t, N * N> occ_col{};
        std::array<uint64_t, N * N> block_col{};

        for (int u = 0; u < N; ++u) {
            for (int v = 0; v < N; ++v) {
                uint64_t occ = 0, blk = 0;
                for (int a = 0; a < N; ++a) {
                    int x, y, z;
                    if (axis == 0)      { x = a; y = v; z = u; }
                    else if (axis == 1) { x = u; y = a; z = v; }
                    else                { x = u; y = v; z = a; }
                    VoxelType t = vxl(x, y, z);
                    if (occupancy(t)) occ |= (1ULL << a);
                    if (blocker(t))   blk |= (1ULL << a);
                }
                occ_col[u * N + v] = occ;
                block_col[u * N + v] = blk;
            }
        }

        for (int dir = 0; dir < 2; ++dir) {
            std::unordered_map<VoxelType, std::array<uint64_t, N * N>> face_col_by_type;

            for (int u = 0; u < N; ++u) {
                for (int v = 0; v < N; ++v) {
                    uint64_t occ = occ_col[u * N + v];
                    uint64_t blk = block_col[u * N + v];
                    // Face exists where voxel (occ=1) but neighbour along dir is not a blocker.
                    uint64_t faces = (dir == 0)
                        ? (occ & ~(blk >> 1))
                        : (occ & ~(blk << 1));
                    // Clear border bit if the neighbour chunk blocks that face.
                    if (faces) {
                        if (dir == 0) {
                            if (neighborBlocks(axis, 0, u, v))
                                faces &= ~(1ULL << (N - 1));
                        } else {
                            if (neighborBlocks(axis, 1, u, v))
                                faces &= ~1ULL;
                        }
                    }
                    if (faces == 0) continue;

                    uint64_t remaining = faces;
                    while (remaining) {
                        int a = ctz64(remaining);
                        remaining &= remaining - 1;

                        int x, y, z;
                        if (axis == 0)      { x = a; y = v; z = u; }
                        else if (axis == 1) { x = u; y = a; z = v; }
                        else                { x = u; y = v; z = a; }

                        VoxelType t = vxl(x, y, z);
                        auto it = face_col_by_type.find(t);
                        if (it == face_col_by_type.end()) {
                            std::array<uint64_t, N * N> arr{};
                            it = face_col_by_type.emplace(t, arr).first;
                        }
                        it->second[u * N + v] |= (1ULL << a);
                    }
                }
            }

            for (auto& kv : face_col_by_type) {
                VoxelType type = kv.first;
                const auto& col = kv.second;
                glm::vec3 color = voxelTypeToColor(type);

                for (int slice = 0; slice < N; ++slice) {
                    std::array<uint64_t, N> rows{};
                    for (int u = 0; u < N; ++u) {
                        uint64_t r = 0;
                        for (int v = 0; v < N; ++v) {
                            if ((col[u * N + v] >> slice) & 1ULL) {
                                r |= (1ULL << v);
                            }
                        }
                        rows[u] = r;
                    }

                    for (int u = 0; u < N; ++u) {
                        while (rows[u]) {
                            int v_start = ctz64(rows[u]);
                            uint64_t shifted = rows[u] >> v_start;
                            int w;
                            if (~shifted == 0ULL) {
                                w = N - v_start;
                            } else {
                                w = ctz64(~shifted);
                                if (w > N - v_start) w = N - v_start;
                            }
                            uint64_t strip = (w >= 64)
                                ? (~0ULL)
                                : (((1ULL << w) - 1ULL) << v_start);

                            int h = 1;
                            for (int u2 = u + 1; u2 < N; ++u2) {
                                if ((rows[u2] & strip) == strip) ++h;
                                else break;
                            }
                            for (int u2 = u; u2 < u + h; ++u2) {
                                rows[u2] &= ~strip;
                            }

                            emitQuad(outVerts, outIndices, axis, dir, slice,
                                     u, v_start, h, w, color, chunkOrigin);
                        }
                    }
                }
            }
        }
    }
}

void Chunk::generateMesh() {
    generateMesh(nullptr);
}

void Chunk::generateMesh(const World* world) {
    if (!m_isDirty) return;

    m_vertices.clear();
    m_indices.clear();
    m_waterVertices.clear();
    m_waterIndices.clear();

    static_assert(CHUNK_SIZE == 64, "Binary meshing requires CHUNK_SIZE==64");

    const glm::vec3 chunkOrigin = glm::vec3(m_position * CHUNK_SIZE);

    // Gather six axis-aligned neighbour chunks.
    const Chunk* neighbors[3][2] = {{nullptr, nullptr}, {nullptr, nullptr}, {nullptr, nullptr}};
    if (world) {
        for (int ax = 0; ax < 3; ++ax) {
            glm::ivec3 dp(0);
            dp[ax] = 1;
            neighbors[ax][0] = world->getChunk(m_position + dp);
            neighbors[ax][1] = world->getChunk(m_position - dp);
        }
    }

    // Solid pass: opaque blocks hidden only by opaque neighbours.
    meshPass(m_voxels, chunkOrigin, m_vertices, m_indices, neighbors,
             [](VoxelType t) { return isVoxelOpaque(t); },
             [](VoxelType t) { return isVoxelOpaque(t); });

    // Water pass: transparent faces culled by both opaque and other water, so only outer
    // shell (top surface + sides exposed to air) is emitted.
    meshPass(m_voxels, chunkOrigin, m_waterVertices, m_waterIndices, neighbors,
             [](VoxelType t) { return isVoxelTransparent(t); },
             [](VoxelType t) { return isVoxelSolid(t); });

    m_isDirty = false;
}
