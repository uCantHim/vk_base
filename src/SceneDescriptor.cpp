#include "trc/SceneDescriptor.h"

#include <trc_util/Padding.h>

#include "trc/DescriptorSetUtils.h"
#include "trc/LightSceneModule.h"
#include "trc/RaySceneModule.h"
#include "trc/core/SceneBase.h"
#include "trc/ray_tracing/RayPipelineBuilder.h"



trc::SceneDescriptor::SceneDescriptor(const Device& device)
    :
    device(device),
    lightBuffer(
        device,
        util::sizeof_pad_16_v<LightDeviceData> * 128,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
    ),
    lightBufferMap(lightBuffer.map()),
    drawableDataBuf(
        device,
        200 * sizeof(RaySceneModule::RayInstanceData),
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible
    ),
    drawableBufferMap(drawableDataBuf.map<std::byte*>())
{
    createDescriptors();
    writeDescriptors();
}

void trc::SceneDescriptor::update(const SceneBase& scene)
{
    if (auto mod = scene.tryGetModule<LightSceneModule>()) {
        updateLightData(*mod);
    }
    if (auto mod = scene.tryGetModule<RaySceneModule>()) {
        updateRayData(*mod);
    }
}

void trc::SceneDescriptor::updateLightData(const LightSceneModule& lights)
{
    // Resize light buffer if the current one is too small
    if (lights.getRequiredLightDataSize() > lightBuffer.size())
    {
        lightBuffer.unmap();

        // Create new buffer
        lightBuffer = Buffer(
            device,
            lights.getRequiredLightDataSize(),
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        lightBufferMap = lightBuffer.map();

        // Update descriptor
        vk::DescriptorBufferInfo lightBufferInfo(*lightBuffer, 0, VK_WHOLE_SIZE);
        std::vector<vk::WriteDescriptorSet> writes = {
            { *descSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &lightBufferInfo },
        };
        device->updateDescriptorSets(writes, {});
    }

    // Update light data
    lights.writeLightData(lightBufferMap);
}

void trc::SceneDescriptor::updateRayData(const RaySceneModule& scene)
{
    const size_t dataSize = scene.getMaxRayDeviceDataSize();
    if (dataSize > drawableDataBuf.size())
    {
        drawableDataBuf.unmap();
        drawableDataBuf = Buffer(
            device,
            glm::max(dataSize, drawableDataBuf.size() * 2),
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible
        );
        drawableBufferMap = drawableDataBuf.map<std::byte*>();

        vk::DescriptorBufferInfo bufferInfo(*drawableDataBuf, 0, VK_WHOLE_SIZE);
        std::vector<vk::WriteDescriptorSet> writes = {
            { *descSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &bufferInfo },
        };
        device->updateDescriptorSets(writes, {});
    }

    scene.writeRayDeviceData(drawableBufferMap, dataSize);
    drawableDataBuf.flush();
}

auto trc::SceneDescriptor::getDescriptorSetLayout() const noexcept
    -> vk::DescriptorSetLayout
{
    return *descLayout;
}

void trc::SceneDescriptor::bindDescriptorSet(
    vk::CommandBuffer cmdBuf,
    vk::PipelineBindPoint bindPoint,
    vk::PipelineLayout pipelineLayout,
    ui32 setIndex) const
{
    cmdBuf.bindDescriptorSets(
        bindPoint,
        pipelineLayout,
        setIndex, *descSet,
        {}
    );
}

void trc::SceneDescriptor::createDescriptors()
{
    vk::ShaderStageFlags shaderStages = vk::ShaderStageFlagBits::eFragment
                                        | vk::ShaderStageFlagBits::eCompute
                                        | rt::ALL_RAY_PIPELINE_STAGE_FLAGS;

    // Layout
    descLayout = buildDescriptorSetLayout()
        .addFlag(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool)
        // Shadow data buffer
        .addBinding(vk::DescriptorType::eStorageBuffer, 1, shaderStages,
                    vk::DescriptorBindingFlagBits::eUpdateAfterBind)
        // Ray tracing related drawable data buffer
        .addBinding(vk::DescriptorType::eStorageBuffer, 1, shaderStages,
                    vk::DescriptorBindingFlagBits::eUpdateAfterBind)
        .build(device);

    // Pool
    std::vector<vk::DescriptorPoolSize> poolSizes{
        { vk::DescriptorType::eStorageBuffer, 1 },
    };
    descPool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
        | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
        1, poolSizes
    ));

    // Sets
    descSet = std::move(device->allocateDescriptorSetsUnique(
        vk::DescriptorSetAllocateInfo(*descPool, 1, &*descLayout)
    )[0]);
}

void trc::SceneDescriptor::writeDescriptors()
{
    vk::DescriptorBufferInfo lightBufferInfo(*lightBuffer, 0, VK_WHOLE_SIZE);
    vk::DescriptorBufferInfo drawableBufferInfo(*drawableDataBuf, 0, VK_WHOLE_SIZE);

    std::vector<vk::WriteDescriptorSet> writes = {
        { *descSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &lightBufferInfo },
        { *descSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &drawableBufferInfo },
    };
    device->updateDescriptorSets(writes, {});
}
