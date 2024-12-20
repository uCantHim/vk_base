#include "trc/ray_tracing/ShaderBindingTable.h"

#include <numeric>

#include "trc_util/Padding.h"



trc::rt::ShaderBindingTable::ShaderBindingTable(
    const Device& device,
    const VulkanDispatchLoaderDynamic& dl,
    vk::Pipeline pipeline,
    std::vector<ui32> entrySizes,
    const DeviceMemoryAllocator& alloc)
{
    const auto rayTracingProperties = device.getPhysicalDevice().physicalDevice.getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
    >().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    const ui32 groupHandleSize = rayTracingProperties.shaderGroupHandleSize;
    const ui32 alignedGroupHandleSize = util::pad(
        groupHandleSize, rayTracingProperties.shaderGroupBaseAlignment
    );
    const ui32 numShaderGroups = std::accumulate(entrySizes.begin(), entrySizes.end(), 0u);
    const vk::DeviceSize sbtSize = alignedGroupHandleSize * numShaderGroups;

    std::vector<ui8> shaderHandleStorage(sbtSize);
    auto result = device->getRayTracingShaderGroupHandlesKHR(
        pipeline,
        0, // first group
        numShaderGroups, // group count
        shaderHandleStorage.size(),
        shaderHandleStorage.data(),
        dl
    );
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Unable to retrieve shader group handles");
    }

    // Calculate shader group addresses
    for (ui32 groupIndex = 0; ui32 numGroups : entrySizes)
    {
        const ui32 stride = alignedGroupHandleSize;
        const ui32 size = alignedGroupHandleSize * numGroups;

        // Copy all group addresses of current entry into aligned storage
        std::vector<ui8> alignedHandleStorage(sbtSize);
        for (ui32 i = 0; i < numGroups; i++)
        {
            memcpy(
                alignedHandleStorage.data() + i * alignedGroupHandleSize,
                shaderHandleStorage.data() + groupIndex * groupHandleSize,
                groupHandleSize
            );
            groupIndex++;
        }

        DeviceLocalBuffer buffer{
            device,
            alignedHandleStorage,
            vk::BufferUsageFlagBits::eShaderBindingTableKHR
            | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            alloc
        };
        vk::StridedDeviceAddressRegionKHR address(
            device->getBufferAddress({ *buffer }),
            stride,  // stride
            size     // size
        );
        entries.push_back({ std::move(buffer), address });
    }
}

void trc::rt::ShaderBindingTable::setEntryAlias(
    std::string entryName,
    ui32 entryIndex)
{
    entryAliases.try_emplace(std::move(entryName), entryIndex);
}

auto trc::rt::ShaderBindingTable::getEntryAddress(ui32 entryIndex)
    -> vk::StridedDeviceAddressRegionKHR
{
    return entries.at(entryIndex).address;
}

auto trc::rt::ShaderBindingTable::getEntryAddress(const std::string& entryName)
    -> vk::StridedDeviceAddressRegionKHR
{
    return getEntryAddress(entryAliases.at(entryName));
}
