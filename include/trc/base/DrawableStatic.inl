


template<
    typename Derived,
    RenderStage::ID::Type renderStage,
    SubPass::ID::Type subPass,
    GraphicsPipeline::ID::Type pipeline
>
trc::StaticPipelineRenderInterface<Derived, renderStage, subPass, pipeline>::StaticPipelineRenderInterface()
{
    using Self = StaticPipelineRenderInterface<Derived, renderStage, subPass, pipeline>;

    // Assert that Derived is actually a subclass of this template instantiation
    static_assert(std::is_base_of_v<Self, Derived>,
                  "Template parameter Derived must be a type derived from"
                  " StaticPipelineRenderInterface.");
    // Assert that Derived also inherits from SceneRegisterable
    static_assert(std::is_base_of_v<SceneRegisterable, Derived>,
                  "Classes derived from StaticPipelineRenderInterface must also inherit"
                  " SceneRegisterable.");

    static_cast<Derived*>(this)->usePipeline(
        renderStage,
        subPass,
        pipeline,
        [this](const DrawEnvironment& env, vk::CommandBuffer cmdBuf) {
            static_cast<Derived*>(this)->recordCommandBuffer(
                PipelineIndex<pipeline>(), env, cmdBuf
            );
        }
    );
}
