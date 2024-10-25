#include "trc/RasterTasks.h"

#include "trc/RasterSceneModule.h"
#include "trc/ShadowPool.h"
#include "trc/core/Frame.h"
#include "trc/core/ResourceConfig.h"
#include "trc/core/SceneBase.h"



namespace trc
{

RenderPassDrawTask::RenderPassDrawTask(
    RenderStage::ID renderStage,
    s_ptr<RenderPass> _renderPass)
    :
    renderStage(renderStage),
    renderPass(std::move(_renderPass))
{
    if (renderPass == nullptr) {
        throw std::invalid_argument("[In RenderPassDrawTask::RenderPassDrawTask]: Render pass"
                                    " must not be nullptr!");
    }
}

void RenderPassDrawTask::record(vk::CommandBuffer cmdBuf, ViewportDrawContext& ctx)
{
    auto& scene = ctx.scene().getModule<RasterSceneModule>();

    renderPass->begin(cmdBuf, vk::SubpassContents::eInline, ctx.frame());

    // Record all commands
    for (auto subpass : renderPass->executeSubpasses(cmdBuf, vk::SubpassContents::eInline))
    {
        for (auto pipeline : scene.iterPipelines(renderStage, subpass))
        {
            // Bind the current pipeline
            auto& p = ctx.resources().getPipeline(pipeline);
            p.bind(cmdBuf, ctx.resources());

            // Record commands for all objects with this pipeline
            const DrawEnvironment env{ .currentPipeline = &p };

            for (auto& func : scene.iterDrawFunctions(renderStage, subpass, pipeline)) {
                func(env, cmdBuf);
            }
        }
    }

    renderPass->end(cmdBuf);
}



ShadowMapDrawTask::ShadowMapDrawTask(RenderStage::ID stage, s_ptr<ShadowMap> shadowMap)
    :
    renderStage(stage),
    shadowMap(std::move(shadowMap))
{}

void ShadowMapDrawTask::record(vk::CommandBuffer cmdBuf, SceneUpdateContext& ctx)
{
    auto& scene = ctx.scene().getModule<RasterSceneModule>();
    auto& renderPass = shadowMap->getRenderPass();

    renderPass.begin(cmdBuf, vk::SubpassContents::eInline, ctx.frame());

    // Record all commands
    for (auto subpass : renderPass.executeSubpasses(cmdBuf, vk::SubpassContents::eInline))
    {
        for (auto pipeline : scene.iterPipelines(renderStage, subpass))
        {
            // Bind the current pipeline
            auto& p = ctx.resources().getPipeline(pipeline);
            p.bind(cmdBuf, ctx.resources());

            // Set renderpass-specific data
            cmdBuf.pushConstants<ui32>(*p.getLayout(), vk::ShaderStageFlagBits::eVertex,
                                       sizeof(mat4), renderPass.getShadowMatrixIndex());

            // Record commands for all objects with this pipeline
            const DrawEnvironment env{ .currentPipeline = &p };

            for (auto& func : scene.iterDrawFunctions(renderStage, subpass, pipeline)) {
                func(env, cmdBuf);
            }
        }
    }

    renderPass.end(cmdBuf);
}

} // namespace trc
