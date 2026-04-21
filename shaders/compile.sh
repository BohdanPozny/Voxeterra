#!/usr/bin/env bash

# Компіляція всіх шейдерів у SPIR-V

# Тестові шейдери
glslc triangle.vert -o triangle.vert.spv
glslc triangle.frag -o triangle.frag.spv

# Воксельні шейдери
glslc voxel.vert -o voxel.vert.spv
glslc voxel.frag -o voxel.frag.spv

# Тестові шейдери
glslc test.vert -o test.vert.spv
glslc test.frag -o test.frag.spv

# Hardcoded трикутник
glslc hardcoded.vert -o hardcoded.vert.spv
glslc hardcoded.frag -o hardcoded.frag.spv

echo "Шейдери скомпільовано успішно"
