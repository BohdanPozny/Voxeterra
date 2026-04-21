#include "Vulkan/FrameSync.hpp"
#include <iostream>

FrameSync::~FrameSync() {
    cleanup();
}

bool FrameSync::init(VkDevice device, uint32_t maxFramesInFlight) {
    m_device = device;

    m_imageAvailable.resize(maxFramesInFlight, VK_NULL_HANDLE);
    m_renderFinished.resize(maxFramesInFlight, VK_NULL_HANDLE);
    m_inFlightFences.resize(maxFramesInFlight, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // start signalled so first frame does not block.

    for (uint32_t i = 0; i < maxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "[FrameSync] Failed to create sync objects for frame " << i << std::endl;
            return false;
        }
    }

    return true;
}

void FrameSync::cleanup() {
    if (m_device == VK_NULL_HANDLE) return;

    for (auto& sem : m_imageAvailable) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, sem, nullptr);
            sem = VK_NULL_HANDLE;
        }
    }
    for (auto& sem : m_renderFinished) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, sem, nullptr);
            sem = VK_NULL_HANDLE;
        }
    }
    for (auto& fence : m_inFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, fence, nullptr);
            fence = VK_NULL_HANDLE;
        }
    }

    m_device = VK_NULL_HANDLE;
}

void FrameSync::waitForFence(uint32_t frameIndex) const {
    vkWaitForFences(m_device, 1, &m_inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);
}

void FrameSync::resetFence(uint32_t frameIndex) const {
    vkResetFences(m_device, 1, &m_inFlightFences[frameIndex]);
}
