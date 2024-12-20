#pragma once

#include <concepts>
#include <stdexcept>
#include <string>
#include <vector>

#include "trc/base/PhysicalDevice.h"
#include "trc/base/QueueManager.h"

namespace trc
{

/**
 * A logical device used to interface with an underlying physical device.
 */
class Device
{
public:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device& operator=(Device&&) noexcept = delete;

    Device(Device&&) noexcept = default;
    ~Device() = default;

    /**
     * @brief Create a logical device from a physical device
     *
     * Fully initializes the device.
     *
     * Only pass a physical device to create a device with default
     * extensions and features.
     *
     * @param const PhysicalDevice& physDevice
     * @param std::vector<const char*> deviceExtensions Additional
     *        extensions to load on the device.
     * @param void* extraPhysicalDeviceFeatureChain Additional chained
     *        device features to enable on the logical device. This
     *        pointer will be set as pNext of the end of vkb's default
     *        feature chain.
     */
    explicit Device(const PhysicalDevice& physDevice,
                    std::vector<const char*> deviceExtensions = {},
                    void* extraPhysicalDeviceFeatureChain = nullptr);

    /**
     * @brief Create a device with a pre-created logical device
     *
     * Use this overload for maximum control over logical device creation.
     *
     * The constructor also has an overload that accepts various
     * configuration parameters and uses those to create a logical device.
     *
     * @param const PhysicalDevice& physDevice
     * @param vk::UniqueDevice device MUST have been created from the
     *                                physical device.
     */
    Device(const PhysicalDevice& physDevice, vk::UniqueDevice device);

    auto operator->() const noexcept -> const vk::Device*;
    auto operator*() const noexcept -> vk::Device;
    auto get() const noexcept -> vk::Device;

    auto getPhysicalDevice() const noexcept -> const PhysicalDevice&;

    /**
     * @brief Get a dynamic loader that can dispatch calls to device functions
     *        as well as those of the device's instance.
     *
     * @return The device's dynamic dispatch loader.
     */
    auto getDL() const noexcept -> const VulkanDispatchLoaderDynamic&;

    auto getQueueManager() noexcept -> QueueManager&;
    auto getQueueManager() const noexcept -> const QueueManager&;

    /**
     * @brief Execute commands on the device
     *
     * Creates a transient command buffer, passes it to a function that
     * records some commands, then submits it to an appropriate queue.
     *
     * Only returns once all commands have finished executing.
     */
    template<std::invocable<vk::CommandBuffer> F>
    void executeCommands(QueueType queueType, F func, uint64_t timeout = UINT64_MAX) const;

    /**
     * @brief Set a debug name on a Vulkan object
     *
     * Does nothing when the TRC_DEBUG macro is not defined.
     */
    template<typename T>
        requires requires {
            typename T::NativeType;
            requires std::same_as<const vk::ObjectType, decltype(T::objectType)>;
        }
    void setDebugName(T object, const char* name) const;

    /**
     * @brief Set a debug name on a Vulkan object
     *
     * Does nothing when the TRC_DEBUG macro is not defined.
     */
    template<typename T>
        requires requires {
            typename T::NativeType;
            requires std::same_as<const vk::ObjectType, decltype(T::objectType)>;
        }
    void setDebugName(T object, const std::string& name) const;

private:
#ifdef TRC_DEBUG
    const PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
#endif

    const PhysicalDevice physicalDevice;
    vk::UniqueDevice device;
    const VulkanDispatchLoaderDynamic dispatchLoaderDynamic;

    QueueManager queueManager;

    // Stores one command pool for each queue
    std::vector<vk::UniqueCommandPool> commandPools;
};

template<std::invocable<vk::CommandBuffer> F>
void Device::executeCommands(QueueType queueType, F func, uint64_t timeout) const
{
    // Create a temporary fence
    auto fence = device->createFenceUnique({ vk::FenceCreateFlags() });

    // Get a suitable queue
    auto [queue, family] = queueManager.getAnyQueue(queueType);

    // Record commands
    auto cmdBuf = std::move(device->allocateCommandBuffersUnique({
        *commandPools[family], vk::CommandBufferLevel::ePrimary, 1
    })[0]);
    cmdBuf->begin(vk::CommandBufferBeginInfo{});
    func(*cmdBuf);
    cmdBuf->end();

    queue.waitSubmit(vk::SubmitInfo(0, nullptr, nullptr, 1, &*cmdBuf), *fence);

    // Wait for the fence
    const auto res = device->waitForFences(*fence, true, timeout);
    if (res != vk::Result::eSuccess)
    {
        throw std::runtime_error(
            "Error while waiting for fence in Device::executeCommands: " + vk::to_string(res)
        );
    }
}

template<typename T>
    requires requires {
        typename T::NativeType;
        requires std::same_as<const vk::ObjectType, decltype(T::objectType)>;
    }
void Device::setDebugName([[maybe_unused]] T object, [[maybe_unused]] const char* name) const
{
#ifdef TRC_DEBUG
    assert(object);

    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = VkObjectType(object.objectType);
    info.objectHandle = (uint64_t)typename T::NativeType(object);
    info.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(*device, &info);
#endif
}

template<typename T>
    requires requires {
        typename T::NativeType;
        requires std::same_as<const vk::ObjectType, decltype(T::objectType)>;
    }
void Device::setDebugName(T object, const std::string& name) const
{
    setDebugName(std::forward<T>(object), name.c_str());
}

} // namespace trc
