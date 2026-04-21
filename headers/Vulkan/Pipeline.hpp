#pragma once

#include <vulkan/vulkan.h>
#include <string>

class Pipeline {
private:
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDevice m_deviceHandle = VK_NULL_HANDLE;

public:
    Pipeline() = default;
    ~Pipeline() noexcept;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    bool init(VkDevice device,
              VkRenderPass renderPass,
              VkExtent2D swapchainExtent,
              const std::string& vertShaderPath,
              const std::string& fragShaderPath) noexcept;

    bool initWithVertexInput(VkDevice device,
                             VkRenderPass renderPass,
                             VkExtent2D swapchainExtent,
                             const std::string& vertShaderPath,
                             const std::string& fragShaderPath,
                             VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE,
                             bool enableAlphaBlending = false,
                             bool enableDepthWrite = true) noexcept;

    VkPipeline getPipeline() const noexcept { return m_pipeline; }
    VkPipelineLayout getLayout() const noexcept { return m_pipelineLayout; }
};
