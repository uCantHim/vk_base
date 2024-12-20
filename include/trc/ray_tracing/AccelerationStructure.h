#pragma once

#include "trc/base/Buffer.h"

#include "trc/Types.h"

namespace trc {
    class Instance;
}

namespace trc::rt
{
    /**
     * This file also contains the following aliases at the bottom:
     *
     *     using BLAS = BottomLevelAccelerationStructure;
     *     using TLAS = TopLevelAccelerationStructure;
     */

    namespace internal
    {
        class AccelerationStructureBase
        {
        protected:
            AccelerationStructureBase() = default;

        public:
            /**
             * @return The underlying acceleration structure handle
             */
            auto operator*() const noexcept -> const vk::AccelerationStructureKHR&;

            auto getGeometryBuildInfo() const noexcept
                -> const vk::AccelerationStructureBuildGeometryInfoKHR&;
            auto getBuildSize() const noexcept
                -> const vk::AccelerationStructureBuildSizesInfoKHR&;

        protected:
            using UniqueAccelerationStructure = vk::UniqueHandle<
                vk::AccelerationStructureKHR, VulkanDispatchLoaderDynamic
            >;

            /**
             * @brief Create or recreate the acceleration structure
             */
            void create(const ::trc::Instance& instance,
                        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo,
                        const vk::ArrayProxy<const ui32>& primitiveCount,
                        const DeviceMemoryAllocator& alloc);

            UniqueAccelerationStructure accelerationStructure;

            vk::AccelerationStructureBuildGeometryInfoKHR geoBuildInfo;
            vk::AccelerationStructureBuildSizesInfoKHR buildSizes;
            DeviceLocalBuffer accelerationStructureBuffer;
        };
    } // namespace internal

    /**
     * @brief A bottom-level acceleration structure
     */
    class BottomLevelAccelerationStructure : public internal::AccelerationStructureBase
    {
    public:
        /**
         * @param GeometryID geo
         * @param const DeviceMemoryAllocator& alloc
         */
        BottomLevelAccelerationStructure(
            const ::trc::Instance& instance,
            vk::AccelerationStructureGeometryKHR geo,
            const DeviceMemoryAllocator& alloc = DefaultDeviceMemoryAllocator{});

        BottomLevelAccelerationStructure(
            const ::trc::Instance& instance,
            std::vector<vk::AccelerationStructureGeometryKHR> geos,
            const DeviceMemoryAllocator& alloc = DefaultDeviceMemoryAllocator{});

        /**
         * @brief Build the acceleration structure
         *
         * It is advised to use buildAccelerationStructures() to build
         * multiple acceleration structures at once.
         */
        void build();

        void build(vk::CommandBuffer cmdBuf, vk::DeviceAddress scratchMemoryAddress);

        /**
         * Used to create instances from the acceleration structure
         *
         * @return ui64 The acceleration structure's address on the device
         */
        auto getDeviceAddress() const noexcept -> ui64;

        /**
         * @brief Create build ranges for the acceleration structure
         *
         * The interface for build ranges in vulkan.hpp is terrible and so
         * is this function. The first element in the resulting pair keeps
         * the build ranges alive, while the second element contains the
         * pointers to those build ranges that are actually passed to the
         * vkCmdBuildAccelerationStructuresKHR command.
         */
        auto makeBuildRanges() const noexcept
            -> std::pair<
                std::unique_ptr<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>>,
                std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*>
            >;

    private:
        const ::trc::Instance& instance;

        std::vector<vk::AccelerationStructureGeometryKHR> geometries;
        std::vector<ui32> primitiveCounts;
        ui64 deviceAddress;
    };

    /**
     * @brief A collection of geometry instances that can be raytraced
     */
    class TopLevelAccelerationStructure : public internal::AccelerationStructureBase
    {
    public:
        /**
         * @brief Create a top level acceleration structure
         *
         * One must specify a maximum number of contained geometry instances at
         * creation time because the number of drawn instances may vary over
         * time.
         *
         * @param ui32 maxInstances The maximum number of geometry instances
         *                          in the TLAS
         */
        TopLevelAccelerationStructure(
            const ::trc::Instance& instance,
            ui32 maxInstances,
            const DeviceMemoryAllocator& alloc = DefaultDeviceMemoryAllocator{});

        auto getMaxInstances() const -> ui32;

        /**
         * @brief Build the TLAS from a buffer of GeometryInstance structs
         *
         * @param vk::Buffer instanceBuffer Buffer that contains instance
         *        data formatted as vk::AccelerationStructureInstanceKHR
         *        dictates.
         */
        void build(vk::Buffer instanceBuffer, ui32 numInstances, ui32 offset = 0);

        /**
         * @brief Build the TLAS from a buffer of GeometryInstance structs
         *
         * @param vk::Buffer instanceBuffer Buffer that contains instance
         *        data formatted as vk::AccelerationStructureInstanceKHR
         *        dictates.
         * @param ui32 numInstance The number of instances to read from
         *        instanceBuffer. Will be capped at the TLAS's maxInstances
         *        number.
         * @param ui32 offset Byte offset into instanceBuffer.
         */
        void build(vk::CommandBuffer cmdBuf,
                   vk::DeviceAddress scratchMemoryAddress,
                   vk::Buffer instanceBuffer,
                   ui32 numInstances,
                   ui32 offset = 0);

        /**
         * @brief Update the TLAS from a buffer of GeometryInstance structs
         *
         * @param vk::Buffer instanceBuffer Buffer that contains instance
         *        data formatted as vk::AccelerationStructureInstanceKHR
         *        dictates.
         * @param ui32 numInstance The number of instances to read from
         *        instanceBuffer. Will be capped at the TLAS's maxInstances
         *        number.
         * @param ui32 offset Byte offset into instanceBuffer.
         */
        void update(vk::CommandBuffer cmdBuf,
                    vk::DeviceAddress scratchMemoryAddress,
                    vk::Buffer instanceBuffer,
                    ui32 numInstances,
                    ui32 offset = 0);

    private:
        void updateOrBuild(vk::CommandBuffer cmdBuf,
                           vk::BuildAccelerationStructureModeKHR mode,
                           vk::DeviceAddress scratchMemoryAddress,
                           vk::Buffer instanceBuffer,
                           ui32 numInstances,
                           ui32 offset);

        const Instance& instance;

        ui32 maxInstances;
        vk::AccelerationStructureGeometryKHR geometry;

        /**
         * The top level AS keeps the scratch buffer because it is updated
         * way more often than the bottom level AS
         */
        //Buffer scratchBuffer;
        //Buffer instanceBuffer;
    };


    // ---------------- //
    //      Helpers     //
    // ---------------- //

    /**
     * @brief Build multiple acceleration structures at once
     */
    void buildAccelerationStructures(const ::trc::Instance& instance,
                                     const std::vector<BottomLevelAccelerationStructure*>& as);

    using BLAS = BottomLevelAccelerationStructure;
    using TLAS = TopLevelAccelerationStructure;
} // namespace trc::rt
