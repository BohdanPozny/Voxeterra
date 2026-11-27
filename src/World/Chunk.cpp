#include "World/Chunk.hpp"
#include "World/World.hpp"
#include "utils/BitUtils.hpp"
#include "utils/Profiler.hpp"
#include <cstring>
#include <cstdint>
#include <cmath>
#include <array>
#include <vector>

Chunk::Chunk(const glm::ivec3& position) 
    : m_position(position) {
    std::memset(m_voxels.data(), VoxelTypes::AIR, CHUNK_VOLUME);
}

glm::ivec3 Chunk::worldToChunkCoord(const glm::ivec3& worldBlock) {
    // Floor-division by CHUNK_SIZE so negative coordinates land in the correct chunk.
    auto floorDiv = [](int a, int b) -> int {
        return a >= 0 ? a / b : (a - b + 1) / b;
    };
    return {
        floorDiv(worldBlock.x, CHUNK_SIZE),
        floorDiv(worldBlock.y, CHUNK_SIZE),
        floorDiv(worldBlock.z, CHUNK_SIZE),
    };
}

glm::ivec3 Chunk::worldToChunkCoord(const glm::vec3& worldPos) {
    return worldToChunkCoord(glm::ivec3(
        static_cast<int>(std::floor(worldPos.x)),
        static_cast<int>(std::floor(worldPos.y)),
        static_cast<int>(std::floor(worldPos.z))));
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
    VoxelType& slot = m_voxels[getIndex(x, y, z)];
    if (slot == type) return;
    countVoxel(slot, -1);
    countVoxel(type, +1);
    slot = type;
    m_isDirty = true;
}

void Chunk::countVoxel(VoxelType type, int delta) {
    if (isVoxelTransparent(type))  m_waterCount += delta;
    else if (isVoxelOpaque(type))  m_opaqueCount += delta;
}

bool Chunk::isInBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_SIZE &&
           z >= 0 && z < CHUNK_SIZE;
}

void Chunk::clearMesh() {
    std::vector<VoxelVertex>().swap(m_vertices);
    std::vector<uint32_t>().swap(m_indices);
    std::vector<VoxelVertex>().swap(m_waterVertices);
    std::vector<uint32_t>().swap(m_waterIndices);
}

