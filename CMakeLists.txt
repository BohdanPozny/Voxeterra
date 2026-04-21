cmake_minimum_required(VERSION 3.16)

project(Voxterra VERSION 0.1 LANGUAGES CXX)

# Export compile_commands.json for clangd / IDE integration.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Place the executable in the top-level build directory so runtime assets
# (shaders/, fonts/, config.json) resolve with relative paths.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm    CONFIG REQUIRED)

# glslc shader compiler (ships with the Vulkan SDK on Windows/macOS).
find_program(GLSLC_EXECUTABLE
    NAMES glslc glslc.exe
    HINTS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/Bin
)
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK or shaderc.")
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "main.cpp")

add_executable(Voxterra ${SOURCES})

target_compile_features(Voxterra PRIVATE cxx_std_20)
set_target_properties(Voxterra PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(Voxterra PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/headers"
)

target_compile_definitions(Voxterra PRIVATE GLFW_INCLUDE_VULKAN)

target_link_libraries(Voxterra PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# -----------------------------------------------------------------------------
# Platform-specific flags
# -----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(Voxterra PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(Voxterra PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
else()
    target_compile_options(Voxterra PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(APPLE)
    # MoltenVK ships as a non-conformant Vulkan implementation and requires
    # the portability enumeration extension at instance creation time.
    target_compile_definitions(Voxterra PRIVATE VK_USE_PORTABILITY_ENUMERATION)
endif()

# -----------------------------------------------------------------------------
# Shader compilation (cross-platform, integrated into the build graph)
# -----------------------------------------------------------------------------
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES
    voxel.vert
    voxel.frag
    water.frag
    ui.vert
    ui.frag
)

set(SPV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    set(SRC ${SHADER_SRC_DIR}/${SHADER})
    set(DST ${SHADER_OUT_DIR}/${SHADER}.spv)
    add_custom_command(
        OUTPUT  ${DST}
        COMMAND ${GLSLC_EXECUTABLE} ${SRC} -o ${DST}
        DEPENDS ${SRC}
        COMMENT "Compiling shader ${SHADER}"
        VERBATIM
    )
    list(APPEND SPV_OUTPUTS ${DST})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_OUTPUTS})
add_dependencies(Voxterra Shaders)

# -----------------------------------------------------------------------------
# Copy runtime assets next to the executable
# -----------------------------------------------------------------------------
add_custom_command(TARGET Voxterra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/fonts
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/config.json
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COMMENT "Copying runtime assets (fonts, config)"
)
