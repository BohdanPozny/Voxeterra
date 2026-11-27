#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// Synchronization primitives:
//   imageAvailable : per frame slot, signalled when the swapchain image is ready to render into.
//   renderFinished : per swapchain image, since presentation may still hold it after the frame slot is reused.
//   inFlightFence  : per frame slot, prevents the CPU from lapping the GPU.
class FrameSync {
public:
    FrameSync() = default;
    ~FrameSync();

    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    bool init(VkDevice device, uint32_t maxFramesInFlight, uint32_t imageCount);
    bool recreateRenderFinished(uint32_t imageCount);
    void cleanup();

    // Host waits on the fence associated with the given frame slot.
    void waitForFence(uint32_t frameIndex) const;
    void resetFence(uint32_t frameIndex) const;

    VkSemaphore getImageAvailable(uint32_t frameIndex) const { return m_imageAvailable[frameIndex]; }
    VkSemaphore getRenderFinished(uint32_t imageIndex) const { return m_renderFinished[imageIndex]; }
    VkFence getFence(uint32_t frameIndex) const { return m_inFlightFences[frameIndex]; }

    uint32_t getMaxFramesInFlight() const { return static_cast<uint32_t>(m_inFlightFences.size()); }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlightFences;

    void destroyRenderFinished();
};
