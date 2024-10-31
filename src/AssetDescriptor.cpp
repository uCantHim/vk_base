#include "trc/AssetDescriptor.h"

#include "trc/assets/AnimationRegistry.h"
#include "trc/assets/AssetRegistry.h"
#include "trc/assets/AssetRegistry.h"
#include "trc/assets/GeometryRegistry.h"
#include "trc/assets/MaterialRegistry.h"
#include "trc/assets/RigRegistry.h"
#include "trc/assets/TextureRegistry.h"
#include "trc/base/ImageUtils.h"
#include "trc/core/Instance.h"
#include "trc/ray_tracing/RayPipelineBuilder.h" // For ALL_RAY_PIPELINE_STAGE_FLAGS
#include "trc/text/Font.h"



namespace trc
{

struct AssetRegistryCreateInfo
{
    vk::BufferUsageFlags geometryBufferUsage{};

    vk::ShaderStageFlags materialDescriptorStages{};
    vk::ShaderStageFlags textureDescriptorStages{};
    vk::ShaderStageFlags geometryDescriptorStages{};
};

auto makeDefaultDescriptorUsageSettings(const bool enableRayTracing)
    -> AssetRegistryCreateInfo
{
    AssetRegistryCreateInfo info{
        .materialDescriptorStages = vk::ShaderStageFlagBits::eFragment
                                           | vk::ShaderStageFlagBits::eCompute,
        .textureDescriptorStages = vk::ShaderStageFlagBits::eFragment
                                          | vk::ShaderStageFlagBits::eCompute,
    };

    if (enableRayTracing)
    {
        info.geometryBufferUsage |=
            vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;

        info.materialDescriptorStages |= rt::ALL_RAY_PIPELINE_STAGE_FLAGS;
        info.textureDescriptorStages  |= rt::ALL_RAY_PIPELINE_STAGE_FLAGS;
        info.geometryDescriptorStages |= rt::ALL_RAY_PIPELINE_STAGE_FLAGS;
    }

    return info;
}

auto makeAssetDescriptor(
    const Instance& instance,
    AssetRegistry& registry,
    const AssetDescriptorCreateInfo& descriptorCreateInfo) -> s_ptr<AssetDescriptor>
{
    const Device& device = instance.getDevice();
    const auto config = makeDefaultDescriptorUsageSettings(instance.hasRayTracing());
    auto desc = std::make_shared<AssetDescriptor>(device, descriptorCreateInfo);

    try {
        // Add modules in the order in which they should be destroyed
        registry.addModule<Material>(std::make_unique<MaterialRegistry>());
        registry.addModule<Texture>(std::make_unique<TextureRegistry>(
            TextureRegistryCreateInfo{
                device,
                desc->getBinding(AssetDescriptorBinding::eTextureSamplers)
            }
        ));
        registry.addModule<Geometry>(std::make_unique<GeometryRegistry>(
            GeometryRegistryCreateInfo{
                .instance                = instance,
                .indexDescriptorBinding  = desc->getBinding(AssetDescriptorBinding::eGeometryIndexBuffers),
                .vertexDescriptorBinding = desc->getBinding(AssetDescriptorBinding::eGeometryVertexBuffers),
                .geometryBufferUsage     = config.geometryBufferUsage,
                .enableRayTracing        = instance.hasRayTracing(),
            }
        ));
        registry.addModule<Rig>(std::make_unique<RigRegistry>());
        registry.addModule<Animation>(std::make_unique<AnimationRegistry>(
            AnimationRegistryCreateInfo{
                .device = device,
                .metadataDescBinding = desc->getBinding(AssetDescriptorBinding::eAnimationMetadata),
                .dataDescBinding = desc->getBinding(AssetDescriptorBinding::eAnimationData),
            }
        ));
        registry.addModule<Font>(std::make_unique<FontRegistry>(
            FontRegistryCreateInfo{
                .device = device,
                .glyphMapBinding = desc->getBinding(AssetDescriptorBinding::eGlyphMapSamplers)
            }
        ));

        // Add default assets
        registry.add<Texture>(std::make_unique<InMemorySource<Texture>>(
            TextureData{ { 1, 1 }, makeSinglePixelImageData(vec4(1.0f)).pixels }
        ));
    }
    catch (std::out_of_range& err) {
        throw std::invalid_argument("[In makeDefaultAssetModules]: Don't call this function"
                                    " more than once for the same asset registry!"
                                    " (Internal error: " + std::string(err.what()));
    }

    return desc;
}



//////////////////////////////////
//    Asset descriptor class    //
//////////////////////////////////

AssetDescriptor::AssetDescriptor(const Device& device, const AssetDescriptorCreateInfo& info)
{
    auto builder = buildSharedDescriptorSet();
    builder.addLayoutFlag(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
    builder.addPoolFlag(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

    // Geometry related bindings
    bindings.emplace(AssetDescriptorBinding::eGeometryIndexBuffers, builder.addBinding(
        vk::DescriptorType::eStorageBuffer,
        info.maxGeometries,
        rt::ALL_RAY_PIPELINE_STAGE_FLAGS,
        vk::DescriptorBindingFlagBits::ePartiallyBound
            | vk::DescriptorBindingFlagBits::eUpdateAfterBind
    ));
    bindings.emplace(AssetDescriptorBinding::eGeometryVertexBuffers, builder.addBinding(
        vk::DescriptorType::eStorageBuffer,
        info.maxGeometries,
        rt::ALL_RAY_PIPELINE_STAGE_FLAGS,
        vk::DescriptorBindingFlagBits::ePartiallyBound
            | vk::DescriptorBindingFlagBits::eUpdateAfterBind
    ));

    // Texture related bindings
    bindings.emplace(AssetDescriptorBinding::eTextureSamplers, builder.addBinding(
        vk::DescriptorType::eCombinedImageSampler,
        info.maxTextures,
        vk::ShaderStageFlagBits::eAll,
        vk::DescriptorBindingFlagBits::ePartiallyBound
            | vk::DescriptorBindingFlagBits::eUpdateAfterBind
    ));

    // Font related bindings
    bindings.emplace(AssetDescriptorBinding::eGlyphMapSamplers, builder.addBinding(
        vk::DescriptorType::eCombinedImageSampler,
        info.maxFonts,
        vk::ShaderStageFlagBits::eFragment,
        vk::DescriptorBindingFlagBits::ePartiallyBound
            | vk::DescriptorBindingFlagBits::eUpdateAfterBind
    ));

    // Animation related bindings
    bindings.emplace(AssetDescriptorBinding::eAnimationMetadata, builder.addBinding(
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute,
        vk::DescriptorBindingFlagBits::eUpdateAfterBind
    ));
    bindings.emplace(AssetDescriptorBinding::eAnimationData, builder.addBinding(
        vk::DescriptorType::eStorageBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eCompute,
        vk::DescriptorBindingFlagBits::eUpdateAfterBind
    ));

    descSet = builder.build(device);

    // Assert that the descriptors have been added in the correct order
    auto assertIndex = [this](AssetDescriptorBinding binding) {
        assert(bindings.at(binding).getBindingIndex() == getBindingIndex(binding));
    };
    assertIndex(AssetDescriptorBinding::eGeometryIndexBuffers);
    assertIndex(AssetDescriptorBinding::eGeometryVertexBuffers);
    assertIndex(AssetDescriptorBinding::eTextureSamplers);
    assertIndex(AssetDescriptorBinding::eGlyphMapSamplers);
    assertIndex(AssetDescriptorBinding::eAnimationMetadata);
    assertIndex(AssetDescriptorBinding::eAnimationData);
}

void AssetDescriptor::update(const Device& device)
{
    descSet->update(device);
}

auto AssetDescriptor::getBinding(AssetDescriptorBinding binding) -> Binding
{
    return bindings.at(binding);
}

auto AssetDescriptor::getBinding(AssetDescriptorBinding binding) const -> const Binding
{
    return bindings.at(binding);
}

auto AssetDescriptor::getDescriptorSetLayout() const noexcept -> vk::DescriptorSetLayout
{
    return descSet->getDescriptorSetLayout();
}

void AssetDescriptor::bindDescriptorSet(
    vk::CommandBuffer cmdBuf,
    vk::PipelineBindPoint bindPoint,
    vk::PipelineLayout pipelineLayout,
    ui32 setIndex) const
{
    descSet->getProvider()->bindDescriptorSet(cmdBuf, bindPoint, pipelineLayout, setIndex);
}

} // namespace trc
