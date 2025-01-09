#include "trc/ray_tracing/AccelerationStructure.h"

#include "trc/base/MemoryPool.h"

#include "trc/core/Instance.h"



auto trc::rt::internal::AccelerationStructureBase::operator*() const noexcept
    -> const vk::AccelerationStructureKHR&
{
    return *accelerationStructure;
}

void trc::rt::internal::AccelerationStructureBase::create(
    const ::trc::Instance& instance,
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo,
    const vk::ArrayProxy<const ui32>& primitiveCount,
    const DeviceMemoryAllocator& alloc)
{
    geoBuildInfo = buildInfo;

    buildSizes = instance.getDevice()->getAccelerationStructureBuildSizesKHR(
        vk::AccelerationStructureBuildTypeKHR::eHostOrDevice,
        geoBuildInfo,
        // Collect number of primitives for each geometry in the build
        // info
        primitiveCount,
        instance.getDL()
    );

    /**
     * TODO: Create a buffer pool instead of one buffer per AS
     */
    accelerationStructureBuffer = DeviceLocalBuffer{
        instance.getDevice(),
        buildSizes.accelerationStructureSize, nullptr,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
        | vk::BufferUsageFlagBits::eShaderDeviceAddress,
        alloc
    };
    accelerationStructure = instance.getDevice()->createAccelerationStructureKHRUnique(
        vk::AccelerationStructureCreateInfoKHR{
            {},
            *accelerationStructureBuffer,
            0, // buffer offset
            buildSizes.accelerationStructureSize,
            geoBuildInfo.type
        },
        nullptr,
        instance.getDL()
    );
}

auto trc::rt::internal::AccelerationStructureBase::getGeometryBuildInfo() const noexcept
    -> const vk::AccelerationStructureBuildGeometryInfoKHR&
{
    return geoBuildInfo;
}

auto trc::rt::internal::AccelerationStructureBase::getBuildSize() const noexcept
    -> const vk::AccelerationStructureBuildSizesInfoKHR&
{
    return buildSizes;
}



trc::rt::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(
    const ::trc::Instance& instance,
    vk::AccelerationStructureGeometryKHR geo,
    const DeviceMemoryAllocator& alloc)
    :
    BottomLevelAccelerationStructure(
        instance,
        std::vector<vk::AccelerationStructureGeometryKHR>{ geo },
        alloc
    )
{
}

trc::rt::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(
    const ::trc::Instance& instance,
    std::vector<vk::AccelerationStructureGeometryKHR> geos,
    const DeviceMemoryAllocator& alloc)
    :
    instance(instance),
    geometries(std::move(geos)),
    primitiveCounts(
        [this]() -> std::vector<ui32>
        {
            std::vector<ui32> numPrimitivesPerGeo;
            for (auto& geo : geometries)
            {
                assert(geo.geometryType == vk::GeometryTypeKHR::eTriangles);
                numPrimitivesPerGeo.push_back(geo.geometry.triangles.maxVertex / 3);
            }
            return numPrimitivesPerGeo;
        }()  // array of number of primitives for each geo in geometryinfo
    )
{
    // Create the acceleration structure now (when the geometries vector
    // is initialized)
    create(
        instance,
        vk::AccelerationStructureBuildGeometryInfoKHR{
            vk::AccelerationStructureTypeKHR::eBottomLevel,
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
            vk::BuildAccelerationStructureModeKHR::eBuild,
            {}, {}, // src and dst AS if mode is update
            static_cast<ui32>(geometries.size()),
            geometries.data()
        },
        primitiveCounts,
        alloc
    );

    deviceAddress = instance.getDevice()->getAccelerationStructureAddressKHR(
        { *accelerationStructure },
        instance.getDL()
    );
}

