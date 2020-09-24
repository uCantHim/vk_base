#include "RenderStageDeferred.h"

#include "utils/Util.h"
#include "PipelineDefinitions.h"



trc::DeferredStage::DeferredStage()
    :
    RenderStage(NUM_DEFERRED_SUBPASSES)
{
    RenderPass::create<RenderPassDeferred>(internal::RenderPasses::eDeferredPass);
    addRenderPass(internal::RenderPasses::eDeferredPass);
}



trc::RenderPassDeferred::RenderPassDeferred()
    :
    RenderPass(
        [&]()
        {
            std::vector<vk::AttachmentDescription> attachments = {
                // Vertex positions
                vk::AttachmentDescription(
                    {}, vk::Format::eR16G16B16A16Sfloat, vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
                ),
                // Normals
                vk::AttachmentDescription(
                    {}, vk::Format::eR16G16B16A16Sfloat, vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
                ),
                // UVs
                vk::AttachmentDescription(
                    {}, vk::Format::eR16G16Sfloat, vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
                ),
                // Material indices
                vk::AttachmentDescription(
                    {}, vk::Format::eR32Uint, vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal
                ),
                // Depth-/Stencil buffer
                vk::AttachmentDescription(
                    vk::AttachmentDescriptionFlags(),
                    vk::Format::eD24UnormS8Uint,
                    vk::SampleCountFlagBits::e1,
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, // load/store ops
                    vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, // stencil ops
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal
                ),
                // Swapchain images
                trc::makeDefaultSwapchainColorAttachment(vkb::VulkanBase::getSwapchain()),
            };

            std::vector<vk::AttachmentReference> deferredAttachments = {
                { 0, vk::ImageLayout::eColorAttachmentOptimal }, // Vertex positions
                { 1, vk::ImageLayout::eColorAttachmentOptimal }, // Normals
                { 2, vk::ImageLayout::eColorAttachmentOptimal }, // UVs
                { 3, vk::ImageLayout::eColorAttachmentOptimal }, // Material indices
                { 4, vk::ImageLayout::eDepthStencilAttachmentOptimal }, // Depth buffer
            };

            std::vector<vk::AttachmentReference> transparencyAttachments{
                { 4, vk::ImageLayout::eDepthStencilAttachmentOptimal }, // Depth buffer
            };
            std::vector<ui32> transparencyPreservedAttachments{ 0, 1, 2, 3 };

            std::vector<vk::AttachmentReference> lightingAttachments = {
                { 0, vk::ImageLayout::eShaderReadOnlyOptimal }, // Vertex positions
                { 1, vk::ImageLayout::eShaderReadOnlyOptimal }, // Normals
                { 2, vk::ImageLayout::eShaderReadOnlyOptimal }, // UVs
                { 3, vk::ImageLayout::eShaderReadOnlyOptimal }, // Material indices
                { 5, vk::ImageLayout::eColorAttachmentOptimal }, // Swapchain images
            };

            std::vector<vk::SubpassDescription> subpasses = {
                // Deferred diffuse subpass
                vk::SubpassDescription(
                    vk::SubpassDescriptionFlags(),
                    vk::PipelineBindPoint::eGraphics,
                    0, nullptr,
                    4, &deferredAttachments[0],
                    nullptr, // resolve attachments
                    &deferredAttachments[4]
                ),
                // Deferred transparency subpass
                vk::SubpassDescription(
                    vk::SubpassDescriptionFlags(),
                    vk::PipelineBindPoint::eGraphics,
                    0, nullptr, // input attachments
                    0, nullptr, // color attachments
                    nullptr,    // resolve attachments
                    &transparencyAttachments[0], // depth attachment (read-only)
                    4, transparencyPreservedAttachments.data() // preserve the deferred attachments
                ),
                // Final lighting subpass
                vk::SubpassDescription(
                    vk::SubpassDescriptionFlags(),
                    vk::PipelineBindPoint::eGraphics,
                    4, &lightingAttachments[0],
                    1, &lightingAttachments[4],
                    nullptr, // resolve attachments
                    nullptr
                ),
            };

            std::vector<vk::SubpassDependency> dependencies = {
                vk::SubpassDependency(
                    VK_SUBPASS_EXTERNAL, 0,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::AccessFlags(),
                    vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::DependencyFlags()
                ),
                vk::SubpassDependency(
                    0, 1,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits::eDepthStencilAttachmentRead,
                    vk::DependencyFlagBits::eByRegion
                ),
                vk::SubpassDependency(
                    1, 2,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::PipelineStageFlagBits::eAllGraphics,
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::AccessFlagBits::eInputAttachmentRead,
                    vk::DependencyFlagBits::eByRegion
                )
            };

            return vkb::getDevice()->createRenderPassUnique(
                vk::RenderPassCreateInfo(
                    vk::RenderPassCreateFlags(),
                    static_cast<ui32>(attachments.size()), attachments.data(),
                    static_cast<ui32>(subpasses.size()), subpasses.data(),
                    static_cast<ui32>(dependencies.size()), dependencies.data()
                )
            );
        }(),
        NUM_DEFERRED_SUBPASSES
    ), // Base class RenderPass constructor
    depthPixelReadBuffer(
        sizeof(vec4),
        vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eDeviceLocal
    ),
    framebuffers([&](ui32 frameIndex)
    {
        constexpr size_t numAttachments = 5;

        const auto swapchainExtent = vkb::getSwapchain().getImageExtent();
        auto& images = attachmentImages.emplace_back();
        images.reserve(numAttachments);
        auto& imageViews = attachmentImageViews.emplace_back();

        auto& positionImage = images.emplace_back(vk::ImageCreateInfo(
            {}, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat,
            vk::Extent3D{ swapchainExtent, 1 },
            1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
        ));
        auto& normalImage = images.emplace_back(vk::ImageCreateInfo(
            {}, vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat,
            vk::Extent3D{ swapchainExtent, 1 },
            1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
            | vk::ImageUsageFlagBits::eTransferSrc
        ));
        auto& uvImage = images.emplace_back(vk::ImageCreateInfo(
            {}, vk::ImageType::e2D, vk::Format::eR16G16Sfloat,
            vk::Extent3D{ swapchainExtent, 1 },
            1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
        ));
        auto& materialImage = images.emplace_back(vk::ImageCreateInfo(
            {}, vk::ImageType::e2D, vk::Format::eR32Uint,
            vk::Extent3D{ swapchainExtent, 1 },
            1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
        ));
        auto& depthImage = images.emplace_back(vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D,
            vk::Format::eD24UnormS8Uint,
            vk::Extent3D{ swapchainExtent, 1 },
            1, 1, vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc
        ));

        auto positionView = *imageViews.emplace_back(
            positionImage.createView(vk::ImageViewType::e2D, vk::Format::eR16G16B16A16Sfloat, {})
        );
        auto normalView = *imageViews.emplace_back(
            normalImage.createView(vk::ImageViewType::e2D, vk::Format::eR16G16B16A16Sfloat, {})
        );
        auto uvView = *imageViews.emplace_back(
            uvImage.createView(vk::ImageViewType::e2D, vk::Format::eR16G16Sfloat, {})
        );
        auto materialView = *imageViews.emplace_back(
            materialImage.createView(vk::ImageViewType::e2D, vk::Format::eR32Uint, {})
        );
        auto depthView = *imageViews.emplace_back(
            depthImage.createView(
                vk::ImageViewType::e2D, vk::Format::eD24UnormS8Uint, vk::ComponentMapping(),
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1)
            )
        );
        auto colorOutputView = vkb::getSwapchain().getImageView(frameIndex);

        std::vector<vk::ImageView> attachments = {
            positionView,
            normalView,
            uvView,
            materialView,
            depthView,
            colorOutputView
        };

        framebufferSize = vk::Extent2D{ swapchainExtent.width, swapchainExtent.height };
        return vkb::getDevice()->createFramebufferUnique(
            vk::FramebufferCreateInfo(
                {},
                *renderPass,
                attachments,
                framebufferSize.width, framebufferSize.height, 1
            )
        );
    }),
    clearValues({})
{
    clearValues = {
        vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }),
        vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }),
        vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f }),
        vk::ClearColorValue(std::array<ui32, 4>{ UINT32_MAX, 0, 0, 0 }),
        vk::ClearDepthStencilValue(1.0f, 0.0f),
        vk::ClearColorValue(std::array<float, 4>{ 0.5f, 0.0f, 1.0f, 0.0f }),
    };

    DeferredRenderPassDescriptor::init(*this);
}

