#include "RenderPassShadow.h"

#include "PipelineDefinitions.h"
#include <vulkan/vulkan.hpp>



trc::RenderPassShadow::RenderPassShadow(uvec2 resolution, const mat4& projMatrix, Light& light)
    :
    RenderPass(
        [&]()
        {
            std::vector<vk::AttachmentDescription> attachments{
                vk::AttachmentDescription(
                    {},
                    vk::Format::eD24UnormS8Uint,
                    vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal
                )
            };

            vk::AttachmentReference depthRef(0, vk::ImageLayout::eDepthStencilAttachmentOptimal);
            std::vector<vk::SubpassDescription> subpasses{
                vk::SubpassDescription(
                    {},
                    vk::PipelineBindPoint::eGraphics,
                    0, nullptr,
                    0, nullptr,
                    nullptr,
                    &depthRef
                ),
            };

            std::vector<vk::SubpassDependency> dependencies{
                vk::SubpassDependency(
                    VK_SUBPASS_EXTERNAL, 0,
                    vk::PipelineStageFlagBits::eAllCommands,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::AccessFlags(),
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead,
                    vk::DependencyFlagBits::eByRegion
                ),
            };

            return vkb::getDevice()->createRenderPassUnique(
                vk::RenderPassCreateInfo({}, attachments, subpasses, dependencies)
            );
        }(),
        1
    ),
    light(&light),
    resolution(resolution),
    projMatrix(projMatrix),
    depthImages([&resolution](ui32) {
        return vkb::Image(
            vk::ImageCreateInfo(
                {},
                vk::ImageType::e2D,
                vk::Format::eD24UnormS8Uint,
                vk::Extent3D(resolution.x, resolution.y, 1),
                1, 1,
                vk::SampleCountFlagBits::e1,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
            )
        );
    }),
    depthImageViews([&](ui32 imageIndex) {
        return depthImages.getAt(imageIndex).createView(
            vk::ImageViewType::e2D, vk::Format::eD24UnormS8Uint, {},
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1)
        );
    }),
    framebuffers([&](ui32 imageIndex) {
        vk::ImageView depthView = *depthImageViews.getAt(imageIndex);
        return vkb::getDevice()->createFramebufferUnique(
            vk::FramebufferCreateInfo(
                {},
                *renderPass,
                depthView,
                resolution.x, resolution.y, 1
            )
        );
    })
{
    shadowDescriptorIndex = ShadowDescriptor::addShadow(
        vkb::FrameSpecificObject<vk::Sampler>{ [&](ui32 imageIndex) {
            return depthImages.getAt(imageIndex).getDefaultSampler();
        }},
        vkb::FrameSpecificObject<vk::ImageView>{ [&](ui32 imageIndex) {
            return *depthImageViews.getAt(imageIndex);
        }},
        projMatrix * getGlobalTransform()
    );

    light.hasShadow = true;
    light.firstShadowIndex = shadowDescriptorIndex;
}

trc::RenderPassShadow::~RenderPassShadow()
{
    ShadowDescriptor::removeShadow(shadowDescriptorIndex);
    light->hasShadow = false;
}

void trc::RenderPassShadow::begin(vk::CommandBuffer cmdBuf, vk::SubpassContents subpassContents)
{
    ShadowDescriptor::updateShadow(shadowDescriptorIndex, projMatrix * getGlobalTransform());

    depthImages->changeLayout(
        cmdBuf,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        { vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1 }
    );

    vk::ClearValue clearValue{ vk::ClearDepthStencilValue(1.0f, 0) };
    cmdBuf.beginRenderPass(
        vk::RenderPassBeginInfo(
            *renderPass,
            **framebuffers,
            vk::Rect2D({ 0, 0 }, { resolution.x, resolution.y }),
            clearValue
        ),
        subpassContents
    );
}

void trc::RenderPassShadow::end(vk::CommandBuffer cmdBuf)
{
    cmdBuf.endRenderPass();
}

auto trc::RenderPassShadow::getResolution() const noexcept -> uvec2
{
    return resolution;
}



auto trc::ShadowDescriptor::getProvider() noexcept -> const DescriptorProviderInterface&
{
    return *descProvider;
}

auto trc::ShadowDescriptor::getNewIndex() -> ui32
{
    if (freeIndices.empty()) {
        return nextIndex++;
    }

    ui32 result = freeIndices.back();
    freeIndices.pop_back();
    return result;
}

