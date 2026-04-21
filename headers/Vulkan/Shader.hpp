#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class Shader {
private:
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

    std::vector<char> readFile(const std::string& filename);

public:
    Shader() = default;
    ~Shader() noexcept;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool loadFromFile(VkDevice device, const std::string& filename) noexcept;

    VkShaderModule getModule() const noexcept { return m_shaderModule; }
};
