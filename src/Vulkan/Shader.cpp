#include "Vulkan/Shader.hpp"
#include <iostream>
#include <fstream>

Shader::~Shader() noexcept {
    if (m_shaderModule != VK_NULL_HANDLE && m_deviceHandle != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_deviceHandle, m_shaderModule, nullptr);
    }
}

std::vector<char> Shader::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "[Shader] Failed to open file: " << filename << std::endl;
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

bool Shader::loadFromFile(VkDevice device, const std::string& filename) noexcept {
    m_deviceHandle = device;

    std::vector<char> code = readFile(filename);
    if (code.empty()) {
        std::cerr << "[Shader] Empty shader code from: " << filename << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    if (vkCreateShaderModule(m_deviceHandle, &createInfo, nullptr, &m_shaderModule) != VK_SUCCESS) {
        std::cerr << "[Shader] Failed to create shader module from: " << filename << std::endl;
        return false;
    }

    std::cout << "[Shader] Loaded: " << filename << std::endl;
    return true;
}