void trc::RenderPassDeferred::begin(vk::CommandBuffer cmdBuf, vk::SubpassContents subpassContents)
{
    readMouseDepthValueFromBuffer();

    // Bring attachment images in color attachment layout
    ui32 imageIndex = vkb::getSwapchain().getCurrentFrame();
    auto& images = attachmentImages[imageIndex];
    images[0].changeLayout(cmdBuf, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eColorAttachmentOptimal);
    images[1].changeLayout(cmdBuf, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eColorAttachmentOptimal);
    images[2].changeLayout(cmdBuf, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eColorAttachmentOptimal);
    images[3].changeLayout(cmdBuf, vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eColorAttachmentOptimal);
    images[4].changeLayout(
        cmdBuf,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
            0, 1, 0, 1
        )
    );
    DeferredRenderPassDescriptor::resetValues(cmdBuf);

    cmdBuf.beginRenderPass(
        vk::RenderPassBeginInfo(
            *renderPass,
            **framebuffers,
            vk::Rect2D(
                { 0, 0 },
                framebufferSize
            ),
            static_cast<ui32>(clearValues.size()), clearValues.data()
        ),
        subpassContents
    );
}

void trc::RenderPassDeferred::end(vk::CommandBuffer cmdBuf)
{
    cmdBuf.endRenderPass();
    copyMouseDepthValueToBuffer(cmdBuf);
}

