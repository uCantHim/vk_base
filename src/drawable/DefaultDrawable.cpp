#include "trc/drawable/DefaultDrawable.h"

#include "trc/DrawablePipelines.h"
#include "trc/material/VertexShader.h" // For the DrawablePushConstIndex enum



namespace trc
{

auto DrawablePipelineInfo::toPipelineFlags() const -> pipelines::DrawableBasePipelineTypeFlags
{
    pipelines::DrawableBasePipelineTypeFlags flags;
    assert(flags.get<pipelines::AnimationTypeFlagBits>() == pipelines::AnimationTypeFlagBits::none);
    assert(flags.get<pipelines::PipelineShadingTypeFlagBits>() == pipelines::PipelineShadingTypeFlagBits::opaque);

    if (transparent) {
        flags |= pipelines::PipelineShadingTypeFlagBits::transparent;
    }
    if (animated) {
        flags |= pipelines::AnimationTypeFlagBits::boneAnim;
    }

    return flags;
}

auto DrawablePipelineInfo::determineShadowPipeline() const -> Pipeline::ID
{
    return pipelines::getDrawableShadowPipeline(toPipelineFlags());
}

auto makeGBufferDrawFunction(s_ptr<DrawableRasterDrawInfo> drawInfo) -> DrawableFunction
{
    return [drawInfo](const DrawEnvironment& env, vk::CommandBuffer cmdBuf)
    {
        auto layout = *env.currentPipeline->getLayout();
        auto& material = drawInfo->matRuntime;
        material.pushConstants(cmdBuf, layout, DrawablePushConstIndex::eModelMatrix,
                               drawInfo->modelMatrixId.get());

        const bool animated = drawInfo->anim != AnimationEngine::ID::NONE;
        if (animated)
        {
            material.pushConstants(
                cmdBuf, layout, DrawablePushConstIndex::eAnimationData,
                drawInfo->anim.get()
            );
        }
        material.uploadPushConstantDefaultValues(cmdBuf, layout);

        drawInfo->geo.bindVertices(cmdBuf, 0);
        cmdBuf.drawIndexed(drawInfo->geo.getIndexCount(), 1, 0, 0, 0);
    };
}

auto makeShadowDrawFunction(s_ptr<DrawableRasterDrawInfo> drawInfo) -> DrawableFunction
{
    return [drawInfo](const DrawEnvironment& env, vk::CommandBuffer cmdBuf)
    {
        auto layout = *env.currentPipeline->getLayout();

        // Bind buffers and upload data
        drawInfo->geo.bindVertices(cmdBuf, 0);

        cmdBuf.pushConstants<mat4>(layout, vk::ShaderStageFlagBits::eVertex,
                                   0, drawInfo->modelMatrixId.get());
        if (drawInfo->anim != AnimationEngine::ID::NONE)
        {
            cmdBuf.pushConstants<AnimationDeviceData>(
                layout, vk::ShaderStageFlagBits::eVertex, sizeof(mat4) + sizeof(ui32),
                drawInfo->anim.get()
            );
        }

        // Draw
        cmdBuf.drawIndexed(drawInfo->geo.getIndexCount(), 1, 0, 0, 0);
    };
}

} // namespace trc
