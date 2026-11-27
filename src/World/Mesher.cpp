#include "World/Mesher.hpp"
#include "utils/BitUtils.hpp"
#include "utils/Profiler.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstring>

// Binary greedy meshing: build 64x64 columns of uint64_t bitmasks per axis in one pass,
// derive face masks with bitshifts, scatter faces into per-type planes, then greedy-merge.

namespace {

static_assert(CHUNK_SIZE == 64, "Binary meshing requires CHUNK_SIZE==64");

constexpr int N = CHUNK_SIZE;
using Columns = std::array<uint64_t, N * N>;

inline uint32_t packVertex(int x, int y, int z, int normal, VoxelType type) {
    return static_cast<uint32_t>(x) | static_cast<uint32_t>(z) << 7 | static_cast<uint32_t>(y) << 14 |
           static_cast<uint32_t>(normal) << 23 | static_cast<uint32_t>(type) << 26;
}

// Emit one merged quad.
// axis: 0=X, 1=Y, 2=Z (face perpendicular to this)
// dir: 0=positive (+axis), 1=negative (-axis)
// slice: bit position along axis (face sits between slice and slice±1)
// u0, v0: origin on face plane; h, w: extent along u/v respectively
// Mapping:
//   axis 0 (X): u=z, v=y → face spans y=[v0,v0+w], z=[u0,u0+h]
//   axis 1 (Y): u=x, v=z → face spans x=[u0,u0+h], z=[v0,v0+w]
//   axis 2 (Z): u=x, v=y → face spans x=[u0,u0+h], y=[v0,v0+w]
inline void emitQuad(std::vector<uint32_t>& out, ColumnMesh& mesh,
                     int axis, int dir, int slice, int u0, int v0, int h, int w,
                     VoxelType type, int yBase) {
    const int a = dir == 0 ? slice + 1 : slice;
    int p[4][3];

    if (axis == 0) {
        const int y0 = v0, y1 = v0 + w, z0 = u0, z1 = u0 + h;
        if (dir == 0) {
            // CW viewed from +X
            p[0][0] = a; p[0][1] = y0; p[0][2] = z0;  p[1][0] = a; p[1][1] = y0; p[1][2] = z1;
            p[2][0] = a; p[2][1] = y1; p[2][2] = z1;  p[3][0] = a; p[3][1] = y1; p[3][2] = z0;
        } else {
            p[0][0] = a; p[0][1] = y0; p[0][2] = z1;  p[1][0] = a; p[1][1] = y0; p[1][2] = z0;
            p[2][0] = a; p[2][1] = y1; p[2][2] = z0;  p[3][0] = a; p[3][1] = y1; p[3][2] = z1;
        }
    } else if (axis == 1) {
        const int x0 = u0, x1 = u0 + h, z0 = v0, z1 = v0 + w;
        if (dir == 0) {
            p[0][0] = x0; p[0][1] = a; p[0][2] = z0;  p[1][0] = x1; p[1][1] = a; p[1][2] = z0;
            p[2][0] = x1; p[2][1] = a; p[2][2] = z1;  p[3][0] = x0; p[3][1] = a; p[3][2] = z1;
        } else {
            p[0][0] = x0; p[0][1] = a; p[0][2] = z1;  p[1][0] = x1; p[1][1] = a; p[1][2] = z1;
            p[2][0] = x1; p[2][1] = a; p[2][2] = z0;  p[3][0] = x0; p[3][1] = a; p[3][2] = z0;
        }
    } else {
        const int x0 = u0, x1 = u0 + h, y0 = v0, y1 = v0 + w;
        if (dir == 0) {
            p[0][0] = x0; p[0][1] = y0; p[0][2] = a;  p[1][0] = x0; p[1][1] = y1; p[1][2] = a;
            p[2][0] = x1; p[2][1] = y1; p[2][2] = a;  p[3][0] = x1; p[3][1] = y0; p[3][2] = a;
        } else {
            p[0][0] = x1; p[0][1] = y0; p[0][2] = a;  p[1][0] = x1; p[1][1] = y1; p[1][2] = a;
            p[2][0] = x0; p[2][1] = y1; p[2][2] = a;  p[3][0] = x0; p[3][1] = y0; p[3][2] = a;
        }
    }

    const int normal = axis * 2 + dir;
    for (const auto& v : p) {
        const int y = v[1] + yBase;
        out.push_back(packVertex(v[0], y, v[2], normal, type));
        mesh.minY = std::min(mesh.minY, y);
        mesh.maxY = std::max(mesh.maxY, y);
    }
}

// Column layout per axis, index u * N + v, bit a = slice along the axis:
//   axis 0: a=x, u=z, v=y    axis 1: a=y, u=x, v=z    axis 2: a=z, u=x, v=y
// Coordinates are chunk-local; yBase selects the chunk inside the padded column.
inline VoxelType voxelAt(const ColumnVolume& vol, int yBase, int axis, int a, int u, int v) {
    int x, y, z;
    if (axis == 0)      { x = a; y = v; z = u; }
    else if (axis == 1) { x = u; y = a; z = v; }
    else                { x = u; y = v; z = a; }
    return vol.cells[ColumnVolume::index(x, y + yBase, z)];
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

// Bit i set where byte i of w is non-zero.
inline uint64_t nonZeroBytes(uint64_t w) {
    constexpr uint64_t kLow7 = 0x7F7F7F7F7F7F7F7FULL;
    const uint64_t high = (((w & kLow7) + kLow7) | w) & ~kLow7;  // top bit of each non-zero byte
    return ((high >> 7) * 0x0102040810204080ULL) >> 56;
}

// Transposes a 64x64 bit matrix in place: bit j of word i moves to bit i of word j.
void transpose64(uint64_t a[64]) {
    uint64_t mask = 0x00000000FFFFFFFFULL;
    for (int j = 32; j != 0; j >>= 1, mask ^= mask << j) {
        for (int k = 0; k < 64; k = ((k | j) + 1) & ~j) {
            const uint64_t t = ((a[k] >> j) ^ a[k | j]) & mask;
            a[k] ^= t << j;
            a[k | j] ^= t;
        }
    }
}

// Derives the Y and Z column sets from the X rows (index z * N + y, bit x).
void transposeColumns(Columns cols[3]) {
    uint64_t m[64];
    for (int z = 0; z < N; ++z) {
        for (int y = 0; y < N; ++y) m[y] = cols[0][z * N + y];
        transpose64(m);
        for (int x = 0; x < N; ++x) cols[1][x * N + z] = m[x];
    }
    for (int y = 0; y < N; ++y) {
        for (int z = 0; z < N; ++z) m[z] = cols[0][z * N + y];
        transpose64(m);
        for (int x = 0; x < N; ++x) cols[2][x * N + y] = m[x];
    }
}

// Returns {any opaque, any water} for the chunk starting at yBase.
std::pair<bool, bool> buildColumns(const ColumnVolume& vol, int yBase, MeshScratch& s) {
    ZoneScoped;
    static_assert(VoxelTypes::WATER == 5, "water byte pattern below");
    constexpr uint64_t kWater = 0x0505050505050505ULL;

    // X rows are classified 8 voxels at a time; the other axes are bit transposes of them.
    uint64_t anyOpaque = 0, anyWater = 0;
    for (int z = 0; z < N; ++z) {
        for (int y = 0; y < N; ++y) {
            const VoxelType* row = &vol.cells[ColumnVolume::index(0, y + yBase, z)];
            uint64_t opaqueRow = 0, waterRow = 0;
            for (int k = 0; k < N / 8; ++k) {
                uint64_t w;
                std::memcpy(&w, row + k * 8, sizeof(w));
                const uint64_t solid = nonZeroBytes(w);
                const uint64_t water = ~nonZeroBytes(w ^ kWater) & 0xFF;
                opaqueRow |= (solid & ~water) << (k * 8);
                waterRow  |= water << (k * 8);
            }
            s.opaque[0][z * N + y] = opaqueRow;
            s.water[0][z * N + y]  = waterRow;
            anyOpaque |= opaqueRow;
            anyWater  |= waterRow;
        }
    }

    if (anyOpaque) transposeColumns(s.opaque);
    if (anyWater) {
        transposeColumns(s.water);
        for (int axis = 0; axis < 3; ++axis) {
            for (int i = 0; i < N * N; ++i) s.solid[axis][i] = s.opaque[axis][i] | s.water[axis][i];
        }
    }
    return {anyOpaque != 0, anyWater != 0};
}

// occ: voxels that emit faces in this pass; blk: voxels that hide a neighbouring face.
// closeSides treats the X/Z borders as covered (water never needs side faces there).
template <typename BlockFn>
void meshPass(const ColumnVolume& vol, int yBase, std::vector<uint32_t>& out, ColumnMesh& mesh,
              const Columns occ[3], const Columns blk[3], bool closeSides,
              MeshScratch& s, BlockFn blocker) {
    auto neighborBlocks = [&](int axis, int dir, int u, int v) -> bool {
        if (closeSides && axis != 1) return true;
        const int edge = (dir == 0) ? N : -1;
        int x, y, z;
        if (axis == 0)      { x = edge; y = v; z = u; }
        else if (axis == 1) { x = u; y = edge; z = v; }
        else                { x = u; y = v; z = edge; }
        return blocker(vol.cells[ColumnVolume::index(x, y + yBase, z)]);
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

                        const VoxelType t = voxelAt(vol, yBase, axis, a, u, v);
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
                const VoxelType type = s.slotType[slot];
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

                            emitQuad(out, mesh, axis, dir, slice, u, vStart, h, w, type, yBase);
                        }
                    }
                }
            }
        }
    }
}

} // namespace

void meshColumn(const ColumnVolume& volume, ColumnMesh& out) {
    ZoneScoped;
    out.solid.clear();
    out.water.clear();
    out.minY = INT_MAX;
    out.maxY = INT_MIN;

    MeshScratch& s = scratch();
    for (int c = 0; c < volume.chunks; ++c) {
        const int yBase = c * N;
        if (yBase >= volume.filledTop) break;

        const auto [hasOpaque, hasWater] = buildColumns(volume, yBase, s);

        // Solid pass: opaque blocks hidden only by opaque neighbours.
        if (hasOpaque) {
            ZoneScopedN("Mesh solid");
            meshPass(volume, yBase, out.solid, out, s.opaque, s.opaque, false, s,
                     [](VoxelType t) { return isVoxelOpaque(t); });
        }

        // Water pass: faces culled by opaque blocks and other water, so only the outer shell is emitted.
        if (hasWater) {
            ZoneScopedN("Mesh water");
            meshPass(volume, yBase, out.water, out, s.water, s.solid, true, s,
                     [](VoxelType t) { return isVoxelSolid(t); });
        }
    }
    if (out.minY > out.maxY) out.minY = out.maxY = 0;
}
