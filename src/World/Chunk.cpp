#include "World/Chunk.hpp"
#include <cstring>
#include <iostream>

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

void Chunk::generateMesh() {
    if (!m_isDirty) {
        return;
    }

    m_vertices.clear();
    m_indices.clear();
    
    // Всі 6 напрямків: +Y, -Y, +X, -X, +Z, -Z
    const glm::ivec3 directions[6] = {
        {0, 1, 0},   // Top
        {0, -1, 0},  // Bottom
        {1, 0, 0},   // Right
        {-1, 0, 0},  // Left
        {0, 0, 1},   // Front
        {0, 0, -1}   // Back
    };
    
    const glm::vec3 normals[6] = {
        {0, 1, 0},   // Top
        {0, -1, 0},  // Bottom
        {1, 0, 0},   // Right
        {-1, 0, 0},  // Left
        {0, 0, 1},   // Front
        {0, 0, -1}   // Back
    };
    
    // Вершини для кожної грані (clockwise winding)
    const glm::vec3 faceVertices[6][4] = {
        // Top (+Y)
        {{0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}},
        // Bottom (-Y)
        {{0, 0, 1}, {1, 0, 1}, {1, 0, 0}, {0, 0, 0}},
        // Right (+X)
        {{1, 0, 0}, {1, 0, 1}, {1, 1, 1}, {1, 1, 0}},
        // Left (-X)
        {{0, 0, 1}, {0, 0, 0}, {0, 1, 0}, {0, 1, 1}},
        // Front (+Z)
        {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
        // Back (-Z)
        {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}
    };
    
    for (int y = 0; y < CHUNK_SIZE; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                VoxelType voxelType = getVoxel(x, y, z);
                
                if (!isVoxelSolid(voxelType)) {
                    continue;
                }
                
                glm::vec3 worldPos = glm::vec3(m_position * CHUNK_SIZE) + glm::vec3(x, y, z);
                
                // Перевіряємо всі 6 граней
                for (int face = 0; face < 6; face++) {
                    glm::ivec3 neighborPos(x + directions[face].x, 
                                          y + directions[face].y, 
                                          z + directions[face].z);
                    VoxelType neighborType = getVoxel(neighborPos.x, neighborPos.y, neighborPos.z);
                    
                    // Рендеримо грань тільки якщо сусід не solid
                    if (!isVoxelSolid(neighborType)) {
                        uint32_t baseIndex = m_vertices.size();
                        
                        // Додаємо 4 вершини для грані
                        for (int i = 0; i < 4; i++) {
                            VoxelVertex v;
                            v.position = worldPos + faceVertices[face][i];
                            v.normal = normals[face];
                            v.voxelType = voxelType;
                            m_vertices.push_back(v);
                        }
                        
                        // Додаємо 2 трикутники (6 індексів)
                        m_indices.push_back(baseIndex + 0);
                        m_indices.push_back(baseIndex + 1);
                        m_indices.push_back(baseIndex + 2);
                        
                        m_indices.push_back(baseIndex + 0);
                        m_indices.push_back(baseIndex + 2);
                        m_indices.push_back(baseIndex + 3);
                    }
                }
            }
        }
    }
    
    m_isDirty = false;
}