auto trc::RenderPassDeferred::getAttachmentImageViews(ui32 imageIndex) const noexcept
   -> const std::vector<vk::UniqueImageView>&
{
    return attachmentImageViews[imageIndex];
}

auto trc::RenderPassDeferred::getInputAttachmentDescriptor() const noexcept
    -> const DescriptorProviderInterface&
{
    return DeferredRenderPassDescriptor::getProvider();
}

auto trc::RenderPassDeferred::getMouseDepthValue() -> float
{
    return mouseDepthValue;
}

void trc::RenderPassDeferred::copyMouseDepthValueToBuffer(vk::CommandBuffer cmdBuf)
{
    vkb::Image& depthImage = attachmentImages[vkb::getSwapchain().getCurrentFrame()][4];
    const vk::Extent3D size = depthImage.getSize();
    const vec2 mousePos = vkb::getSwapchain().getMousePosition();
    if (mousePos.x < 0 || mousePos.y < 0
        || mousePos.x >= size.width || mousePos.y >= size.height)
    {
        return;
    }

    depthImage.changeLayout(
        cmdBuf,
        vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal,
        { vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1 }
    );
    cmdBuf.copyImageToBuffer(
        *depthImage, vk::ImageLayout::eTransferSrcOptimal,
        *depthPixelReadBuffer,
        vk::BufferImageCopy(
            0, // buffer offset
            0, 0, // some weird 2D or 3D offsets, idk
            vk::ImageSubresourceLayers(
                vk::ImageAspectFlagBits::eDepth,
                0, 0, 1
            ),
            { static_cast<i32>(mousePos.x), static_cast<i32>(mousePos.y), 0 },
            { 1, 1, 1 }
        )
    );
}

