# Chunk

## Огляд

`Chunk` представляє блок 16×16×16 вокселів. Відповідає за:
- Зберігання воксельних даних
- Генерацію мешу з face culling
- Відслідковування "брудного" (dirty) стану

## API

```cpp
#pragma once
#include <array>
#include <vector>
#include "World/Voxel.hpp"
#include <glm/glm.hpp>

struct VoxelVertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct ChunkPos {
    int x, y, z;
    
    bool operator==(const ChunkPos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

class Chunk {
public:
    Chunk() = default;

    void setPosition(const ChunkPos& pos);
    ChunkPos getPosition() const { return m_position; }

    // Доступ до вокселів
    Voxel getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, VoxelType type);
    void setVoxel(int x, int y, int z, const Voxel& voxel);

    // Перевірка меж
    bool isInBounds(int x, int y, int z) const;

    // Генерація мешу
    std::pair<std::vector<VoxelVertex>, std::vector<uint32_t>> generateMesh();
    void regenerateMesh();

    // Стан
    bool isDirty() const { return m_dirty; }
    void markDirty() { m_dirty = true; }
    void clearDirty() { m_dirty = false; }

    // Заповнення тестовими даними
    void fillTestData();

private:
    std::array<Voxel, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> m_voxels{};
    ChunkPos m_position{0, 0, 0};
    bool m_dirty = true;

    // Helper для індексації в 1D масиві
    int getIndex(int x, int y, int z) const {
        return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
    }

    // Додавання граней до мешу
    void addTopFace(std::vector<VoxelVertex>& vertices, 
                    std::vector<uint32_t>& indices,
                    int x, int y, int z, const glm::vec3& color);
    void addBottomFace(std::vector<VoxelVertex>& vertices,
                       std::vector<uint32_t>& indices,
                       int x, int y, int z, const glm::vec3& color);
    void addLeftFace(std::vector<VoxelVertex>& vertices,
                     std::vector<uint32_t>& indices,
                     int x, int y, int z, const glm::vec3& color);
    void addRightFace(std::vector<VoxelVertex>& vertices,
                      std::vector<uint32_t>& indices,
                      int x, int y, int z, const glm::vec3& color);
    void addFrontFace(std::vector<VoxelVertex>& vertices,
                      std::vector<uint32_t>& indices,
                      int x, int y, int z, const glm::vec3& color);
    void addBackFace(std::vector<VoxelVertex>& vertices,
                     std::vector<uint32_t>& indices,
                     int x, int y, int z, const glm::vec3& color);
};
```

## Зберігання вокселів

Вокселі зберігаються у вигляді 1D масиву для кращої продуктивності кешу:

```cpp
// 3D → 1D: index = x + y*16 + z*16*16
std::array<Voxel, 4096> m_voxels;  // 16 * 16 * 16

// Доступ
int idx = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
return m_voxels[idx];
```

## Доступ до вокселів

```cpp
Voxel Chunk::getVoxel(int x, int y, int z) const {
    if (!isInBounds(x, y, z)) {
        return Voxel();  // Повітря за межами чанку
    }
    return m_voxels[getIndex(x, y, z)];
}

void Chunk::setVoxel(int x, int y, int z, VoxelType type) {
    if (!isInBounds(x, y, z)) return;
    
    m_voxels[getIndex(x, y, z)] = Voxel(type);
    m_dirty = true;  // Потрібна регенерація мешу
}

bool Chunk::isInBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_SIZE &&
           z >= 0 && z < CHUNK_SIZE;
}
```

## Генерація мешу

### Face Culling Algorithm

```cpp
std::pair<std::vector<VoxelVertex>, std::vector<uint32_t>> Chunk::generateMesh() {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                Voxel voxel = getVoxel(x, y, z);
                if (!voxel.isSolid()) continue;

                glm::vec3 color = voxel.getColor();
                float worldX = m_position.x + x;
                float worldY = m_position.y + y;
                float worldZ = m_position.z + z;

                // +Y (Top)
                if (y == CHUNK_SIZE - 1 || !getVoxel(x, y + 1, z).isSolid()) {
                    addTopFace(vertices, indices, worldX, worldY, worldZ, color);
                }
                // -Y (Bottom)
                if (y == 0 || !getVoxel(x, y - 1, z).isSolid()) {
                    addBottomFace(vertices, indices, worldX, worldY, worldZ, color);
                }
                // -X (Left)
                if (x == 0 || !getVoxel(x - 1, y, z).isSolid()) {
                    addLeftFace(vertices, indices, worldX, worldY, worldZ, color);
                }
                // +X (Right)
                if (x == CHUNK_SIZE - 1 || !getVoxel(x + 1, y, z).isSolid()) {
                    addRightFace(vertices, indices, worldX, worldY, worldZ, color);
                }
                // -Z (Back)
                if (z == 0 || !getVoxel(x, y, z - 1).isSolid()) {
                    addBackFace(vertices, indices, worldX, worldY, worldZ, color);
                }
                // +Z (Front)
                if (z == CHUNK_SIZE - 1 || !getVoxel(x, y, z + 1).isSolid()) {
                    addFrontFace(vertices, indices, worldX, worldY, worldZ, color);
                }
            }
        }
    }

    m_dirty = false;
    return {vertices, indices};
}
```

## Додавання граней

### Top Face (+Y)

```cpp
void Chunk::addTopFace(std::vector<VoxelVertex>& vertices,
                       std::vector<uint32_t>& indices,
                       int x, int y, int z, const glm::vec3& color) {
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    // 4 вершини (проти годинникової стрілки, зверху)
    vertices.push_back({glm::vec3(x,     y + 1, z),     color});  // 0
    vertices.push_back({glm::vec3(x + 1, y + 1, z),     color});  // 1
    vertices.push_back({glm::vec3(x + 1, y + 1, z + 1), color});  // 2
    vertices.push_back({glm::vec3(x,     y + 1, z + 1), color});  // 3

    // 2 трикутники: 0-1-2 та 0-2-3
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);
}
```

### Front Face (+Z)

```cpp
void Chunk::addFrontFace(std::vector<VoxelVertex>& vertices,
                         std::vector<uint32_t>& indices,
                         int x, int y, int z, const glm::vec3& color) {
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    vertices.push_back({glm::vec3(x,     y,     z + 1), color});  // 0
    vertices.push_back({glm::vec3(x + 1, y,     z + 1), color});  // 1
    vertices.push_back({glm::vec3(x + 1, y + 1, z + 1), color});  // 2
    vertices.push_back({glm::vec3(x,     y + 1, z + 1), color});  // 3

    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);
}
```

## Тестові дані

```cpp
void Chunk::fillTestData() {
    // Простий шаровий чанк
    for (int y = 0; y < CHUNK_SIZE; y++) {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                if (y < 4) {
                    setVoxel(x, y, z, VoxelType::STONE);
                } else if (y < 7) {
                    setVoxel(x, y, z, VoxelType::DIRT);
                } else if (y == 7) {
                    setVoxel(x, y, z, VoxelType::GRASS);
                }
            }
        }
    }
}
```

## Дивіться також

- [[World\|World]] — колекція чанків
- [[Voxel\|Voxel]] — типи та властивості вокселів
