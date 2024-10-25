#include "trc/core/RenderPass.h"



trc::RenderPass::RenderPass(vk::UniqueRenderPass renderPass, ui32 subpassCount)
    :
    renderPass(std::move(renderPass)),
    numSubpasses(subpassCount)
{
}

auto trc::RenderPass::operator*() const noexcept -> vk::RenderPass
{
    return *renderPass;
}

auto trc::RenderPass::get() const noexcept -> vk::RenderPass
{
    return *renderPass;
}

auto trc::RenderPass::getNumSubPasses() const noexcept -> ui32
{
    return numSubpasses;
}

auto trc::RenderPass::executeSubpasses(
    vk::CommandBuffer cmdBuf,
    vk::SubpassContents subpassContents) const
    -> std::generator<SubPass::ID>
{
    const ui32 subPassCount = getNumSubPasses();
    for (ui32 subPass = 0; subPass < subPassCount; subPass++)
    {
        co_yield SubPass::ID{ subPass };

        if (subPass < subPassCount - 1) {
            cmdBuf.nextSubpass(subpassContents);
        }
    }
}