void trc::RenderPassDeferred::readMouseDepthValueFromBuffer()
{
    auto buf = reinterpret_cast<ui32*>(depthPixelReadBuffer.map());
    ui32 depthValueD24S8 = buf[0];
    depthPixelReadBuffer.unmap();

    // Don't ask me why 16 bit here, I think it should be 24. The result
    // seems to be correct with 16 though.
    constexpr float maxFloat16 = 65536.0f; // 2**16 -- std::pow is not constexpr
    mouseDepthValue = static_cast<float>(depthValueD24S8 >> 8) / maxFloat16;
}



auto trc::getMouseDepth() -> float
{
    return RenderPassDeferred::getMouseDepthValue();
}

auto trc::getMouseWorldPos(const Camera& camera) -> vec3
{
    const float depth = getMouseDepth();
    const auto windowSize = vkb::getSwapchain().getImageExtent();

    return glm::unProject(
        vec3(vkb::getSwapchain().getMousePosition(), depth),
        camera.getViewMatrix(),
        camera.getProjectionMatrix(),
        vec4(0.0f, 0.0f, windowSize.width, windowSize.height)
    );
}



// -------------------------------- //
//       Deferred descriptor        //
// -------------------------------- //

vkb::StaticInit trc::DeferredRenderPassDescriptor::_init{
    []() {},
    []() {
        provider.reset();
        descSets.reset();
        descLayout.reset();
        descPool.reset();

        fragmentListBuffer.reset();
        fragmentListHeadPointerImageView.reset();
        fragmentListHeadPointerImage.reset();
    }
};

