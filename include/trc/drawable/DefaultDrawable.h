#pragma once

#include "trc/AnimationEngine.h"
#include "trc/DrawablePipelines.h"
#include "trc/Transformation.h"
#include "trc/assets/GeometryRegistry.h"
#include "trc/assets/MaterialRegistry.h"
#include "trc/RasterSceneBase.h"

namespace trc
{
    struct DrawablePipelineInfo
    {
        bool animated;
        bool transparent;

        auto toPipelineFlags() const -> pipelines::DrawableBasePipelineTypeFlags;
        auto determineShadowPipeline() const -> Pipeline::ID;
    };

    struct DrawableRasterDrawInfo
    {
        GeometryHandle geo;
        MaterialHandle mat;
        MaterialRuntime matRuntime;

        Transformation::ID modelMatrixId;
        AnimationEngine::ID anim;
    };

    auto makeGBufferDrawFunction(s_ptr<DrawableRasterDrawInfo> drawInfo) -> DrawableFunction;
    auto makeShadowDrawFunction(s_ptr<DrawableRasterDrawInfo> drawInfo) -> DrawableFunction;
} // namespace trc
