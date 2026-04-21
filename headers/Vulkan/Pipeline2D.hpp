#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Device;

// Pipeline2D - спеціалізований pipeline для 2D рендерингу (UI, overlays)
class Pipeline2D {
public:
    struct Vertex2D {
        glm::vec2 position;  // Screen space (-1 to 1)
        glm::vec4 color;
    };

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_fragShader = VK_NULL_HANDLE;

public:
    Pipeline2D() = default;
    ~Pipeline2D();

    Pipeline2D(const Pipeline2D&) = delete;
    Pipeline2D& operator=(const Pipeline2D&) = delete;

    bool init(Device& device, VkRenderPass renderPass, VkExtent2D screenExtent);
    void cleanup();

    VkPipeline getPipeline() const { return m_pipeline; }
    VkPipelineLayout getLayout() const { return m_pipelineLayout; }

private:
    void createPipeline(VkRenderPass renderPass, VkExtent2D screenExtent);
};