// Binary greedy meshing: build 64x64 columns of uint64_t bitmasks per axis in one pass,
// derive face masks with bitshifts, scatter faces into per-type planes, then greedy-merge.

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
            p0 = {x0, y0, a}; p1 = {x0, y1, a}; p2 = {x1, y1, a}; p3 = {x1, y0, a};
        } else {
            p0 = {x1, y0, a}; p1 = {x1, y1, a}; p2 = {x0, y1, a}; p3 = {x0, y0, a};
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

namespace {

constexpr int N = CHUNK_SIZE;
using Columns = std::array<uint64_t, N * N>;

// Column layout per axis, index u * N + v, bit a = slice along the axis:
//   axis 0: a=x, u=z, v=y    axis 1: a=y, u=x, v=z    axis 2: a=z, u=x, v=y

inline VoxelType voxelAt(const std::array<VoxelType, CHUNK_VOLUME>& voxels, int axis, int a, int u, int v) {
    int x, y, z;
    if (axis == 0)      { x = a; y = v; z = u; }
    else if (axis == 1) { x = u; y = a; z = v; }
    else                { x = u; y = v; z = a; }
    return voxels[x + y * N + z * N * N];
}

struct MeshScratch {
    Columns opaque[3];
    Columns water[3];
    Columns solid[3];

    // Face planes per voxel type, index slice * N + u, bit v. Greedy merging
    // consumes every bit, so planes are always zero between uses.
    std::vector<Columns> planes;
    std::array<int, 256> slotOf;
    std::vector<VoxelType> slotType;
    std::vector<uint64_t> sliceMask;
};

MeshScratch& scratch() {
    static thread_local MeshScratch s;
    return s;
}

void buildColumns(const std::array<VoxelType, CHUNK_VOLUME>& voxels, MeshScratch& s, bool hasWater) {
    ZoneScoped;
    for (int axis = 0; axis < 3; ++axis) {
        s.opaque[axis].fill(0);
        s.water[axis].fill(0);
    }
    for (int z = 0; z < N; ++z) {
        for (int y = 0; y < N; ++y) {
            const VoxelType* row = &voxels[y * N + z * N * N];
            uint64_t opaqueRow = 0, waterRow = 0;
            for (int x = 0; x < N; ++x) {
                const VoxelType t = row[x];
                if (t == VoxelTypes::AIR) continue;
                Columns* cols;
                if (isVoxelTransparent(t)) { waterRow  |= 1ULL << x; cols = s.water; }
                else                       { opaqueRow |= 1ULL << x; cols = s.opaque; }
                cols[1][x * N + z] |= 1ULL << y;
                cols[2][x * N + y] |= 1ULL << z;
            }
            s.opaque[0][z * N + y] = opaqueRow;
            s.water[0][z * N + y]  = waterRow;
        }
    }
    if (hasWater) {
        for (int axis = 0; axis < 3; ++axis) {
            for (int i = 0; i < N * N; ++i) s.solid[axis][i] = s.opaque[axis][i] | s.water[axis][i];
        }
    }
}

// occ: voxels that emit faces in this pass; blk: voxels that hide a neighbouring face.
template <typename BlockFn>
void meshPass(const std::array<VoxelType, CHUNK_VOLUME>& voxels,
              const glm::vec3& chunkOrigin,
              std::vector<VoxelVertex>& outVerts,
              std::vector<uint32_t>& outIndices,
              const Chunk* const neighbors[3][2],
              const Columns occ[3], const Columns blk[3],
              MeshScratch& s, BlockFn blocker) {
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
        for (int dir = 0; dir < 2; ++dir) {
            s.slotType.clear();
            s.sliceMask.clear();
            s.slotOf.fill(-1);

            for (int u = 0; u < N; ++u) {
                for (int v = 0; v < N; ++v) {
                    const uint64_t o = occ[axis][u * N + v];
                    if (o == 0) continue;
                    const uint64_t b = blk[axis][u * N + v];
                    uint64_t faces = (dir == 0) ? (o & ~(b >> 1)) : (o & ~(b << 1));

                    const uint64_t borderBit = (dir == 0) ? (1ULL << (N - 1)) : 1ULL;
                    if ((faces & borderBit) && neighborBlocks(axis, dir, u, v)) faces &= ~borderBit;

                    while (faces) {
                        const int a = ctz64(faces);
                        faces &= faces - 1;

                        const VoxelType t = voxelAt(voxels, axis, a, u, v);
                        int slot = s.slotOf[t];
                        if (slot < 0) {
                            slot = static_cast<int>(s.slotType.size());
                            s.slotOf[t] = slot;
                            s.slotType.push_back(t);
                            s.sliceMask.push_back(0);
                            if (s.planes.size() < s.slotType.size()) s.planes.emplace_back().fill(0);
                        }
                        s.planes[slot][a * N + u] |= 1ULL << v;
                        s.sliceMask[slot] |= 1ULL << a;
                    }
                }
            }

            for (size_t slot = 0; slot < s.slotType.size(); ++slot) {
                const glm::vec3 color = voxelTypeToColor(s.slotType[slot]);
                uint64_t slices = s.sliceMask[slot];
                while (slices) {
                    const int slice = ctz64(slices);
                    slices &= slices - 1;
                    uint64_t* rows = &s.planes[slot][slice * N];

                    for (int u = 0; u < N; ++u) {
                        while (rows[u]) {
                            const int vStart = ctz64(rows[u]);
                            const uint64_t shifted = rows[u] >> vStart;
                            const int w = (~shifted == 0ULL) ? N - vStart : ctz64(~shifted);
                            const uint64_t strip = (w >= 64) ? ~0ULL : (((1ULL << w) - 1ULL) << vStart);

                            int h = 1;
                            while (u + h < N && (rows[u + h] & strip) == strip) ++h;
                            for (int u2 = u; u2 < u + h; ++u2) rows[u2] &= ~strip;

                            emitQuad(outVerts, outIndices, axis, dir, slice,
                                     u, vStart, h, w, color, chunkOrigin);
                        }
                    }
                }
            }
        }
    }
}

} // namespace

void Chunk::generateMesh(const World* world) {
    ZoneScoped;
    if (!m_isDirty) return;

    m_vertices.clear();
    m_indices.clear();
    m_waterVertices.clear();
    m_waterIndices.clear();
    m_isDirty = false;

    static_assert(CHUNK_SIZE == 64, "Binary meshing requires CHUNK_SIZE==64");

    if (m_opaqueCount == 0 && m_waterCount == 0) return;

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

    MeshScratch& s = scratch();
    buildColumns(m_voxels, s, m_waterCount > 0);

    // Solid pass: opaque blocks hidden only by opaque neighbours.
    if (m_opaqueCount > 0) {
        ZoneScopedN("Mesh solid");
        meshPass(m_voxels, chunkOrigin, m_vertices, m_indices, neighbors, s.opaque, s.opaque, s,
                 [](VoxelType t) { return isVoxelOpaque(t); });
    }

    // Water pass: faces culled by opaque blocks and other water, so only the outer shell is emitted.
    if (m_waterCount > 0) {
        ZoneScopedN("Mesh water");
        meshPass(m_voxels, chunkOrigin, m_waterVertices, m_waterIndices, neighbors, s.water, s.solid, s,
                 [](VoxelType t) { return isVoxelSolid(t); });
    }
}
