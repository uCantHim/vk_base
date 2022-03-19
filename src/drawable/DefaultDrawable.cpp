#include "drawable/DefaultDrawable.h"

#include "TorchResources.h"
#include "GBufferPass.h"
#include "RenderPassShadow.h"
#include "drawable/RasterPipelines.h"
#include "GeometryRegistry.h"



namespace trc
{

void drawShadow(
    const drawcomp::RasterComponent& data,
    const DrawEnvironment& env,
    vk::CommandBuffer cmdBuf)
{
    auto currentRenderPass = dynamic_cast<RenderPassShadow*>(env.currentRenderPass);
    assert(currentRenderPass != nullptr);

    // Bind buffers and push constants
    data.geo.bindVertices(cmdBuf, 0);

    auto layout = *env.currentPipeline->getLayout();
    cmdBuf.pushConstants<mat4>(
        layout, vk::ShaderStageFlagBits::eVertex,
        0, data.modelMatrixId.get()
    );
    cmdBuf.pushConstants<ui32>(
        layout, vk::ShaderStageFlagBits::eVertex,
        sizeof(mat4), currentRenderPass->getShadowMatrixIndex()
    );
    if (data.geo.hasRig())
    {
        cmdBuf.pushConstants<AnimationDeviceData>(
            layout, vk::ShaderStageFlagBits::eVertex, sizeof(mat4) + sizeof(ui32),
            data.anim != AnimationEngine::ID::NONE ? data.anim.get() : AnimationDeviceData{}
        );
    }

    // Draw
    cmdBuf.drawIndexed(data.geo.getIndexCount(), 1, 0, 0, 0);
}

auto getDrawablePipelineFlags(const DrawableCreateInfo& info) -> PipelineFlags
{
    PipelineFlags flags;
    if (info.transparent) {
        flags |= PipelineShadingTypeFlagBits::eTransparent;
    }
    if (info.geo.get().hasRig()) {
        flags |= PipelineAnimationTypeFlagBits::eAnimated;
    }
    if (info.geo.get().getVertexType() == GeometryDeviceHandle::VertexType::eSkeletal) {
        flags |= PipelineVertexTypeFlagBits::eSkeletal;
    }

    return flags;
}

} // namespace trc



auto trc::determineDrawablePipeline(const DrawableCreateInfo& info) -> Pipeline::ID
{
    return getPipeline(getDrawablePipelineFlags(info));
}

auto trc::makeDefaultDrawableRasterization(const DrawableCreateInfo& info, Pipeline::ID pipeline)
    -> RasterComponentCreateInfo
{
    using FuncType = std::function<void(const drawcomp::RasterComponent&,
                                        const DrawEnvironment&,
                                        vk::CommandBuffer)>;

    FuncType func;
    if (info.geo.get().hasRig())
    {
        func = [](auto& data, const DrawEnvironment& env, vk::CommandBuffer cmdBuf) {
            data.geo.bindVertices(cmdBuf, 0);

            auto layout = *env.currentPipeline->getLayout();
            cmdBuf.pushConstants<mat4>(layout, vk::ShaderStageFlagBits::eVertex, 0,
                                       data.modelMatrixId.get());
            cmdBuf.pushConstants<ui32>(layout, vk::ShaderStageFlagBits::eVertex,
                                       sizeof(mat4), static_cast<ui32>(data.mat.id));
            cmdBuf.pushConstants<AnimationDeviceData>(
                layout, vk::ShaderStageFlagBits::eVertex, sizeof(mat4) + sizeof(ui32),
                data.anim.get()
            );

            cmdBuf.drawIndexed(data.geo.getIndexCount(), 1, 0, 0, 0);
        };
    }
    else
    {
        func = [](auto& data, const DrawEnvironment& env, vk::CommandBuffer cmdBuf) {
            data.geo.bindVertices(cmdBuf, 0);

            auto layout = *env.currentPipeline->getLayout();
            cmdBuf.pushConstants<mat4>(layout, vk::ShaderStageFlagBits::eVertex, 0,
                                       data.modelMatrixId.get());
            cmdBuf.pushConstants<ui32>(layout, vk::ShaderStageFlagBits::eVertex,
                                       sizeof(mat4), static_cast<ui32>(data.mat.id));
            cmdBuf.drawIndexed(data.geo.getIndexCount(), 1, 0, 0, 0);
        };
    }

    auto deferredSubpass = info.transparent ? GBufferPass::SubPasses::transparency
                                            : GBufferPass::SubPasses::gBuffer;

    RasterComponentCreateInfo result{
        .drawData={
            .geo=info.geo.getDeviceDataHandle(),
            .mat=info.mat,
            .modelMatrixId={},  // None
            .anim={},
        },
        .drawFunctions={},
    };

    result.drawFunctions.emplace_back(
        gBufferRenderStage,
        deferredSubpass,
        pipeline,
        std::move(func)
    );
    if (info.drawShadow)
    {
        result.drawFunctions.emplace_back(
            shadowRenderStage, SubPass::ID(0),
            getPipeline(getDrawablePipelineFlags(info) | PipelineShadingTypeFlagBits::eShadow),
            drawShadow
        );
    }

    return result;
}