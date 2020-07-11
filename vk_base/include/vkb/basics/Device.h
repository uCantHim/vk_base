#pragma once

#include <vector>

#include "PhysicalDevice.h"

namespace vkb
{

/**
 * A logical device used to interface with an underlying physical device.
 */
class Device
{
public:
    /**
     * @brief Create a logical device from a physical device
     *
     * Fully initializes the device.
     */
    explicit Device(const PhysicalDevice& physDevice);
    Device(Device&&) noexcept = default;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device& operator=(Device&&) noexcept = delete;

    auto operator->() const noexcept -> const vk::Device*;
    auto operator*() const noexcept -> vk::Device;
    auto get() const noexcept -> vk::Device;

    auto getPhysicalDevice() const noexcept -> const PhysicalDevice&;

    /**
     * @brief Create a temporary command buffer for graphics operations
     *
     * Allocates the command buffer from a pool with the reset and the
     * transient flags set.
     *
     * The command buffer CAN only be executed on a queue of the first
     * graphics-capable queue family. To ensure that this is the case, you
     * can use Device::executeGraphicsCommandBuffer(), which submits a
     * command buffer to that specific queue.
     *
     * @param vk::CommandBufferLevel level Create a primary or a secondary
     *                                     command buffer.
     *
     * @return vk::UniqueCommandBuffer A command buffer
     */
    auto createGraphicsCommandBuffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const
        -> vk::UniqueCommandBuffer;

    /**
     * @brief Execute a command buffer
     *
     * Executes a command buffer on the first queue of the first
     * graphics-capable queue family.
     *
     * @param vk::CommandBuffer cmdBuf The command buffer to execute
     */
    void executeGraphicsCommandBuffer(vk::CommandBuffer cmdBuf) const;

    /**
     * @brief Execute a command buffer synchronously
     *
     * Creates a fence and waits for it to be signaled before the function
     * returns.
     *
     * Executes the command buffer on the first queue of the first
     * graphics-capable queue family.
     *
     * @param vk::CommandBuffer cmdBuf The command buffer to execute
     */
    void executeGraphicsCommandBufferSynchronously(vk::CommandBuffer cmdBuf) const;

    /**
     * @brief Create a temporary command buffer for transfer operations
     *
     * Allocates the command buffer from a pool with the reset and the
     * transient flags set.
     *
     * The command buffer CAN only be executed on a queue of the first
     * transfer-capable queue family. To ensure that this is the case, you
     * can use Device::executeTransferCommandBuffer(), which submits a
     * command buffer to that specific queue.
     *
     * @param vk::CommandBufferLevel level Create a primary or a secondary
     *                                     command buffer.
     *
     * @return vk::UniqueCommandBuffer A command buffer
     */
    auto createTransferCommandBuffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const
        -> vk::UniqueCommandBuffer;

    /**
     * @brief Execute a command buffer
     *
     * Executes a command buffer on the first queue of the first
     * transfer-capable queue family.
     *
     * @param vk::CommandBuffer cmdBuf The command buffer to execute
     */
    void executeTransferCommandBuffer(vk::CommandBuffer cmdBuf) const;

    /**
     * @brief Execute a command buffer synchronously
     *
     * Creates a fence and waits for it to be signaled before the function
     * returns.
     *
     * Executes the command buffer on the first queue of the first
     * transfer-capable queue family.
     *
     * @param vk::CommandBuffer cmdBuf The command buffer to execute
     */
    void executeTransferCommandBufferSyncronously(vk::CommandBuffer cmdBuf) const;

private:
    const PhysicalDevice& physicalDevice;
    vk::UniqueDevice device;

    vk::UniqueCommandPool graphicsPool;
    vk::Queue graphicsQueue;
    vk::UniqueCommandPool transferPool;
    vk::Queue transferQueue;
};

} // namespace vkb