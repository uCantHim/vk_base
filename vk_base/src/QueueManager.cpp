#include "QueueManager.h"

#include <iostream>

#include "basics/VulkanDebug.h"
#include "basics/Device.h"



namespace vkb
{
    /**
     * @return nullopt if no queue with the requested capability exists.
     */
    auto findMostSpecialized(QueueType type, const std::vector<QueueFamily>& families)
        -> std::optional<QueueFamilyIndex>
    {
        // Queue families with the number of additional capabilities
        std::vector<std::pair<QueueFamilyIndex, uint32_t>> possibleFamilies;

        for (const auto& family : families)
        {
            if (family.isCapable(type))
            {
                auto& [_, numCapabilities] = possibleFamilies.emplace_back(family.index, 0);

                // Test for other supported capabilities
                for (uint32_t i = 0; i < static_cast<uint32_t>(QueueType::numQueueTypes); i++)
                {
                    if (family.isCapable(static_cast<QueueType>(i))){
                        numCapabilities++;
                    }
                }
            }
        }

        if (possibleFamilies.empty()) {
            return std::nullopt;
        }

        // Find family with the least number of capabilities
        uint32_t minCapabilities = static_cast<uint32_t>(QueueType::numQueueTypes);
        QueueFamilyIndex familyWithMinCapabilities{ UINT32_MAX };
        for (const auto& [familyIndex, numCapabilities] : possibleFamilies)
        {
            if (numCapabilities < minCapabilities)
            {
                minCapabilities = numCapabilities;
                familyWithMinCapabilities = familyIndex;
            }
        }

        if (familyWithMinCapabilities == UINT32_MAX) {
            return std::nullopt;
        }
        return familyWithMinCapabilities;
    }
} // namespace vkb



vkb::QueueManager::QueueManager(const PhysicalDevice& physDevice, const Device& device)
{
    // Retrieve queues per family.
    // This first round also fills the allQueues array.
    queuesPerFamily.resize(physDevice.queueFamilies.size());
    for (const auto& family : physDevice.queueFamilies)
    {
        auto& queues = queuesPerFamily.at(family.index);
        for (uint32_t i = 0; i < family.queueCount; i++)
        {
            queueStorage.push_back(device->getQueue(family.index, i));
            uint32_t newQueueIndex{ static_cast<uint32_t>(queueStorage.size()) - 1 };
            queues.push_back(newQueueIndex);
        }
    }

    // Determine the primary queues for each capability
    for (size_t i = 0; i < queueTypeCount; i++)
    {
        auto mostSpecialized = findMostSpecialized(
            static_cast<QueueType>(i),
            physDevice.queueFamilies
        );

        if (mostSpecialized.has_value()) {
            primaryQueueFamilies[i] = mostSpecialized.value();
        }
        else {
            primaryQueueFamilies[i] = UINT32_MAX;
        }
    }

    // Collect all queues for each capability, not just of the primary
    // queue family
    for (const QueueFamily& family : physDevice.queueFamilies)
    {
        const auto& queues = queuesPerFamily[family.index];
        for (size_t i = 0; i < queueTypeCount; i++)
        {
            if (family.isCapable(QueueType(i)))
            {
                for (uint32_t queueIndex : queues) {
                    queuesPerCapability[i].emplace_back(queueIndex, family.index);
                }
            }
        }
    }

    if constexpr (enableVerboseLogging)
    {
        std::cout << "\nQueue manager created for logical device.\n";
        for (int i = 0; i < static_cast<int>(QueueType::numQueueTypes); i++)
        {
            if (primaryQueueFamilies[i] != UINT32_MAX)
            {
                std::cout << "   Chose queue family " << primaryQueueFamilies[i]
                    << " as the primary " << std::to_string(QueueType(i)) << " queue family.\n";
            }
            else
            {
                std::cout << "   No queue family found with " << std::to_string(QueueType(i))
                    << " support.\n";
            }
        }
    }
}

auto vkb::QueueManager::getFamilyQueues(QueueFamilyIndex family) const -> std::vector<vk::Queue>
{
    std::vector<vk::Queue> result;

    const auto& indices = queuesPerFamily.at(family);
    for (uint32_t index : indices)
    {
        auto queue = getQueue(index);
        if (queue.has_value()) {
            result.push_back(queue.value());
        }
    }

    return result;
}

auto vkb::QueueManager::getPrimaryQueueFamily(QueueType type) const -> QueueFamilyIndex
{
    QueueFamilyIndex family = primaryQueueFamilies.at(static_cast<size_t>(type));
    if (family == UINT32_MAX)
    {
        throw std::out_of_range(
            "[QueueManager::getPrimaryQueueFamily]: No queue supports the requested "
            "capability" + std::to_string(static_cast<size_t>(type))
        );
    }

    return family;
}

