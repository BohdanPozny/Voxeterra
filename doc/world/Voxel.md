# Voxel

## Огляд

`Voxel` — базова одиниця воксельного світу. Має тип та колір. Реалізований як проста структура для мінімального використання пам'яті.

## API

```cpp
#pragma once
#include <glm/glm.hpp>

// Типи вокселів
enum class VoxelType : uint8_t {
    AIR = 0,    // Повітря (не твердий)
    GRASS,      // Трава
    DIRT,       // Ґрунт
    STONE,      // Камінь
    SAND,       // Пісок
    WATER,      // Вода
    WOOD,       // Деревина
    LEAVES,     // Листя
    COUNT       // Кількість типів
};

// Властивості вокселя
struct Voxel {
    VoxelType type = VoxelType::AIR;
    
    Voxel() = default;
    explicit Voxel(VoxelType t) : type(t) {}
    
    // Чи є воксель твердим
    bool isSolid() const {
        return type != VoxelType::AIR && type != VoxelType::WATER;
    }
    
    // Чи є воксель прозорим
    bool isTransparent() const {
        return type == VoxelType::AIR || type == VoxelType::WATER || 
               type == VoxelType::LEAVES;
    }
    
    // Отримати колір вокселя
    glm::vec3 getColor() const;
    
    // Отримати текстуру (для майбутньої підтримки текстур)
    int getTextureID() const;
};

// Утиліти для роботи з вокселями
namespace VoxelUtils {
    const char* getVoxelName(VoxelType type);
    bool isOpaque(VoxelType type);
}
```

## Колірна палітра

```cpp
glm::vec3 Voxel::getColor() const {
    switch (type) {
        case VoxelType::AIR:    return glm::vec3(0.0f, 0.0f, 0.0f);      // Невидимий
        case VoxelType::GRASS:  return glm::vec3(0.2f, 0.7f, 0.2f);       // Зелений
        case VoxelType::DIRT:   return glm::vec3(0.55f, 0.27f, 0.07f);   // Коричневий
        case VoxelType::STONE:  return glm::vec3(0.5f, 0.5f, 0.5f);       // Сірий
        case VoxelType::SAND:   return glm::vec3(0.76f, 0.7f, 0.5f);      // Жовтий
        case VoxelType::WATER:  return glm::vec3(0.0f, 0.3f, 0.8f);       // Синій
        case VoxelType::WOOD:   return glm::vec3(0.4f, 0.26f, 0.13f);    // Темно-коричн.
        case VoxelType::LEAVES: return glm::vec3(0.13f, 0.55f, 0.13f);    // Темно-зелений
        default:                return glm::vec3(1.0f, 0.0f, 1.0f);       // Magenta (error)
    }
}
```

## Властивості типів

| Тип | Solid | Прозорий | Колір RGB |
|-----|-------|----------|-----------|
| AIR | ❌ | ✅ | — |
| GRASS | ✅ | ❌ | (51, 179, 51) |
| DIRT | ✅ | ❌ | (140, 70, 18) |
| STONE | ✅ | ❌ | (128, 128, 128) |
| SAND | ✅ | ❌ | (194, 179, 128) |
| WATER | ❌ | ✅ | (0, 77, 204) |
| WOOD | ✅ | ❌ | (102, 66, 33) |
| LEAVES | ✅ | ✅ | (33, 140, 33) |

## Утиліти

```cpp
const char* VoxelUtils::getVoxelName(VoxelType type) {
    switch (type) {
        case VoxelType::AIR:    return "Air";
        case VoxelType::GRASS:  return "Grass";
        case VoxelType::DIRT:   return "Dirt";
        case VoxelType::STONE:  return "Stone";
        case VoxelType::SAND:   return "Sand";
        case VoxelType::WATER:  return "Water";
        case VoxelType::WOOD:   return "Wood";
        case VoxelType::LEAVES: return "Leaves";
        default:                return "Unknown";
    }
}

bool VoxelUtils::isOpaque(VoxelType type) {
    switch (type) {
        case VoxelType::AIR:
        case VoxelType::WATER:
        case VoxelType::LEAVES:
            return false;
        default:
            return true;
    }
}
```

## Оптимізація пам'яті

Поточна реалізація:

```cpp
struct Voxel {
    VoxelType type;  // uint8_t = 1 byte
};
// Розмір: 1 байт
// Chunk: 16×16×16 = 4096 вокселів = 4 KB на чанк
```

Можливі покращення:

```cpp
// Bit packing для типів + метаданих
struct Voxel {
    uint8_t type : 4;      // 16 типів
    uint8_t metadata : 4;  // orientation, light level, etc.
};

// Palette-based storage (для однорідних чанків)
struct Chunk {
    uint8_t paletteIndex[CHUNK_VOLUME];  // Індекс в палітрі
    std::vector<Voxel> palette;           // Унікальні вокселі
};
```

## Плановані розширення

- **Metadata**: орієнтація, рівень освітлення, стан
- **Animated voxels**: вода, лава
- **Connected textures**: з'єднані текстури для кращого вигляду
- **Variants**: різні варіанти каменю, трави

## Дивіться також

- [[Chunk\|Chunk]] — зберігання та меш-генерація
- [[World\|World]] — управління вокселями
