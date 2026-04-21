cmake_minimum_required(VERSION 3.15)

project(Voxterra VERSION 1 LANGUAGES CXX)

# МАГІЯ ДЛЯ NEOVIM:
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Збираємо всі файли (поки що залишаємо GLOB, хоча краще перераховувати руками)
file(GLOB_RECURSE SOURCES "src/*.cpp" "main.cpp")

# Створюємо програму
add_executable(Voxterra ${SOURCES})

# Налаштовуємо стандарт та макроси
target_compile_features(Voxterra PRIVATE cxx_std_20)
set_target_properties(Voxterra PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(Voxterra PRIVATE GLFW_INCLUDE_VULKAN)

# Вказуємо компілятору, де шукати заголовкові файли (.hpp)
target_include_directories(Voxterra PUBLIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/headers"
)
target_compile_options(Voxterra PRIVATE "-I../headers")

# Підключаємо бібліотеки
target_link_libraries(Voxterra PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Перевірки коду
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Voxterra PRIVATE -Wall -Wextra)
endif()
