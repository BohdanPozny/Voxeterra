cmake_minimum_required(VERSION 3.16)

project(Voxterra VERSION 0.1 LANGUAGES CXX)

# Export compile_commands.json for clangd / IDE integration.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators (Ninja/Make) put the binary in ${CMAKE_BINARY_DIR};
# multi-config generators (Visual Studio/Xcode) use a per-config subdirectory.
# Asset copy steps below use $<TARGET_FILE_DIR:Voxterra> so they work with both.

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

set(SHADER_SOURCES
    voxel.vert
    voxel.frag
    water.frag
    ui.vert
    ui.frag
)

# Intermediate .spv files live in the build tree; they are copied next to the
# executable by the post-build step below.
set(SHADER_INT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_INT_DIR})

set(SPV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    set(SRC ${SHADER_SRC_DIR}/${SHADER})
    set(DST ${SHADER_INT_DIR}/${SHADER}.spv)
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
# Copy runtime assets next to the executable (works on Ninja/Make AND on
# multi-config generators like Visual Studio/Xcode).
# -----------------------------------------------------------------------------
add_custom_command(TARGET Voxterra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_INT_DIR}
            $<TARGET_FILE_DIR:Voxterra>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/fonts
            $<TARGET_FILE_DIR:Voxterra>/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/config.json
            $<TARGET_FILE_DIR:Voxterra>/config.json
    COMMENT "Copying runtime assets (shaders, fonts, config)"
)
