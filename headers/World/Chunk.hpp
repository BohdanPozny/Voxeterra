#pragma once

#include "World/Voxel.hpp"
#include <array>
#include <vector>
#include <glm/glm.hpp>

// Розмір чанку (16x16x16 вокселів)
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

// Структура вершини для воксельного меша
struct VoxelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    uint32_t voxelType;  // для текстурування
};

class Chunk {
private:
    glm::ivec3 m_position;  // позиція чанку у світі (в чанках)
    std::array<VoxelType, CHUNK_VOLUME> m_voxels;
    
    bool m_isDirty = true;  // чи потрібно перегенерувати меш
    
    std::vector<VoxelVertex> m_vertices;
    std::vector<uint32_t> m_indices;

    // Допоміжні методи для індексації
    inline int getIndex(int x, int y, int z) const {
        return x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
    }

public:
    Chunk(const glm::ivec3& position);
    ~Chunk() = default;

    // Отримання/встановлення воксела
    VoxelType getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, VoxelType type);
    
    // Перевірка меж
    bool isInBounds(int x, int y, int z) const;
    
    // Генерація меша (жадібний бінарний алгоритм)
    void generateMesh();
    
    // Getters
    const glm::ivec3& getPosition() const { return m_position; }
    const std::vector<VoxelVertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }
    bool isDirty() const { return m_isDirty; }
    void setDirty(bool dirty) { m_isDirty = dirty; }
    
    // Заповнення тестовими даними
    void fillTestData();
};
