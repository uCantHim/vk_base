#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "trc/base/Buffer.h"

#include "trc/Types.h"

namespace trc::rt
{
    class ShaderBindingTable
    {
    public:
        /**
         * @brief Create a shader binding table
         *
         * Is is advised to pass an allocator to this constructor that
         * allocates from a memory pool. Otherwise, each entry in the table
         * gets its own allociation.
         *
         * @param const Device& device
         * @param vk::Pipeline pipeline The ray tracing pipeline that this
         *                              table refers to
         * @param std::vector<ui32> entrySizes One ui32 for each entry in
         *        the table. The number signals how many subsequent shader
         *        groups are present in the entry.
         * @param const DeviceMemoryAllocator& alloc
         */
        ShaderBindingTable(
            const Device& device,
            const VulkanDispatchLoaderDynamic& dl,
            vk::Pipeline pipeline,
            std::vector<ui32> entrySizes,
            const DeviceMemoryAllocator& alloc
                = DefaultDeviceMemoryAllocator{ vk::MemoryAllocateFlagBits::eDeviceAddress}
        );

        /**
         * @brief Map an entry index to a string
         */
        void setEntryAlias(std::string entryName, ui32 entryIndex);

        /**
         * @return vk::StridedDeviceAddressRegionKHR The address of the
         *         entryIndex-th entry in the shader binding table.
         */
        auto getEntryAddress(ui32 entryIndex) -> vk::StridedDeviceAddressRegionKHR;

        /**
         * @return vk::StridedDeviceAddressRegionKHR The address of an
         *         entry with name entryName.
         * @throw std::out_of_range if no entry has previously been mapped
         *        to entryName.
         */
        auto getEntryAddress(const std::string& entryName)
            -> vk::StridedDeviceAddressRegionKHR;

    private:
        /**
         * A single entry in the shader binding table has its own buffer
         * and an address region specifying the address of the entry.
         */
        struct GroupEntry
        {
            DeviceLocalBuffer buffer;
            vk::StridedDeviceAddressRegionKHR address;
        };

        std::vector<GroupEntry> entries;
        std::unordered_map<std::string, ui32> entryAliases;
    };
} // namespace trc::rt
