#pragma once

#include <vulkan/vulkan.h>
#include <string>

class Instance {
private:
    VkInstance m_instance;
    bool checkValidationLayerSupport() noexcept;

public:
    Instance() = default;
    ~Instance() noexcept;

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    bool init(std::string name) noexcept;

    VkInstance getInstance() noexcept;
};
