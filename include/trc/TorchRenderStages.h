#pragma once

#include "trc/core/RenderStage.h"

namespace trc
{
    inline RenderStage resourceUpdateStage = RenderStage::make();

    inline RenderStage gBufferRenderStage = RenderStage::make();
    inline RenderStage shadowRenderStage = RenderStage::make();
    inline RenderStage mouseDepthReadStage = RenderStage::make();
    inline RenderStage finalLightingRenderStage = RenderStage::make();

    inline RenderStage rayTracingRenderStage = RenderStage::make();
} // namespace trc
