#pragma once

#include "World/Chunk.hpp"
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

// Hash функція для glm::ivec3 (для unordered_map)
struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        std::size_t h1 = std::hash<int>{}(v.x);
        std::size_t h2 = std::hash<int>{}(v.y);
        std::size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class World {
private:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> m_chunks;

public:
    World() = default;
    ~World() = default;

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Створення/отримання чанку
    Chunk* getChunk(const glm::ivec3& position);
    Chunk* createChunk(const glm::ivec3& position);
    
    // Отримання воксела у світових координатах
    VoxelType getVoxel(int x, int y, int z);
    void setVoxel(int x, int y, int z, VoxelType type);
    
    // Генерація тестового світу
    void generateTestWorld();
    
    // Оновлення всіх dirty чанків
    void updateChunks();
    
    // Getters
    const auto& getChunks() const { return m_chunks; }
};