void trc::rt::BottomLevelAccelerationStructure::build()
{
    // Create a temporary scratch buffer
    DeviceLocalBuffer scratchBuffer{
        instance.getDevice(),
        buildSizes.buildScratchSize, nullptr,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
        DefaultDeviceMemoryAllocator{ vk::MemoryAllocateFlagBits::eDeviceAddress }
    };

    static auto features = instance.getPhysicalDevice().physicalDevice.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR
    >().get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();

    // Decide whether the AS should be built on the host or on the device.
    // Build on the host if possible.
    if (features.accelerationStructureHostCommands)
    {
        auto [_, buildRangePointers] = makeBuildRanges();

        [[maybe_unused]]
        vk::Result result = instance.getDevice()->buildAccelerationStructuresKHR(
            {}, // optional deferred operation
            geoBuildInfo
                .setScratchData(instance.getDevice()->getBufferAddress({ *scratchBuffer }))
                .setDstAccelerationStructure(*accelerationStructure),
            buildRangePointers,
            instance.getDL()
        );
        assert(result == vk::Result::eSuccess);
    }
    else
    {
        instance.getDevice().executeCommands(
            QueueType::compute,
            [this, &scratchBuffer](vk::CommandBuffer cmdBuf) {
                build(cmdBuf, instance.getDevice()->getBufferAddress({ *scratchBuffer }));
            }
        );
    }
}

void trc::rt::BottomLevelAccelerationStructure::build(
    vk::CommandBuffer cmdBuf,
    vk::DeviceAddress scratchDataAddress)
{
    auto [_, buildRangePointers] = makeBuildRanges();

    cmdBuf.buildAccelerationStructuresKHR(
        geoBuildInfo
            .setScratchData(scratchDataAddress)
            .setDstAccelerationStructure(*accelerationStructure),
        buildRangePointers,
        instance.getDL()
    );
}

auto trc::rt::BottomLevelAccelerationStructure::getDeviceAddress() const noexcept -> ui64
{
    return deviceAddress;
}

auto trc::rt::BottomLevelAccelerationStructure::makeBuildRanges() const noexcept
    -> std::pair<
        std::unique_ptr<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>>,
        std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*>
    >
{
    std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> buildRangePointers;
    auto buildRanges = [this, &buildRangePointers] {
        auto result = std::make_unique<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>>();
        result->reserve(primitiveCounts.size());
        for (ui32 numPrimitives : primitiveCounts)
        {
            buildRangePointers.push_back(&result->emplace_back(
                numPrimitives, // primitive count
                0, // primitive offset
                0, // first vertex index
                0  // transform offset
            ));
        }
        return result;
    }();

    return { std::move(buildRanges), std::move(buildRangePointers) };
}



trc::rt::TopLevelAccelerationStructure::TopLevelAccelerationStructure(
    const ::trc::Instance& instance,
    ui32 maxInstances,
    const DeviceMemoryAllocator& alloc)
    :
    instance(instance),
    maxInstances(maxInstances),
    geometry(
        vk::GeometryTypeKHR::eInstances,
        vk::AccelerationStructureGeometryDataKHR{
            vk::AccelerationStructureGeometryInstancesDataKHR{
                false, // Not an array of pointers
                nullptr
            }
        }
    )
{
    create(
        instance,
        vk::AccelerationStructureBuildGeometryInfoKHR{
            vk::AccelerationStructureTypeKHR::eTopLevel,
            vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
            vk::BuildAccelerationStructureModeKHR::eBuild,
            {}, {}, // src and dst AS if mode is update
            1, // geometryCount MUST be 1 for top-level AS
            &geometry
        },
        maxInstances,
        alloc
    );
}

auto trc::rt::TopLevelAccelerationStructure::getMaxInstances() const -> ui32
{
    return maxInstances;
}

void trc::rt::TopLevelAccelerationStructure::build(
    vk::Buffer instanceBuffer,
    const ui32 numInstances,
    const ui32 offset)
{
    // Temporary scratch buffer
    DeviceLocalBuffer scratchBuffer{
        instance.getDevice(),
        buildSizes.buildScratchSize,
        nullptr,
        vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
        DefaultDeviceMemoryAllocator{ vk::MemoryAllocateFlagBits::eDeviceAddress }
    };

    instance.getDevice().executeCommands(
        QueueType::compute,
        [&](vk::CommandBuffer cmdBuf)
        {
            build(
                cmdBuf,
                instance.getDevice()->getBufferAddress({ *scratchBuffer }),
                instanceBuffer, numInstances, offset
            );
        }
    );
}

void trc::rt::TopLevelAccelerationStructure::build(
    vk::CommandBuffer cmdBuf,
    vk::DeviceAddress scratchMemoryAddress,
    vk::Buffer instanceBuffer,
    ui32 numInstances,
    ui32 offset)
{
    updateOrBuild(
        cmdBuf,
        vk::BuildAccelerationStructureModeKHR::eBuild,
        scratchMemoryAddress,
        instanceBuffer, numInstances, offset
    );
}

