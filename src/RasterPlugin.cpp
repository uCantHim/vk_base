#include "trc/RasterPlugin.h"

#include "trc/GBufferDepthReader.h"
#include "trc/GBufferPass.h"
#include "trc/LightSceneModule.h"
#include "trc/RasterTasks.h"
#include "trc/TorchRenderStages.h"
#include "trc/core/Frame.h"
#include "trc/core/RenderGraph.h"
#include "trc/core/ResourceConfig.h"
#include "trc/core/SceneBase.h"
#include "trc/core/Task.h"



namespace trc
{

RasterPlugin::RasterPlugin(
    const Device& device,
    ui32 maxViewports,
    const RasterPluginCreateInfo& createInfo)
    :
    gBufferDescriptor(device, maxViewports),
    globalDataDescriptor(device, maxViewports),
    sceneDescriptor(std::make_shared<SceneDescriptor>(device)),
    shadowDescriptor(createInfo.shadowDescriptor),
    finalLighting(device, maxViewports)
{
    compatibleGBufferRenderPass = GBufferPass::makeVkRenderPass(device);
    compatibleShadowRenderPass = RenderPassShadow::makeVkRenderPass(device);
}

void RasterPlugin::registerRenderStages(RenderGraph& graph)
{
    graph.insert(shadowRenderStage);
    graph.insert(gBufferRenderStage);
    graph.insert(finalLightingRenderStage);

    graph.createOrdering(resourceUpdateStage, shadowRenderStage);
    graph.createOrdering(shadowRenderStage, gBufferRenderStage);
    graph.createOrdering(gBufferRenderStage, finalLightingRenderStage);

    graph.createOrdering(renderTargetImageInitStage, finalLightingRenderStage);
    graph.createOrdering(finalLightingRenderStage, renderTargetImageFinalizeStage);
}

void RasterPlugin::defineResources(ResourceConfig& config)
{
    config.defineDescriptor(DescriptorName{ GLOBAL_DATA_DESCRIPTOR },
                            globalDataDescriptor.getDescriptorSetLayout());
    config.defineDescriptor(DescriptorName{ G_BUFFER_DESCRIPTOR },
                            gBufferDescriptor.getDescriptorSetLayout());
    config.defineDescriptor(DescriptorName{ SCENE_DESCRIPTOR },
                            sceneDescriptor->getDescriptorSetLayout());
    config.defineDescriptor(DescriptorName{ SHADOW_DESCRIPTOR },
                            shadowDescriptor->getDescriptorSetLayout());
    config.defineDescriptor(DescriptorName{ FinalLighting::OUTPUT_IMAGE_DESCRIPTOR },
                            finalLighting.getDescriptorSetLayout());

    config.addRenderPass(
        RenderPassName{ OPAQUE_G_BUFFER_PASS },
        [&]{ return RenderPassInfo{ *compatibleGBufferRenderPass, 0 }; }
    );
    config.addRenderPass(
        RenderPassName{ TRANSPARENT_G_BUFFER_PASS },
        [&]{ return RenderPassInfo{ *compatibleGBufferRenderPass, 1 }; }
    );
    config.addRenderPass(
        RenderPassName{ SHADOW_PASS },
        [&]{ return RenderPassInfo{ *compatibleShadowRenderPass, 0 }; }
    );
}

auto RasterPlugin::createDrawConfig(const Device& device, Viewport renderTarget)
    -> u_ptr<DrawConfig>
{
    return std::make_unique<RasterDrawConfig>(device, renderTarget, *this);
}



////////////////////////////////////////
//    Per-image draw configuration    //
////////////////////////////////////////

RasterPlugin::RasterDrawConfig::RasterDrawConfig(
    const Device& device,
    Viewport renderTarget,
    RasterPlugin& parent)
    :
    parent(&parent),
    gBuffer(device, { .size=renderTarget.size, .maxTransparentFragsPerPixel=2 }),
    gBufferPass(std::make_shared<GBufferPass>(device, gBuffer)),
    gBufferDepthReaderPass(
        std::make_shared<GBufferDepthReader>(device, []{ return vec2{}; }, gBuffer)
    ),
    finalLighting(parent.finalLighting.makeDrawConfig(device, renderTarget)),
    gBufferDescSet(parent.gBufferDescriptor.makeDescriptorSet(device, gBuffer)),
    globalDataDescriptor(std::make_shared<GlobalRenderDataDescriptor::DescriptorSet>(
        parent.globalDataDescriptor.makeDescriptorSet()
    ))
{
}

void RasterPlugin::RasterDrawConfig::registerResources(ResourceStorage& resources)
{
    resources.provideDescriptor(DescriptorName{ GLOBAL_DATA_DESCRIPTOR },
                                globalDataDescriptor);
    resources.provideDescriptor(DescriptorName{ G_BUFFER_DESCRIPTOR },
                                std::make_shared<DescriptorProvider>(*gBufferDescSet));
    resources.provideDescriptor(DescriptorName{ SCENE_DESCRIPTOR },
                                parent->sceneDescriptor);
    resources.provideDescriptor(DescriptorName{ SHADOW_DESCRIPTOR },
                                parent->shadowDescriptor);
}

void RasterPlugin::RasterDrawConfig::update(
    const Device& /*device*/,
    SceneBase& scene,
    const Camera& camera)
{
    parent->shadowDescriptor->update();
    parent->sceneDescriptor->update(scene);
    globalDataDescriptor->update(camera);
}

void RasterPlugin::RasterDrawConfig::createTasks(SceneBase& scene, TaskQueue& taskQueue)
{
    // Shadow tasks - one for each shadow map
    auto& lights = scene.getModule<LightSceneModule>();
    for (auto& renderPass : lights.getShadowPasses()) {
        taskQueue.spawnTask(shadowRenderStage, std::make_unique<RenderPassDrawTask>(shadowRenderStage, renderPass));
    }

    // G-buffer draw task
    taskQueue.spawnTask(gBufferRenderStage,
                        std::make_unique<RenderPassDrawTask>(gBufferRenderStage, gBufferPass));
    taskQueue.spawnTask(gBufferRenderStage,
        makeTask([this](vk::CommandBuffer cmdBuf, TaskEnvironment& env) {
            gBufferDepthReaderPass->update(cmdBuf, *env.frame);
        })
    );

    // Final lighting compute task
    finalLighting->createTasks(taskQueue);
}

} // namespace trc
