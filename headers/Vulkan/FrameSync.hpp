#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// Per-frame synchronization primitives (MAX_FRAMES_IN_FLIGHT slots):
//   imageAvailable : signalled when the swapchain image is ready to render into.
//   renderFinished : signalled when GPU work for the frame has completed.
//   inFlightFence  : host-side fence preventing the CPU from lapping the GPU.
class FrameSync {
public:
    FrameSync() = default;
    ~FrameSync();

    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    bool init(VkDevice device, uint32_t maxFramesInFlight);
    void cleanup();

    // Host waits on the fence associated with the given frame slot.
    void waitForFence(uint32_t frameIndex) const;
    void resetFence(uint32_t frameIndex) const;

    VkSemaphore getImageAvailable(uint32_t frameIndex) const { return m_imageAvailable[frameIndex]; }
    VkSemaphore getRenderFinished(uint32_t frameIndex) const { return m_renderFinished[frameIndex]; }
    VkFence getFence(uint32_t frameIndex) const { return m_inFlightFences[frameIndex]; }

    uint32_t getMaxFramesInFlight() const { return static_cast<uint32_t>(m_inFlightFences.size()); }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    std::vector<VkSemaphore> m_imageAvailable;
    std::vector<VkSemaphore> m_renderFinished;
    std::vector<VkFence> m_inFlightFences;
};