void trc::DeferredRenderPassDescriptor::init(const RenderPassDeferred& renderPass)
{
    descSets.reset();
    descLayout.reset();
    descPool.reset();

    // Create buffers
    constexpr ui32 MAX_FRAGS_PER_PIXEL{ 4 };
    const auto swapchainSize = vkb::getSwapchain().getImageExtent();
    const ui32 ATOMIC_BUFFER_SECTION_SIZE = util::pad(
        sizeof(ui32), vkb::getPhysicalDevice().properties.limits.minStorageBufferOffsetAlignment);
    const ui32 FRAGMENT_LIST_SIZE = sizeof(uvec4) * MAX_FRAGS_PER_PIXEL
                                    * swapchainSize.width * swapchainSize.height;
    std::vector<vk::UniqueImageView> imageViews;
    fragmentListHeadPointerImage.reset(new vkb::FrameSpecificObject<vkb::Image>{ [&](ui32) {
        vkb::Image result(vk::ImageCreateInfo(
            {},
            vk::ImageType::e2D, vk::Format::eR32Uint,
            vk::Extent3D(swapchainSize.width, swapchainSize.height, 1),
            1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eStorage
        ));
        result.changeLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
        imageViews.push_back(result.createView(vk::ImageViewType::e2D, vk::Format::eR32Uint));

        // Clear image
        auto cmdBuf = vkb::getDevice().createGraphicsCommandBuffer();
        cmdBuf->begin(vk::CommandBufferBeginInfo());
        cmdBuf->clearColorImage(
            *result, vk::ImageLayout::eGeneral,
            vk::ClearColorValue(std::array<ui32, 4>{ ~0u }),
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );
        cmdBuf->end();
        vkb::getDevice().executeGraphicsCommandBufferSynchronously(*cmdBuf);

        return result;
    }});
    fragmentListHeadPointerImageView.reset(new vkb::FrameSpecificObject<vk::UniqueImageView>{
        std::move(imageViews)
    });
    fragmentListBuffer.reset(new vkb::FrameSpecificObject<vkb::Buffer>{ [&](ui32) {
        vkb::Buffer result(
            ATOMIC_BUFFER_SECTION_SIZE + FRAGMENT_LIST_SIZE,
            vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        const ui32 MAX_FRAGS = MAX_FRAGS_PER_PIXEL * swapchainSize.width * swapchainSize.height;
        auto cmdBuf = vkb::getDevice().createTransferCommandBuffer();
        cmdBuf->begin(vk::CommandBufferBeginInfo(vk::CommandBufferBeginInfo()));
        cmdBuf->updateBuffer<ui32>(*result, 0, { 0, MAX_FRAGS, 0, });
        cmdBuf->end();
        vkb::getDevice().executeTransferCommandBufferSyncronously(*cmdBuf);

        return result;
    }});


    // Pool
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eInputAttachment, 4 },
        { vk::DescriptorType::eStorageImage, 1 },
        { vk::DescriptorType::eStorageBuffer, 2 },
    };
    descPool = vkb::getDevice()->createDescriptorPoolUnique(
        vk::DescriptorPoolCreateInfo(
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            vkb::getSwapchain().getFrameCount(), poolSizes)
    );

    // Layout
    std::vector<vk::DescriptorSetLayoutBinding> layoutBindings = {
        // Input attachments
        { 0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        { 3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },
        // Fragment list head pointer image
        { 4, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eFragment },
        // Fragment list allocator
        { 5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },
        // Fragment list
        { 6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },
    };
    descLayout = vkb::getDevice()->createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo({}, layoutBindings)
    );

    // Sets
    descSets = std::make_unique<vkb::FrameSpecificObject<vk::UniqueDescriptorSet>>(
    [&](ui32 imageIndex)
    {
        const auto& imageViews = renderPass.getAttachmentImageViews(imageIndex);

        auto set = std::move(vkb::getDevice()->allocateDescriptorSetsUnique(
            vk::DescriptorSetAllocateInfo(*descPool, *descLayout)
        )[0]);

        // Write set
        std::vector<vk::DescriptorImageInfo> imageInfos = {
            { {}, *imageViews[0], vk::ImageLayout::eShaderReadOnlyOptimal },
            { {}, *imageViews[1], vk::ImageLayout::eShaderReadOnlyOptimal },
            { {}, *imageViews[2], vk::ImageLayout::eShaderReadOnlyOptimal },
            { {}, *imageViews[3], vk::ImageLayout::eShaderReadOnlyOptimal },
            { {}, *fragmentListHeadPointerImageView->getAt(imageIndex), vk::ImageLayout::eGeneral },
        };
        std::vector<vk::DescriptorBufferInfo> bufferInfos{
            { *fragmentListBuffer->getAt(imageIndex),
              0,                          ATOMIC_BUFFER_SECTION_SIZE },
            { *fragmentListBuffer->getAt(imageIndex),
              ATOMIC_BUFFER_SECTION_SIZE, FRAGMENT_LIST_SIZE },
        };
        std::vector<vk::WriteDescriptorSet> writes = {
            { *set, 0, 0, 1, vk::DescriptorType::eInputAttachment, &imageInfos[0] },
            { *set, 1, 0, 1, vk::DescriptorType::eInputAttachment, &imageInfos[1] },
            { *set, 2, 0, 1, vk::DescriptorType::eInputAttachment, &imageInfos[2] },
            { *set, 3, 0, 1, vk::DescriptorType::eInputAttachment, &imageInfos[3] },
            { *set, 4, 0, 1, vk::DescriptorType::eStorageImage, &imageInfos[4] },
            { *set, 5, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &bufferInfos[0] },
            { *set, 6, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &bufferInfos[1] },
        };
        vkb::getDevice()->updateDescriptorSets(writes, {});

        return set;
    });

    provider.reset(new FrameSpecificDescriptorProvider(
        *descLayout,
        vkb::FrameSpecificObject<vk::DescriptorSet>(
            [](ui32 imageIndex) { return *descSets->getAt(imageIndex); }
        )
    ));
}

void trc::DeferredRenderPassDescriptor::resetValues(vk::CommandBuffer cmdBuf)
{
    cmdBuf.copyBuffer(***fragmentListBuffer, ***fragmentListBuffer,
                      vk::BufferCopy(sizeof(ui32) * 3, 0, sizeof(ui32)));
}

auto trc::DeferredRenderPassDescriptor::getProvider() -> const DescriptorProviderInterface&
{
    return *provider;
}