auto trc::ShadowDescriptor::addShadow(
    const vkb::FrameSpecificObject<vk::Sampler>& samplers,
    const vkb::FrameSpecificObject<vk::ImageView>& views,
    const mat4& viewProjMatrix) -> ui32
{
    ui32 newIndex = getNewIndex();

    // Update shadow matrix buffer
    auto buf = reinterpret_cast<mat4*>(shadowMatrixBuffer.map());
    buf[newIndex] = viewProjMatrix;
    shadowMatrixBuffer.unmap();

    // Update image descriptor
    ui32 imageIndex{ 0 };
    descSet->foreach([&](vk::UniqueDescriptorSet& set)
    {
        vk::DescriptorImageInfo imageInfo(
            samplers.getAt(imageIndex),
            views.getAt(imageIndex),
            vk::ImageLayout::eShaderReadOnlyOptimal);
        std::vector<vk::WriteDescriptorSet> writes{
            vk::WriteDescriptorSet(
                *set, 1, newIndex,
                1,
                vk::DescriptorType::eCombinedImageSampler,
                &imageInfo
            ),
        };
        vkb::getDevice()->updateDescriptorSets(writes, {});

        imageIndex++;
    });

    return newIndex;
}

void trc::ShadowDescriptor::updateShadow(ui32 shadowIndex, const mat4& viewProjMatrix)
{
    auto buf = reinterpret_cast<mat4*>(shadowMatrixBuffer.map());
    buf[shadowIndex] = viewProjMatrix;
    shadowMatrixBuffer.unmap();
}

void trc::ShadowDescriptor::removeShadow(ui32 shadowIndex)
{
    freeIndices.push_back(shadowIndex);
}

void trc::ShadowDescriptor::init()
{
    // Shadow matrix buffer
    shadowMatrixBuffer = {
        sizeof(mat4) * MAX_SHADOW_MAPS,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    };

    // Pool
    descPool = vkb::getDevice()->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
        | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
        vkb::getSwapchain().getFrameCount(),
        std::vector<vk::DescriptorPoolSize>{
            { vk::DescriptorType::eStorageBuffer, 1 },
            { vk::DescriptorType::eCombinedImageSampler, MAX_SHADOW_MAPS },
        }
    ));

    // Layout
    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings{
        // Shadow matrix buffer
        vk::DescriptorSetLayoutBinding(
            0, vk::DescriptorType::eStorageBuffer, 1,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            nullptr
        ),
        // Shadow samplers
        vk::DescriptorSetLayoutBinding(
            1, vk::DescriptorType::eCombinedImageSampler, MAX_SHADOW_MAPS,
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
            nullptr
        ),
    };
    std::vector<vk::DescriptorBindingFlags> bindingFlags{
        {},
        vk::DescriptorBindingFlagBits::eVariableDescriptorCount
        | vk::DescriptorBindingFlagBits::eUpdateAfterBind
    };
    vk::StructureChain layoutChain{
        vk::DescriptorSetLayoutCreateInfo(
            vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            layoutBindings
        ),
        vk::DescriptorSetLayoutBindingFlagsCreateInfo(bindingFlags)
    };
    descLayout = vkb::getDevice()->createDescriptorSetLayoutUnique(
        layoutChain.get<vk::DescriptorSetLayoutCreateInfo>()
    );

    // Sets
    std::vector<vk::DescriptorSetLayout> layouts;
    for (ui32 i = 0; i < vkb::getSwapchain().getFrameCount(); i++) {
        layouts.push_back(*descLayout);
    }
    descSet = std::make_unique<vkb::FrameSpecificObject<vk::UniqueDescriptorSet>>(
        vkb::getDevice()->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo(
            *descPool,
            vkb::getSwapchain().getFrameCount(),
            layouts.data()
        ))
    );

    vkb::FrameSpecificObject<vk::DescriptorSet> sets{ [](ui32 i) { return *descSet->getAt(i); } };
    descProvider = std::make_unique<FrameSpecificDescriptorProvider>(*descLayout, std::move(sets));

    // Write descriptors
    descSet->foreach([&](vk::UniqueDescriptorSet& set)
    {
        vk::DescriptorBufferInfo bufferInfo(*shadowMatrixBuffer, 0, VK_WHOLE_SIZE);
        std::vector<vk::WriteDescriptorSet> writes{
            vk::WriteDescriptorSet(
                *set, 0, 0, 1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &bufferInfo,
                nullptr
            ),
        };
        vkb::getDevice()->updateDescriptorSets(writes, {});
    });
}

void trc::ShadowDescriptor::destroy()
{
    descSet.reset();
    descLayout.reset();
    descPool.reset();
}