auto vkb::QueueManager::getPrimaryQueue(QueueType type) const -> vk::Queue
{
    const auto& indices = queuesPerFamily.at(getPrimaryQueueFamily(type));

    std::optional<vk::Queue> queue{ std::nullopt };
    uint32_t& nextQueueIndex = nextPrimaryQueueRotation[static_cast<size_t>(type)];
    const uint32_t initialIndex = nextQueueIndex;
    do {
        nextQueueIndex = ++nextQueueIndex >= indices.size() ? 0 : nextQueueIndex;
        queue = getQueue(indices.at(nextQueueIndex));
    } while (!queue.has_value() && initialIndex != nextQueueIndex);

    if (!queue.has_value())
    {
        throw QueueReservedError("All primary queues of type " + std::to_string(type)
                                 + " are reserved");
    }
    return queue.value();
}

auto vkb::QueueManager::getPrimaryQueue(QueueType type, uint32_t queueIndex) const -> vk::Queue
{
    auto queue = getQueue(queuesPerFamily[getPrimaryQueueFamily(type)].at(queueIndex));
    if (!queue.has_value())
    {
        throw QueueReservedError("Primary queue of type " + std::to_string(type)
                                 + " at index " + std::to_string(queueIndex) + " is reserved");
    }
    return queue.value();
}

auto vkb::QueueManager::getPrimaryQueueCount(QueueType type) const noexcept -> uint32_t
{
    try {
        return static_cast<uint32_t>(queuesPerFamily.at(getPrimaryQueueFamily(type)).size());
    }
    catch (const std::out_of_range&) {
        return 0;
    }
}

auto vkb::QueueManager::getAnyQueue(QueueType type) const
    -> std::pair<vk::Queue, QueueFamilyIndex>
{
    auto getNextQueueIndex = [this](QueueType type) {
        uint32_t& nextQueueIndex = nextAnyQueueRotation[static_cast<size_t>(type)];
        if (nextQueueIndex >= queuesPerCapability[static_cast<size_t>(type)].size()) {
            nextQueueIndex = 0;
        }
        return nextQueueIndex++;
    };

    const uint32_t firstQueueIndex = getNextQueueIndex(type);
    uint32_t queueIndex = firstQueueIndex;
    do {
        auto [index, family] = queuesPerCapability[static_cast<size_t>(type)].at(queueIndex);
        auto queue = getQueue(index);

        if (queue.has_value()) {
            return { queue.value(), family };
        }

        queueIndex = getNextQueueIndex(type);
    } while (firstQueueIndex != queueIndex);

    throw QueueReservedError("All queues of type " + std::to_string(type) + " are reserved");
}

auto vkb::QueueManager::getAnyQueue(QueueType type, uint32_t queueIndex) const
    -> std::pair<vk::Queue, QueueFamilyIndex>
{
    auto [index, family] = queuesPerCapability[static_cast<size_t>(type)].at(queueIndex);
    auto queue = getQueue(index);
    if (!queue.has_value())
    {
        throw QueueReservedError("Queue of type " + std::to_string(type)
                                 + " at index " + std::to_string(queueIndex) + " is reserved");
    }
    return { queue.value(), family };
}

auto vkb::QueueManager::getAnyQueueCount(QueueType type) const noexcept -> uint32_t
{
    return static_cast<uint32_t>(queuesPerCapability[static_cast<size_t>(type)].size());
}

auto vkb::QueueManager::reserveQueue(vk::Queue queue) -> vk::Queue
{
    for (uint32_t i = 0; i < queueStorage.size(); i++)
    {
        if (queueStorage.at(i) == queue)
        {
            auto [_, success] = reservedQueueIndices.emplace(i);
            if (!success) {
                throw QueueReservedError("Tried to reserve queue that is already reserved");
            }

            return queue;
        }
    }

    throw std::out_of_range("Tried to reserve queue that does not exist");
}

auto vkb::QueueManager::reservePrimaryQueue(QueueType type) -> vk::Queue
{
    return reserveQueue(getPrimaryQueue(type));
}

auto vkb::QueueManager::reservePrimaryQueue(QueueType type, uint32_t queueIndex) -> vk::Queue
{
    return reserveQueue(getPrimaryQueue(type, queueIndex));
}

auto vkb::QueueManager::getQueue(uint32_t index) const -> std::optional<vk::Queue>
{
    if (reservedQueueIndices.contains(index)) {
        return std::nullopt;
    }

    return queueStorage.at(index);
}
