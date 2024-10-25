#pragma once

#include "trc/Types.h"
#include "trc/core/RenderPipelineTasks.h"
#include "trc/core/RenderStage.h"

namespace trc
{
    class RasterSceneModule;
    class RenderPass;
    class ShadowMap;

    /**
     * @brief A task that draws classical drawables registered at a
     *        RasterSceneBase for a specific render stage within a specific
     *        instance of a render pass
     */
    class RenderPassDrawTask : public ViewportDrawTask
    {
    public:
        RenderPassDrawTask(RenderStage::ID renderStage,
                           s_ptr<RenderPass> renderPass);

        void record(vk::CommandBuffer cmdBuf, ViewportDrawContext& env) override;

    private:
        RenderStage::ID renderStage;
        s_ptr<RenderPass> renderPass;
    };

    /**
     * Always uploads the shadow matrix index as a push constant to the byte
     * range `[64, 68)` with `vk::ShaderStageFlagBits::eVertex`.
     */
    class ShadowMapDrawTask : public SceneUpdateTask
    {
    public:
        ShadowMapDrawTask(RenderStage::ID renderStage,
                          s_ptr<ShadowMap> shadowMap);

        void record(vk::CommandBuffer cmdBuf, SceneUpdateContext& ctx) override;

    private:
        RenderStage::ID renderStage;
        s_ptr<ShadowMap> shadowMap;
    };
} // namespace trc
