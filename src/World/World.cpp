#include "World/World.hpp"
#include <iostream>

Chunk* World::getChunk(const glm::ivec3& position) {
    auto it = m_chunks.find(position);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk* World::createChunk(const glm::ivec3& position) {
    auto chunk = std::make_unique<Chunk>(position);
    Chunk* ptr = chunk.get();
    m_chunks[position] = std::move(chunk);
    return ptr;
}

VoxelType World::getVoxel(int x, int y, int z) {
    // Конвертуємо світові координати в координати чанку
    glm::ivec3 chunkPos(
        x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE,
        y >= 0 ? y / CHUNK_SIZE : (y - CHUNK_SIZE + 1) / CHUNK_SIZE,
        z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE
    );
    
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        return VoxelTypes::AIR;
    }
    
    // Локальні координати всередині чанку
    int localX = x - chunkPos.x * CHUNK_SIZE;
    int localY = y - chunkPos.y * CHUNK_SIZE;
    int localZ = z - chunkPos.z * CHUNK_SIZE;
    
    return chunk->getVoxel(localX, localY, localZ);
}

void World::setVoxel(int x, int y, int z, VoxelType type) {
    glm::ivec3 chunkPos(
        x >= 0 ? x / CHUNK_SIZE : (x - CHUNK_SIZE + 1) / CHUNK_SIZE,
        y >= 0 ? y / CHUNK_SIZE : (y - CHUNK_SIZE + 1) / CHUNK_SIZE,
        z >= 0 ? z / CHUNK_SIZE : (z - CHUNK_SIZE + 1) / CHUNK_SIZE
    );
    
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        chunk = createChunk(chunkPos);
    }
    
    int localX = x - chunkPos.x * CHUNK_SIZE;
    int localY = y - chunkPos.y * CHUNK_SIZE;
    int localZ = z - chunkPos.z * CHUNK_SIZE;
    
    chunk->setVoxel(localX, localY, localZ, type);
}

void World::generateTestWorld() {
    // Створюємо плоский світ 5x1x5 чанків
    const int worldSizeX = 5;
    const int worldSizeZ = 5;
    const int groundLevel = 8;  // висота землі в вокселях
    
    for (int z = 0; z < worldSizeZ; z++) {
        for (int x = 0; x < worldSizeX; x++) {
            glm::ivec3 chunkPos(x, 0, z);
            Chunk* chunk = createChunk(chunkPos);
            
            // Заповнюємо чанк
            for (int cy = 0; cy < CHUNK_SIZE; cy++) {
                for (int cz = 0; cz < CHUNK_SIZE; cz++) {
                    for (int cx = 0; cx < CHUNK_SIZE; cx++) {
                        // Світові координати
                        int worldX = x * CHUNK_SIZE + cx;
                        int worldY = cy;
                        int worldZ = z * CHUNK_SIZE + cz;
                        
                        VoxelType type = VoxelTypes::AIR;
                        
                        if (worldY < groundLevel - 3) {
                            // Глибокі шари - камінь
                            type = VoxelTypes::STONE;
                        } else if (worldY < groundLevel - 1) {
                            // Середні шари - земля
                            type = VoxelTypes::DIRT;
                        } else if (worldY == groundLevel - 1) {
                            // Верхній шар - плями різних типів
                            // Використовуємо простий шум на основі координат
                            int hash = (worldX * 73856093) ^ (worldZ * 19349663);
                            hash = (hash ^ (hash >> 13)) * 1274126177;
                            int value = hash & 0xFF;
                            
                            if (value < 100) {
                                type = VoxelTypes::GRASS;
                            } else if (value < 150) {
                                type = VoxelTypes::DIRT;
                            } else if (value < 200) {
                                type = VoxelTypes::SAND;
                            } else {
                                type = VoxelTypes::STONE;
                            }
                        }
                        
                        chunk->setVoxel(cx, cy, cz, type);
                    }
                }
            }
            
            chunk->setDirty(true);
        }
    }
    
    std::cout << "[World] Generated flat world with " << m_chunks.size() << " chunks" << std::endl;
}

void World::updateChunks() {
    for (auto& [pos, chunk] : m_chunks) {
        if (chunk->isDirty()) {
            chunk->generateMesh();
        }
    }
}
