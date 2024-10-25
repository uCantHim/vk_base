#pragma once

#include <generator>

#include "trc/Types.h"
#include "trc/VulkanInclude.h"

namespace trc
{
    class FrameRenderState;
    class Swapchain;

    struct SubPass
    {
        using ID = TypesafeID<SubPass>;
    };

    /**
     * @brief A renderpass
     *
     * Contains subpasses.
     */
    class RenderPass
    {
    public:
        /**
         * @brief Constructor
         */
        RenderPass(vk::UniqueRenderPass renderPass, ui32 subpassCount);

        RenderPass(RenderPass&&) noexcept = default;
        virtual ~RenderPass() = default;
        auto operator=(RenderPass&&) noexcept -> RenderPass& = default;

        auto operator*() const noexcept -> vk::RenderPass;
        auto get() const noexcept -> vk::RenderPass;

        auto getNumSubPasses() const noexcept -> ui32;

        /**
         * Iterate over the render pass's subpasses in order, issuing
         * `nextSubpass` commands after each one.
         */
        auto executeSubpasses(vk::CommandBuffer cmdBuf,
                              vk::SubpassContents cnt = vk::SubpassContents::eInline) const
            -> std::generator<SubPass::ID>;

        virtual void begin(vk::CommandBuffer cmdBuf,
                           vk::SubpassContents subpassContents,
                           FrameRenderState& frameState) = 0;
        virtual void end(vk::CommandBuffer cmdBuf) = 0;

    protected:
        vk::UniqueRenderPass renderPass;
        ui32 numSubpasses;
    };
} // namespace trc
