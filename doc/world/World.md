# World

## Огляд

`World` клас управляє колекцією чанків та надає доступ до вокселів за світовими координатами. Використовує `std::unordered_map` для зберігання чанків з ключем `ChunkPos`.

## API

```cpp
#pragma once
#include <unordered_map>
#include "World/Chunk.hpp"

// Hash function для ChunkPos
struct ChunkPosHash {
    size_t operator()(const ChunkPos& pos) const {
        return std::hash<int>()(pos.x) ^ 
               (std::hash<int>()(pos.y) << 1) ^ 
               (std::hash<int>()(pos.z) << 2);
    }
};

class World {
public:
    World() = default;

    // Доступ до чанків
    Chunk* getChunk(int x, int y, int z);
    Chunk* getChunk(const ChunkPos& pos);
    Chunk* getOrCreateChunk(int x, int y, int z);

    // Доступ до вокселів за світовими координатами
    Voxel getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, VoxelType type);
    void setVoxel(int x, int y, int z, const Voxel& voxel);

    // Генерація
    void generateTestWorld();
    void generateChunk(Chunk* chunk, const ChunkPos& pos);

    // Оновлення
    void update();
    void markChunkDirty(const ChunkPos& pos);

    // Getters
    const std::unordered_map<ChunkPos, Chunk, ChunkPosHash>& getChunks() const { return m_chunks; }
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash>& getChunks() { return m_chunks; }

private:
    std::unordered_map<ChunkPos, Chunk, ChunkPosHash> m_chunks;
    std::vector<ChunkPos> m_dirtyChunks;

    ChunkPos worldToChunk(int x, int y, int z) const;
    void worldToLocal(int worldX, int worldY, int worldZ, 
                      int& localX, int& localY, int& localZ) const;
};
```

## Конвертація координат

```cpp
ChunkPos World::worldToChunk(int x, int y, int z) const {
    // Ділення з округленням до -∞ для негативних чисел
    return {
        (x >= 0 ? x : x - CHUNK_SIZE + 1) / CHUNK_SIZE,
        (y >= 0 ? y : y - CHUNK_SIZE + 1) / CHUNK_SIZE,
        (z >= 0 ? z : z - CHUNK_SIZE + 1) / CHUNK_SIZE
    };
}

void World::worldToLocal(int worldX, int worldY, int worldZ,
                         int& localX, int& localY, int& localZ) const {
    // Залишок від ділення, завжди позитивний
    localX = ((worldX % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    localY = ((worldY % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    localZ = ((worldZ % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
}
```

## Доступ до вокселів

```cpp
Voxel World::getVoxel(int x, int y, int z) const {
    ChunkPos chunkPos = worldToChunk(x, y, z);
    
    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return Voxel();  // Повітря (default)
    }
    
    int localX, localY, localZ;
    worldToLocal(x, y, z, localX, localY, localZ);
    
    return it->second.getVoxel(localX, localY, localZ);
}

void World::setVoxel(int x, int y, int z, VoxelType type) {
    ChunkPos chunkPos = worldToChunk(x, y, z);
    
    // Створюємо чанк якщо потрібно
    Chunk* chunk = const_cast<World*>(this)->getOrCreateChunk(chunkPos.x, chunkPos.y, chunkPos.z);
    if (!chunk) return;
    
    int localX, localY, localZ;
    worldToLocal(x, y, z, localX, localY, localZ);
    
    chunk->setVoxel(localX, localY, localZ, type);
    markChunkDirty(chunkPos);
}
```

## Генерація світу

### Тестовий світ

```cpp
void World::generateTestWorld() {
    // Створюємо кілька чанків навколо центру
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                ChunkPos pos{x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE};
                Chunk& chunk = m_chunks[pos];
                chunk.setPosition(pos);
                generateChunk(&chunk, pos);
            }
        }
    }
}
```

### Генерація чанку

```cpp
void World::generateChunk(Chunk* chunk, const ChunkPos& pos) {
    // Висота землі залежить від позиції чанку
    int groundHeight = 8 + (pos.x / CHUNK_SIZE) % 3;
    
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                int worldY = pos.y + y;
                
                if (worldY < groundHeight - 3) {
                    // Глибоко — камінь
                    chunk->setVoxel(x, y, z, VoxelType::STONE);
                } else if (worldY < groundHeight) {
                    // Середина — ґрунт
                    chunk->setVoxel(x, y, z, VoxelType::DIRT);
                } else if (worldY == groundHeight) {
                    // Верхній шар — трава
                    chunk->setVoxel(x, y, z, VoxelType::GRASS);
                }
                // Вище — повітря (за замовчуванням)
            }
        }
    }
    
    chunk->markDirty();  // Потрібна регенерація мешу
}
```

## Оновлення чанків

```cpp
void World::update() {
    // Регенеруємо меші для брудних чанків
    for (const auto& pos : m_dirtyChunks) {
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end()) {
            it->second.regenerateMesh();
        }
    }
    m_dirtyChunks.clear();
}

void World::markChunkDirty(const ChunkPos& pos) {
    m_dirtyChunks.push_back(pos);
}
```

## Структура даних

```
World
└── unordered_map<ChunkPos, Chunk>
    │
    ├── key: {-16, 0, 0} ──> Chunk (data: 16×16×16 voxels)
    ├── key: {0, 0, 0}   ──> Chunk (data: 16×16×16 voxels)
    ├── key: {16, 0, 0}  ──> Chunk (data: 16×16×16 voxels)
    └── ...
```

## Плановані покращення

- **Chunk Loading/Streaming**: Завантажувати чанки за потребою
- **Persistence**: Зберігати зміни на диск
- **World Generation**: Perlin/Simplex noise для реалістичного ландшафту
- **Biomes**: Різні типи місцевості
- **Structure Generation**: Дерева, будівлі, печери

## Дивіться також

- [[Chunk\|Chunk]] — 16×16×16 воксельний чанк
- [[Voxel\|Voxel]] — типи вокселів
