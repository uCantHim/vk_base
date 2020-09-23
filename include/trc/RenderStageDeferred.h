#pragma once

#include <vkb/FrameSpecificObject.h>
#include <vkb/Buffer.h>
#include <vkb/Image.h>

#include "RenderStage.h"
#include "RenderPass.h"

namespace trc
{
    constexpr ui32 NUM_DEFERRED_SUBPASSES = 3;

    class DeferredStage : public RenderStage
    {
    public:
        DeferredStage();
    };

    class RenderPassDeferred : public RenderPass
    {
    public:
        RenderPassDeferred();

        void begin(vk::CommandBuffer cmdBuf, vk::SubpassContents subpassContents) override;
        void end(vk::CommandBuffer cmdBuf) override;

        auto getAttachmentImageViews(ui32 imageIndex) const noexcept
           -> const std::vector<vk::UniqueImageView>&;
        auto getInputAttachmentDescriptor() const noexcept -> const DescriptorProviderInterface&;

        /**
         * @return float Depth of the pixel which contains the mouse cursor.
         */
        static auto getMouseDepthValue() -> float;

    private:
        static inline float mouseDepthValue{ 0.0f };

        void copyMouseDepthValueToBuffer(vk::CommandBuffer cmdBuf);
        void readMouseDepthValueFromBuffer();
        vkb::Buffer depthPixelReadBuffer;

        // Attachments
        std::vector<std::vector<vkb::Image>> attachmentImages;
        std::vector<std::vector<vk::UniqueImageView>> attachmentImageViews;

        vk::Extent2D framebufferSize;
        vkb::FrameSpecificObject<vk::UniqueFramebuffer> framebuffers;

        std::array<vk::ClearValue, 6> clearValues;
    };

    /**
     * @return float Depth of the pixel which contains the mouse cursor.
     *               Is zero if no depth value has been read. Is the last
     *               read depth value if the cursor is not in a window.
     */
    inline auto getMouseDepth() -> float
    {
        return RenderPassDeferred::getMouseDepthValue();
    }

    class DeferredRenderPassDescriptor
    {
    public:
        static void init(const RenderPassDeferred& deferredPass);
        static void resetValues(vk::CommandBuffer cmdBuf);

        static auto getProvider() -> const DescriptorProviderInterface&;

    private:
        static inline vk::UniqueDescriptorPool descPool;
        static inline vk::UniqueDescriptorSetLayout descLayout;
        static inline std::unique_ptr<vkb::FrameSpecificObject<vk::UniqueDescriptorSet>> descSets;
        static inline std::unique_ptr<FrameSpecificDescriptorProvider> provider;

        static inline std::unique_ptr<vkb::FrameSpecificObject<vkb::Image>>
            fragmentListHeadPointerImage;
        static inline std::unique_ptr<vkb::FrameSpecificObject<vk::UniqueImageView>>
            fragmentListHeadPointerImageView;
        static inline std::unique_ptr<vkb::FrameSpecificObject<vkb::Buffer>> fragmentListBuffer;

        static vkb::StaticInit _init;
    };
} // namespace trc
