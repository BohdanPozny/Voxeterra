# Система Воксельного Світу

## Огляд

Світ Voxeterra побудований на основі **чанків** — кубів 16×16×16 вокселів. Світ зберігається як хеш-таблиця чанків, де ключем є позиція чанку.

## Структура модулів

```
world/
├── [[World\|World]]       # Управління колекцією чанків, генерація світу
├── [[Chunk\|Chunk]]       # 16×16×16 вокселів, генерація мешу
├── [[Voxel\|Voxel]]       # Типи вокселів та їх властивості
└── [[WorldRenderer\|WorldRenderer]] # Рендеринг світу (вже описано в Vulkan)
```

## Константи

```cpp
constexpr int CHUNK_SIZE = 16;           // Розмір чанку
constexpr int CHUNK_VOLUME = 4096;       // 16 * 16 * 16
constexpr float VOXEL_SIZE = 1.0f;       // Розмір вокселя в світових одиницях
```

## Координатні системи

### World Coordinates

Глобальні координати в світі (цілі числа):

```cpp
// Позиція вокселя в світі
struct WorldPos {
    int x, y, z;
};

// Приклади:
// (0, 0, 0)     — центр світу
// (16, 0, 0)    — чанк справа від центру
// (-16, 32, 16) — чанк зліва, вгорі, спереду
```

### Chunk Coordinates

Координати чанку (кратні CHUNK_SIZE):

```cpp
// Позиція чанку (у вокселях, кратна 16)
struct ChunkPos {
    int x, y, z;
};

// Конвертація world → chunk
ChunkPos worldToChunk(int worldX, int worldY, int worldZ) {
    return {
        (worldX >= 0 ? worldX : worldX - CHUNK_SIZE + 1) / CHUNK_SIZE,
        (worldY >= 0 ? worldY : worldY - CHUNK_SIZE + 1) / CHUNK_SIZE,
        (worldZ >= 0 ? worldZ : worldZ - CHUNK_SIZE + 1) / CHUNK_SIZE
    };
}

// Конвертація world → local (всередині чанку)
int localX = ((worldX % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
```

### Local Coordinates

Координати всередині чанку (0-15):

```
Chunk (16×16×16):
    Y (height)
    │
    15 ┌────┐
    14 │    │
     … │    │
     1 │    │
     0 └────┘ ───→ X
    / 0 1 … 15
   Z
```

## Типи Вокселів

| Тип     | ID  | Solidity | Колір      | Опис                 |
| ------- | --- | -------- | ---------- | -------------------- |
| `AIR`   | 0   | false    | —          | Порожній простір     |
| `GRASS` | 1   | true     | Зелений    | Трава (земля зверху) |
| `DIRT`  | 2   | true     | Коричневий | Ґрунт                |
| `STONE` | 3   | true     | Сірий      | Камінь               |
| `SAND`  | 4   | true     | Жовтий     | Пісок                |

## Face Culling

При генерації мешу чанку, відображаються тільки грані на кордоні з повітрям:

```cpp
// Для кожного вокселя в чанку
for (int x = 0; x < CHUNK_SIZE; x++) {
    for (int y = 0; y < CHUNK_SIZE; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            Voxel voxel = getVoxel(x, y, z);
            if (!voxel.isSolid()) continue;

            // Перевіряємо 6 сусідів
            // -X face (left)
            if (!getVoxel(x - 1, y, z).isSolid()) addLeftFace(x, y, z);
            // +X face (right)
            if (!getVoxel(x + 1, y, z).isSolid()) addRightFace(x, y, z);
            // -Y face (bottom)
            if (!getVoxel(x, y - 1, z).isSolid()) addBottomFace(x, y, z);
            // +Y face (top)
            if (!getVoxel(x, y + 1, z).isSolid()) addTopFace(x, y, z);
            // -Z face (back)
            if (!getVoxel(x, y, z - 1).isSolid()) addBackFace(x, y, z);
            // +Z face (front)
            if (!getVoxel(x, y, z + 1).isSolid()) addFrontFace(x, y, z);
        }
    }
}
```

## Mesh Data Structure

```cpp
struct VoxelVertex {
    glm::vec3 position;  // Светові координати
    glm::vec3 color;     // Колір вокселя
};

// Чанк генерує:
std::vector<VoxelVertex> vertices;
std::vector<uint32_t> indices;
```

### Кожна грань = 4 вершини, 6 індексів (2 трикутники)

```
Top face (Y+):
    0 ┌────┐ 1
      │  / │
    2 └────┘ 3

Vertices: [0, 1, 2, 0, 2, 3] (трикутники: 0-1-2 та 0-2-3)
```

## Потік даних

```
World::generateTestWorld()
    │
    ├── для кожної позиції чанку:
    │       └── Chunk::fillTestData()
    │               └── генеруємо вокселі (повітря, трава, ґрунт, камінь)
    │
    └── WorldRenderer::createChunkBuffers()
            │
            ├── для кожного чанку:
            │       └── Chunk::generateMesh()
            │               ├── Face culling
            │               ├── Build vertices/indices
            │               └── Return mesh data
            │
            └── Buffer::init() + copyData()
                    └── Upload to GPU
```

## Оптимізації

### 1. Face Culling
- Відображаються тільки зовнішні грані
- Сусідні вокселі не рендеряться між собою
- ~50-90% вокселів невидимі

### 2. Chunk-based Storage
- Тільки завантажені чанки в пам'яті
- Легко додати chunk loading/unloading
- Хеш-таблиця для O(1) доступу

### 3. GPU Buffers
- Chunk mesh завантажується в GPU один раз
- При зміні чанку — перегенерація мешу
- Інстансовий рендеринг для чанків

## Плановані покращення

- [ ] **Greedy Meshing** — об'єднання суміжних граней
- [ ] **Frustum Culling** — відсікання чанків за камерою
- [ ] **LOD (Level of Detail)** — спрощені меші для далеких чанків
- [ ] **Chunk Loading** — асинхронне завантаження/вивантаження
- [ ] **Persistence** — збереження змін чанків на диск

## Дивіться також

- [[World\|World]] — управління колекцією чанків
- [[Chunk\|Chunk]] — 16×16×16 воксельний чанк
- [[Voxel\|Voxel]] — типи вокселів
- [[WorldRenderer\|WorldRenderer]] — рендеринг світу