void trc::rt::TopLevelAccelerationStructure::update(
    vk::CommandBuffer cmdBuf,
    vk::DeviceAddress scratchMemoryAddress,
    vk::Buffer instanceBuffer,
    ui32 numInstances,
    ui32 offset)
{
    updateOrBuild(
        cmdBuf,
        vk::BuildAccelerationStructureModeKHR::eUpdate,
        scratchMemoryAddress,
        instanceBuffer, numInstances, offset
    );
}

void trc::rt::TopLevelAccelerationStructure::updateOrBuild(
    vk::CommandBuffer cmdBuf,
    vk::BuildAccelerationStructureModeKHR mode,
    vk::DeviceAddress scratchMemoryAddress,
    vk::Buffer instanceBuffer,
    ui32 numInstances,
    ui32 offset)
{
    // Use new instance buffer
    geometry.geometry.instances.data = instance.getDevice()->getBufferAddress(instanceBuffer);

    vk::AccelerationStructureBuildRangeInfoKHR buildRange{
        glm::min(numInstances, maxInstances),
        offset,
        0, 0
    };

    cmdBuf.buildAccelerationStructuresKHR(
        geoBuildInfo
            .setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
                      | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate)
            .setGeometries(geometry)
            .setMode(mode)
            .setSrcAccelerationStructure(*accelerationStructure)
            .setDstAccelerationStructure(*accelerationStructure)
            .setScratchData(scratchMemoryAddress),
        { &buildRange },
        instance.getDL()
    );
}



// ---------------- //
//      Helpers     //
// ---------------- //

void trc::rt::buildAccelerationStructures(
    const ::trc::Instance& instance,
    const std::vector<BottomLevelAccelerationStructure*>& as)
{
    if (as.empty()) return;

    static auto features = instance.getPhysicalDevice().physicalDevice.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR
    >().get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();

    // Create a temporary scratch buffer
    const ui32 scratchSize = [&as] {
        ui32 result{ 0 };
        for (auto& blas : as) {
            result += blas->getBuildSize().buildScratchSize;
        }
        return result;
    }();
    MemoryPool scratchPool(instance.getDevice(), scratchSize,
                                vk::MemoryAllocateFlagBits::eDeviceAddress);
    std::vector<DeviceLocalBuffer> scratchBuffers;

    // Collect build infos
    std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> geoBuildInfos;
    for (auto& blas : as)
    {
        DeviceLocalBuffer& scratchBuffer = scratchBuffers.emplace_back(
            instance.getDevice(),
            blas->getBuildSize().buildScratchSize, nullptr,
            vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
            scratchPool.makeAllocator()
        );
        auto info = blas->getGeometryBuildInfo();
        geoBuildInfos.push_back(
            info.setScratchData(instance.getDevice()->getBufferAddress({ *scratchBuffer }))
                .setDstAccelerationStructure(**blas)
        );
    }

    // Collect build ranges for all acceleration structures
    std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> buildRangePointers;
    std::vector<std::unique_ptr<std::vector<vk::AccelerationStructureBuildRangeInfoKHR>>> buildRanges;
    for (auto& blas : as)
    {
        auto [ranges, ptr] = blas->makeBuildRanges();
        buildRangePointers.insert(buildRangePointers.end(), ptr.begin(), ptr.end());
        buildRanges.push_back(std::move(ranges));
    }

    // Decide whether the AS should be built on the host or on the device.
    // Build on the host if possible.
    if (features.accelerationStructureHostCommands)
    {
        [[maybe_unused]]
        vk::Result result = instance.getDevice()->buildAccelerationStructuresKHR(
            {}, // optional deferred operation
            geoBuildInfos,
            buildRangePointers,
            instance.getDL()
        );
        assert(result == vk::Result::eSuccess);
    }
    else
    {
        instance.getDevice().executeCommands(
            QueueType::compute,
            [&](vk::CommandBuffer cmdBuf)
            {
                cmdBuf.buildAccelerationStructuresKHR(
                    geoBuildInfos,
                    buildRangePointers,
                    instance.getDL()
                );
            }
        );
    }
}
